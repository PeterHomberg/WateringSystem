#include "rtc.h"
#include <RTClib.h>   // adafruit/RTClib

// ── Internal state ────────────────────────────────────────────────────────────
static RTC_DS3231 rtc;
static bool       rtcReady = false;

// RTClib dayOfTheWeek():  0=Sun, 1=Mon … 6=Sat
// Firmware convention:    0=Mon, 1=Tue … 6=Sun
// Convert RTClib → firmware:  (rtcDow + 6) % 7
static inline uint8_t toFirmwareWeekday(uint8_t rtcDow) {
    return (rtcDow + 6) % 7;
}

// Convert firmware → RTClib:  (fwDow + 1) % 7
static inline uint8_t toRTClibWeekday(uint8_t fwDow) {
    return (fwDow + 1) % 7;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool initRTC() {
    if (!rtc.begin()) {
        Serial.println("RTC: DS3231 not found on I2C bus");
        rtcReady = false;
        return false;
    }

    // If the RTC lost power and the oscillator stopped, its time is invalid.
    // We flag it but do NOT auto-set to compile time — the iOS app will push
    // the correct time via the TIME BLE characteristic.
    if (rtc.lostPower()) {
        Serial.println("RTC: oscillator stopped — time is invalid until set via BLE");
        // Clear the OSF (Oscillator Stop Flag) so lostPower() won't keep
        // returning true after the iOS app sets a valid time.
        rtc.adjust(DateTime(2000, 1, 1, 0, 0, 0));
    }

    rtcReady = true;
    Serial.println("RTC: DS3231 ready");

    // Print current time to serial for debugging
    DateTime now = rtc.now();
    Serial.printf("RTC: current time %04d-%02d-%02d %02d:%02d:%02d\n",
        now.year(), now.month(), now.day(),
        now.hour(), now.minute(), now.second());

    return true;
}

bool isRTCReady() {
    return rtcReady;
}

bool getRTCTime(RtcTime& out) {
    if (!rtcReady) return false;

    DateTime now = rtc.now();

    // Sanity-check: if year < 2024 the clock has never been set properly
    if (now.year() < 2024) {
        return false;
    }

    out.year    = now.year();
    out.month   = now.month();
    out.day     = now.day();
    out.hour    = now.hour();
    out.minute  = now.minute();
    out.second  = now.second();
    out.weekday = toFirmwareWeekday(now.dayOfTheWeek());
    return true;
}

bool setRTCTime(uint16_t year, uint8_t month, uint8_t day,
                uint8_t hour, uint8_t minute, uint8_t second,
                uint8_t weekday) {
    if (!rtcReady) return false;

    // Validate ranges
    if (year < 2024 || year > 2099) return false;
    if (month < 1   || month > 12)  return false;
    if (day   < 1   || day   > 31)  return false;
    if (hour  > 23)                  return false;
    if (minute > 59)                 return false;
    if (second > 59)                 return false;
    if (weekday > 6)                 return false;

    // RTClib DateTime constructor uses its own dayOfTheWeek field only
    // for display; the real weekday is derived from the date.
    // We still set it correctly via adjust().
    DateTime dt(year, month, day, hour, minute, second);
    rtc.adjust(dt);

    Serial.printf("RTC: time set to %04d-%02d-%02d %02d:%02d:%02d (weekday fw=%d)\n",
        year, month, day, hour, minute, second, weekday);
    return true;
}

bool setRTCTimeFromString(const String& s) {
    // Expected format: "TIME:2025-04-07T14:30:00W1"
    //                   0123456789...
    if (!s.startsWith("TIME:")) {
        Serial.println("RTC: setRTCTimeFromString — missing TIME: prefix");
        return false;
    }

    // Body after "TIME:" → "2025-04-07T14:30:00W1"
    String body = s.substring(5);

    // Find the 'W' separator for weekday
    int wIdx = body.indexOf('W');
    if (wIdx < 0) {
        Serial.println("RTC: setRTCTimeFromString — missing W field");
        return false;
    }

    // Parse weekday (single digit 0–6)
    uint8_t weekday = (uint8_t)body.substring(wIdx + 1).toInt();
    if (weekday > 6) {
        Serial.printf("RTC: setRTCTimeFromString — weekday %d out of range\n", weekday);
        return false;
    }

    // datetime part: "2025-04-07T14:30:00"
    String dt = body.substring(0, wIdx);
    if (dt.length() < 19) {
        Serial.printf("RTC: setRTCTimeFromString — datetime too short: '%s'\n", dt.c_str());
        return false;
    }

    // Parse: "2025-04-07T14:30:00"
    //         0123456789012345678
    uint16_t year   = (uint16_t)dt.substring(0, 4).toInt();
    uint8_t  month  = (uint8_t) dt.substring(5, 7).toInt();
    uint8_t  day    = (uint8_t) dt.substring(8, 10).toInt();
    uint8_t  hour   = (uint8_t) dt.substring(11, 13).toInt();
    uint8_t  minute = (uint8_t) dt.substring(14, 16).toInt();
    uint8_t  second = (uint8_t) dt.substring(17, 19).toInt();

    return setRTCTime(year, month, day, hour, minute, second, weekday);
}

void getRTCTimeString(char* buf, size_t len) {
    if (!rtcReady) {
        snprintf(buf, len, "--:--");
        return;
    }
    RtcTime t;
    if (!getRTCTime(t)) {
        snprintf(buf, len, "--:--");
        return;
    }
    snprintf(buf, len, "%02d:%02d", t.hour, t.minute);
}

void getRTCDateString(char* buf, size_t len) {
    static const char* dayNames[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};

    if (!rtcReady) {
        snprintf(buf, len, "--- --/--");
        return;
    }
    RtcTime t;
    if (!getRTCTime(t)) {
        snprintf(buf, len, "--- --/--");
        return;
    }
    snprintf(buf, len, "%s %02d/%02d",
        dayNames[t.weekday % 7], t.day, t.month);
}
