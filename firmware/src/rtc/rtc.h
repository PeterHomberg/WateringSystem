#pragma once
#include <Arduino.h>

// ── DS3231 RTC module ─────────────────────────────────────────────────────────
// Wraps RTClib to provide a clean API for the rest of the firmware.
//
// Weekday convention used throughout this firmware:
//   0 = Monday … 6 = Sunday
// RTClib's DateTime::dayOfTheWeek() returns 0=Sunday … 6=Saturday.
// The conversion is handled inside this module — callers always get 0=Mon.

struct RtcTime {
    uint16_t year;
    uint8_t  month;     // 1–12
    uint8_t  day;       // 1–31
    uint8_t  hour;      // 0–23
    uint8_t  minute;    // 0–59
    uint8_t  second;    // 0–59
    uint8_t  weekday;   // 0=Mon … 6=Sun (firmware convention)
};

// Call once in setup() — returns true if DS3231 was found on the I2C bus
bool initRTC();

// Returns true if the DS3231 was detected and is running
bool isRTCReady();

// Read current time — call isRTCReady() first
// Returns false if the RTC is not available or the read fails
bool getRTCTime(RtcTime& out);

// Set the DS3231 time from individual fields.
// weekday follows the firmware convention (0=Mon … 6=Sun).
// Returns false if the RTC is not available.
bool setRTCTime(uint16_t year, uint8_t month, uint8_t day,
                uint8_t hour, uint8_t minute, uint8_t second,
                uint8_t weekday);

// Set time by parsing a compact string written via BLE:
//   "TIME:2025-04-07T14:30:00W1"
//   where W<n> is weekday 0=Mon … 6=Sun
// Returns false if the string is malformed or the RTC is not available.
bool setRTCTimeFromString(const String& s);

// Human-readable time string for display: "14:30" (HH:MM)
// Writes into buf (must be at least 6 bytes).
void getRTCTimeString(char* buf, size_t len);

// Human-readable date string: "Mon 07/04" (day DD/MM)
// Writes into buf (must be at least 12 bytes).
void getRTCDateString(char* buf, size_t len);
