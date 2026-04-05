#pragma once
#include <Arduino.h>

// ── Day-of-week bitmask ───────────────────────────────────────────────────────
// Matches the day abbreviations used in the BLE SCHEDULE protocol:
//   SCHED:V1,Mon+Wed+Fri,07:00,20
#define DAY_MON  0x01
#define DAY_TUE  0x02
#define DAY_WED  0x04
#define DAY_THU  0x08
#define DAY_FRI  0x10
#define DAY_SAT  0x20
#define DAY_SUN  0x40

// Maximum number of schedule entries (3 per zone × 2 zones)
#define MAX_SCHEDULE_ENTRIES 6

// ── Schedule entry ────────────────────────────────────────────────────────────
struct ScheduleEntry {
    uint8_t  valve;         // 1 or 2
    uint8_t  dayMask;       // bitmask of DAY_xxx flags
    uint8_t  hour;          // 0–23
    uint8_t  minute;        // 0–59
    uint8_t  durationMin;   // 1–120 minutes
    bool     active;        // true = slot in use
};

// ── Module API ────────────────────────────────────────────────────────────────
void initScheduler();

// Called from ble_server.cpp when SCHEDULE characteristic is written
// Returns true if the line was valid, false on parse error
bool processScheduleLine(const String& line);

// Called from main loop every minute — checks RTC time and fires valves
// Pass in current day (0=Mon … 6=Sun), hour and minute from DS3231
void checkSchedule(uint8_t weekday, uint8_t hour, uint8_t minute);

// Returns a human-readable summary of the current schedule (for serial debug)
void printSchedule();

// How many entries are currently loaded
uint8_t getScheduleCount();

// Last parse/save result — sent back via STATUS characteristic
// Returns "SCH:OK" or "SCH:ERR"
const char* getScheduleAck();
