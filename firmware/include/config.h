#pragma once

// ─── Display ──────────────────────────────────────────────
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C
#define SDA_PIN         5
#define SCL_PIN         6

// ─── BLE ──────────────────────────────────────────────────
#define BLE_DEVICE_NAME     "WateringSystem"
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789000"
#define STATUS_UUID         "12345678-1234-1234-1234-123456789001"
#define SCHEDULE_UUID       "12345678-1234-1234-1234-123456789002"
#define RAIN_UUID           "12345678-1234-1234-1234-123456789003"
#define MANUAL_UUID         "12345678-1234-1234-1234-123456789004"
#define TIME_UUID           "12345678-1234-1234-1234-123456789005"

// ─── Valves ───────────────────────────────────────────────
#define VALVE1_PIN      3
#define VALVE2_PIN      4

// ─── Rain sensor ──────────────────────────────────────────
#define RAIN_PIN_DIGITAL    1       // GPIO1  — active LOW (LOW = raining)
#define RAIN_PIN_ANALOG     0       // GPIO0  — ADC1_0, lower value = wetter

// Analog thresholds (12-bit ADC, 0–4095)
// Sensor output is high (~4095) when dry, falls as surface gets wet.
// Tune these after physical testing with your specific sensor module.
#define RAIN_ANALOG_DRY_THRESHOLD   3500    // above this → dry
#define RAIN_ANALOG_WET_THRESHOLD   1500    // below this → heavy rain
