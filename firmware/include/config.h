#pragma once

// ─── Display ──────────────────────────────────────────────
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C
#define SDA_PIN         5
#define SCL_PIN         6

// ─── WiFi ─────────────────────────────────────────────────
#define WIFI_SSID              "Princess Sissy"      // ← your network name
#define WIFI_PASSWORD          "XialovePeter44"  // ← your password

//#define WIFI_SSID              "Rainy"      // ← your network name
//#define WIFI_PASSWORD          "@Gehlenberg2"  // ← your password
#define WIFI_STATIC_IP               "10.0.0.45"
#define WIFI_GATEWAY              "10.0.0.1"
#define WIFI_SUBNET              "255.255.255.0"
#define WIFI_MAC  {0xE8, 0x3D, 0xC1, 0x8D, 0xCA, 0x90}





#define WIFI_CONNECT_TIMEOUT_MS  30000         // give up after 15 s → BLE-only
#define WEB_SERVER_PORT        80

// ─── BLE ──────────────────────────────────────────────────
#define BLE_DEVICE_NAME     "WateringSystem"
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789000"
#define STATUS_UUID         "12345678-1234-1234-1234-123456789001"
#define SCHEDULE_UUID       "12345678-1234-1234-1234-123456789002"
#define RAIN_UUID           "12345678-1234-1234-1234-123456789003"
#define MANUAL_UUID         "12345678-1234-1234-1234-123456789004"
#define TIME_UUID           "12345678-1234-1234-1234-123456789005"

// ─── Valves ───────────────────────────────────────────────
#define VALVE1_PIN      2
#define VALVE2_PIN      3

// ─── Rain sensor ──────────────────────────────────────────
#define RAIN_PIN_DIGITAL    1       // GPIO1 — active LOW (LOW = raining)
#define RAIN_PIN_ANALOG     0       // GPIO0 — ADC1_0, lower value = wetter

// Analog thresholds (12-bit ADC, 0–4095). Tune after physical testing.
#define RAIN_ANALOG_DRY_THRESHOLD   3500
#define RAIN_ANALOG_WET_THRESHOLD   1500
