#include "scheduler.h"
#include "valves/valves.h"
#include <Preferences.h>   // ESP32 flash storage (built-in, no lib needed)

// ── Internal state ────────────────────────────────────────────────────────────
static ScheduleEntry schedule[MAX_SCHEDULE_ENTRIES];
static uint8_t       entryCount  = 0;
static bool          transferOpen = false;   // true between BEGIN and END
static bool          parseOk      = true;    // tracks errors during transfer
static char          scheduleAck[12] = "SCH:OK";

// Tracks which valves were opened by the scheduler so we can auto-close them
static unsigned long valveOpenedAt[3]   = {0, 0, 0};  // index 1 and 2 used
static uint8_t       valveDuration[3]   = {0, 0, 0};  // minutes

// NVS namespace for flash persistence
static const char* NVS_NAMESPACE = "watering";
static const char* NVS_KEY       = "schedule";

// ── Forward declarations ──────────────────────────────────────────────────────
static bool    parseLine(const String& line);
static uint8_t parseDayMask(const String& days);
static void    saveToFlash();
static void    loadFromFlash();

// ── Public API ────────────────────────────────────────────────────────────────

void initScheduler() {
    memset(schedule, 0, sizeof(schedule));
    entryCount = 0;
    loadFromFlash();
    Serial.printf("Scheduler initialized — %d entries loaded from flash\n", entryCount);
}

bool processScheduleLine(const String& line) {
    Serial.printf("Scheduler rx: %s\n", line.c_str());

    if (line == "SCHED:BEGIN") {
        // Clear in-memory schedule and start accumulating new entries
        memset(schedule, 0, sizeof(schedule));
        entryCount   = 0;
        transferOpen = true;
        parseOk      = true;
        Serial.println("Scheduler: transfer started");
        return true;
    }

    if (line == "SCHED:END") {
        if (!transferOpen) {
            Serial.println("Scheduler: END without BEGIN — ignored");
            strncpy(scheduleAck, "SCH:ERR", sizeof(scheduleAck));
            return false;
        }
        transferOpen = false;
        if (parseOk) {
            saveToFlash();
            strncpy(scheduleAck, "SCH:OK", sizeof(scheduleAck));
            Serial.printf("Scheduler: transfer complete — %d entries saved\n", entryCount);
        } else {
            strncpy(scheduleAck, "SCH:ERR", sizeof(scheduleAck));
            Serial.println("Scheduler: transfer ended with errors — not saved");
        }
        printSchedule();
        return parseOk;
    }

    if (line.startsWith("SCHED:")) {
        if (!transferOpen) {
            Serial.println("Scheduler: entry received outside BEGIN/END — ignored");
            return false;
        }
        bool ok = parseLine(line);
        if (!ok) parseOk = false;
        return ok;
    }

    // Not a scheduler line
    return false;
}

void checkSchedule(uint8_t weekday, uint8_t hour, uint8_t minute) {
    // weekday: 0=Mon, 1=Tue, ... 6=Sun  (matches DS3231 convention after mapping)
    uint8_t dayBit = (1 << weekday);

    for (uint8_t i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
        if (!schedule[i].active) continue;

        ScheduleEntry& e = schedule[i];

        // Check if today is a scheduled day and it's the right time
        if ((e.dayMask & dayBit) &&
            (e.hour   == hour)   &&
            (e.minute == minute))
        {
            // Check rain inhibit — if raining, skip watering
            // (rain.h is included transitively via valves — import it directly
            //  if needed; for now we call openValve and let main handle inhibit)
            Serial.printf("Scheduler: firing V%d for %d min\n", e.valve, e.durationMin);
            openValve(e.valve);
            valveOpenedAt[e.valve] = millis();
            valveDuration[e.valve] = e.durationMin;
        }
    }

    // Auto-close valves when their duration has elapsed
    for (uint8_t v = 1; v <= 2; v++) {
        if (valveOpenedAt[v] > 0 && isValveOpen(v)) {
            unsigned long elapsed = (millis() - valveOpenedAt[v]) / 60000UL;
            if (elapsed >= valveDuration[v]) {
                Serial.printf("Scheduler: closing V%d after %d min\n", v, valveDuration[v]);
                closeValve(v);
                valveOpenedAt[v] = 0;
            }
        }
    }
}

