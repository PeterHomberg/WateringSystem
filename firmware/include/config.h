#pragma once

// ─── Display ─────────────────────────────────────────────
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C
#define SDA_PIN         5
#define SCL_PIN         6

// ─── BLE ─────────────────────────────────────────────────
#define BLE_DEVICE_NAME     "WateringSystem"
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789000"
#define STATUS_UUID         "12345678-1234-1234-1234-123456789001"
#define SCHEDULE_UUID       "12345678-1234-1234-1234-123456789002"
#define RAIN_UUID           "12345678-1234-1234-1234-123456789003"
#define MANUAL_UUID         "12345678-1234-1234-1234-123456789004"

// ─── Valves ───────────────────────────────────────────────
#define VALVE1_PIN      0
#define VALVE2_PIN      1

// ─── Rain sensor ─────────────────────────────────────────
#define RAIN_PIN        2