void printSchedule() {
    Serial.printf("── Schedule (%d entries) ──────────────\n", entryCount);
    const char* dayNames[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
    for (uint8_t i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
        if (!schedule[i].active) continue;
        ScheduleEntry& e = schedule[i];
        Serial.printf("  [%d] V%d  %02d:%02d  %dmin  days:",
            i, e.valve, e.hour, e.minute, e.durationMin);
        for (uint8_t d = 0; d < 7; d++) {
            if (e.dayMask & (1 << d)) Serial.printf(" %s", dayNames[d]);
        }
        Serial.println();
    }
    Serial.println("──────────────────────────────────────");
}

uint8_t getScheduleCount() {
    return entryCount;
}

const char* getScheduleAck() {
    return scheduleAck;
}

// ── Internal helpers ──────────────────────────────────────────────────────────

// Parse one entry line: SCHED:V1,Mon+Wed+Fri,07:00,20
static bool parseLine(const String& line) {
    // Strip "SCHED:" prefix
    String body = line.substring(6);   // "V1,Mon+Wed+Fri,07:00,20"

    // Split by comma — expect exactly 4 tokens
    int c1 = body.indexOf(',');
    if (c1 < 0) { Serial.println("Scheduler parse error: no comma 1"); return false; }
    int c2 = body.indexOf(',', c1 + 1);
    if (c2 < 0) { Serial.println("Scheduler parse error: no comma 2"); return false; }
    int c3 = body.indexOf(',', c2 + 1);
    if (c3 < 0) { Serial.println("Scheduler parse error: no comma 3"); return false; }

    String zoneStr    = body.substring(0, c1);            // "V1"
    String daysStr    = body.substring(c1 + 1, c2);       // "Mon+Wed+Fri"
    String timeStr    = body.substring(c2 + 1, c3);       // "07:00"
    String durStr     = body.substring(c3 + 1);           // "20"

    // Validate zone
    uint8_t valve = 0;
    if      (zoneStr == "V1") valve = 1;
    else if (zoneStr == "V2") valve = 2;
    else {
        Serial.printf("Scheduler parse error: unknown zone '%s'\n", zoneStr.c_str());
        return false;
    }

    // Parse day bitmask
    uint8_t dayMask = parseDayMask(daysStr);
    if (dayMask == 0) {
        Serial.printf("Scheduler parse error: no valid days in '%s'\n", daysStr.c_str());
        return false;
    }

    // Parse time HH:MM
    int colon = timeStr.indexOf(':');
    if (colon < 0) {
        Serial.printf("Scheduler parse error: bad time '%s'\n", timeStr.c_str());
        return false;
    }
    uint8_t hour   = (uint8_t)timeStr.substring(0, colon).toInt();
    uint8_t minute = (uint8_t)timeStr.substring(colon + 1).toInt();
    if (hour > 23 || minute > 59) {
        Serial.printf("Scheduler parse error: time out of range %02d:%02d\n", hour, minute);
        return false;
    }

    // Parse duration
    uint8_t dur = (uint8_t)durStr.toInt();
    if (dur < 1 || dur > 120) {
        Serial.printf("Scheduler parse error: duration out of range %d\n", dur);
        return false;
    }

    // Store entry
    if (entryCount >= MAX_SCHEDULE_ENTRIES) {
        Serial.println("Scheduler parse error: too many entries (max 6)");
        return false;
    }
    schedule[entryCount] = { valve, dayMask, hour, minute, dur, true };
    entryCount++;

    Serial.printf("Scheduler: added V%d  %02d:%02d  %dmin  mask=0x%02X\n",
        valve, hour, minute, dur, dayMask);
    return true;
}

// Parse "Mon+Wed+Fri" into a bitmask
static uint8_t parseDayMask(const String& days) {
    uint8_t mask = 0;
    // Walk through the string splitting on '+'
    int start = 0;
    while (start < (int)days.length()) {
        int plus = days.indexOf('+', start);
        String token = (plus < 0)
            ? days.substring(start)
            : days.substring(start, plus);
        token.trim();
        if      (token == "Mon") mask |= DAY_MON;
        else if (token == "Tue") mask |= DAY_TUE;
        else if (token == "Wed") mask |= DAY_WED;
        else if (token == "Thu") mask |= DAY_THU;
        else if (token == "Fri") mask |= DAY_FRI;
        else if (token == "Sat") mask |= DAY_SAT;
        else if (token == "Sun") mask |= DAY_SUN;
        else Serial.printf("Scheduler: unknown day token '%s'\n", token.c_str());
        if (plus < 0) break;
        start = plus + 1;
    }
    return mask;
}

// ── Flash persistence (ESP32 Preferences / NVS) ───────────────────────────────
// We store the raw schedule array as a blob.  Preferences handles wear-levelling.

static void saveToFlash() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);   // false = read/write
    prefs.putBytes(NVS_KEY, schedule, sizeof(schedule));
    prefs.putUChar("count", entryCount);
    prefs.end();
    Serial.println("Scheduler: saved to flash");
}

static void loadFromFlash() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);    // true = read-only
    size_t len = prefs.getBytesLength(NVS_KEY);
    if (len == sizeof(schedule)) {
        prefs.getBytes(NVS_KEY, schedule, sizeof(schedule));
        entryCount = prefs.getUChar("count", 0);
    } else {
        // Nothing saved yet or size mismatch after a firmware update
        memset(schedule, 0, sizeof(schedule));
        entryCount = 0;
    }
    prefs.end();
}
