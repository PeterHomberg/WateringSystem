# Watering System — Project Context for Claude

## Project Overview
An automated garden watering controller consisting of:
- **ESP32-S3-N16R8 devKitC-1 firmware** (C++/Arduino, developed on Windows 11 with VSCode + PlatformIO)
- **iOS app** (Swift/CoreBluetooth, developed on Mac with Xcode)

The two components communicate via **Bluetooth LE (BLE)** and a **Web Server** (HTTP over WiFi).
There is a local CPE WiFi system with a router of type TP-Link CPE210 available.
The ESP module exposes a web server so that an external device can control, set up, and read the status of the watering system.

> **Hardware note:** The project was originally developed on an ESP32-C3 module. Due to persistent and irresolvable WiFi instability on the C3, the hardware was migrated to the **ESP32-S3-N16R8 devKitC-1**. All firmware files have been updated accordingly. See the [Migration Plan](#migration-plan-esp32-c3--esp32-s3) section for the full rationale and change log.

---

## Repository
- **GitHub:** https://github.com/PeterHomberg/WateringSystem
- **Structure:**
  ```
  WateringSystem/
  ├── firmware/          ← ESP32 PlatformIO project
  └── ios/               ← iOS Xcode project (to be created)
  ```

---

## Hardware

### Main Controller
| Component | Details |
|---|---|
| Board | ESP32-S3-N16R8 devKitC-1 (Espressif) |
| Architecture | Xtensa LX7 Dual-Core, 240 MHz |
| Flash | 16 MB |
| PSRAM | 8 MB (OPI) |
| Operating voltage | 3.3 V GPIO |
| USB | USB-C (USB-OTG + JTAG) |

### Peripherals
| Component | Type | Interface | Pins | Notes |
|---|---|---|---|---|
| Valve 1 | Solenoid valve | GPIO output | GPIO3 | Via relay module |
| Valve 2 | Solenoid valve | GPIO output | GPIO4 | Via relay module |
| Rain sensor (digital) | Rain detector | GPIO input | GPIO2 | Active LOW |
| Rain sensor (analog) | Wetness level | ADC1-CH0 | GPIO1 | 0–4095 raw, lower = wetter |
| DS3231 RTC | Real-time clock | I2C | SDA=GPIO8, SCL=GPIO9 | Shared I2C bus |
| Display | 1.3″ OLED SH1106, 128×64 | I2C | SDA=GPIO8, SCL=GPIO9 | Shared I2C bus |

> **Important:** The display controller is **SH1106**, not SSD1306. These are different chips and require different drivers. See the Display section below.

### Power
- Valves driven via relay module (relay coil powered separately)
- ESP32-S3 powered via USB-C or external 3.3–5 V supply

---

## Firmware

### Development Environment
| Item | Details |
|---|---|
| OS | Windows 11 |
| IDE | VSCode with PlatformIO extension |
| Platform | pioarduino espressif32 (ZIP install, bypasses PlatformIO registry) |
| Framework | Arduino (arduino-esp32 v3.3.7) |
| Python | 3.12 (required — v3.9 and v3.14 do NOT work) |
| PlatformIO Core | 6.1.19 |

### platformio.ini (current — ESP32-S3)
```ini
[platformio]
default_envs = esp32s3

[env:esp32s3]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
board_build.mcu = esp32s3
board_build.f_cpu = 240000000L
board_build.sdkconfig = sdkconfig.defaults
board_build.partitions = partitions.csv
build_flags = -I include
lib_deps =
    h2zero/NimBLE-Arduino
    adafruit/Adafruit GFX Library
    olikraus/U8g2                     ; SH1106 OLED driver
    me-no-dev/ESP Async WebServer     ; replaces synchronous WebServer
    me-no-dev/AsyncTCP
    adafruit/RTClib
    bblanchon/ArduinoJson
```

### sdkconfig.defaults
```
CONFIG_BT_NIMBLE_EXT_ADV=n
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
```
> `CONFIG_BT_NIMBLE_EXT_ADV=n` is required for BLE visibility on iOS.  
> `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` enables the USB-C serial monitor on the S3.  
> **Delete the `.pio` folder whenever `sdkconfig.defaults` changes**, then rebuild.

### Project Structure
```
firmware/
├── platformio.ini
├── sdkconfig.defaults
├── partitions.csv
├── include/
│   └── config.h              ← all pin definitions, UUIDs, constants
└── src/
    ├── main.cpp              ← setup() and loop() only
    ├── display/
    │   ├── display.h
    │   └── display.cpp       ← OLED functions (U8g2 / SH1106)
    ├── ble/
    │   ├── ble_server.h
    │   └── ble_server.cpp    ← BLE server, characteristics, callbacks
    ├── valves/
    │   ├── valves.h
    │   └── valves.cpp        ← valve open/close control
    ├── rain/
    │   ├── rain.h
    │   └── rain.cpp          ← rain sensor + simulation mode
    ├── rtc/
    │   ├── rtc.h
    │   └── rtc.cpp           ← DS3231 RTC (I2C)
    ├── scheduler/
    │   ├── scheduler.h
    │   └── scheduler.cpp     ← watering schedule logic
    ├── wifi_manager/
    │   ├── wifi_manager.h
    │   └── wifi_manager.cpp  ← WiFi connection with auto-reconnect
    └── web_server/
        ├── web_server.h
        └── web_server.cpp    ← AsyncWebServer HTTP API
```

### config.h (include/config.h)
```cpp
#pragma once

// ─── Display (SH1106, 128×64, I2C) ────────────────────────
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C
#define SDA_PIN         8       // GPIO8
#define SCL_PIN         9       // GPIO9

// ─── WiFi ──────────────────────────────────────────────────
#define WIFI_SSID              "Rainy"
#define WIFI_PASSWORD          "@Gehlenberg2"
#define WIFI_CONNECT_TIMEOUT_MS  15000
#define WEB_SERVER_PORT        80

// ─── BLE ───────────────────────────────────────────────────
#define BLE_DEVICE_NAME     "WateringSystem"
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789000"
#define STATUS_UUID         "12345678-1234-1234-1234-123456789001"
#define SCHEDULE_UUID       "12345678-1234-1234-1234-123456789002"
#define RAIN_UUID           "12345678-1234-1234-1234-123456789003"
#define MANUAL_UUID         "12345678-1234-1234-1234-123456789004"
#define TIME_UUID           "12345678-1234-1234-1234-123456789005"

// ─── Valves ────────────────────────────────────────────────
#define VALVE1_PIN      3       // GPIO3
#define VALVE2_PIN      4       // GPIO4

// ─── Rain sensor ───────────────────────────────────────────
#define RAIN_PIN_DIGITAL    2   // GPIO2 — active LOW (LOW = raining)
#define RAIN_PIN_ANALOG     1   // GPIO1 — ADC1_CH0, lower = wetter

// Analog thresholds (12-bit ADC, 0–4095). Tune after physical testing.
#define RAIN_ANALOG_DRY_THRESHOLD   3500
#define RAIN_ANALOG_WET_THRESHOLD   1500
```

---

## BLE Protocol

### Device Advertisement
- **Device name:** `WateringSystem`
- **Service UUID:** `12345678-1234-1234-1234-123456789000`
- **Library:** NimBLE-Arduino (h2zero) v2.x
- **Advertising:** Legacy advertising (not extended) — required for iOS visibility

### Characteristics

| UUID | Name | Direction | Properties | Format |
|---|---|---|---|---|
| `...9001` | STATUS | ESP32 → iOS | READ + NOTIFY | `R:0,L:42,V1:0,V2:0,SCH:OK,T:14:30` |
| `...9002` | SCHEDULE | iOS → ESP32 | WRITE | Line-by-line schedule protocol |
| `...9003` | RAIN | ESP32 → iOS | READ + NOTIFY | `R:0,L:42` |
| `...9004` | MANUAL | iOS → ESP32 | WRITE | `V1:1`, `V1:0`, `V2:1`, `V2:0` |
| `...9005` | TIME | iOS → ESP32 | READ + WRITE | `2025-04-07T14:30:00,1` (ISO + weekday) |

### STATUS format
```
R:0,L:42,V1:0,V2:0,SCH:OK,T:14:30
│   │     │    │    │        └── RTC time HH:MM
│   │     │    │    └─────────── Schedule acknowledgement
│   │     │    └──────────────── Valve 2 state (0=closed, 1=open)
│   │     └───────────────────── Valve 1 state (0=closed, 1=open)
│   └─────────────────────────── Analog wetness level 0–100
└─────────────────────────────── Rain detected (0=no, 1=raining)
```

### RAIN format
```
R:0,L:42
│   └── Analog wetness level 0–100
└─────── Digital rain flag (0=dry, 1=raining)
```

### MANUAL command format
```
V1:1  → Open valve 1
V1:0  → Close valve 1
V2:1  → Open valve 2
V2:0  → Close valve 2
```

### NimBLE v2.x API notes
```cpp
// Server callbacks
void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;

// Characteristic callbacks
void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override;
```

---

## Web Server (HTTP API)

Served by **ESPAsyncWebServer** on port 80. Runs on a background core — no polling required in `loop()`.

| Method | Endpoint | Description |
|---|---|---|
| GET | `/` | Dashboard HTML page |
| GET | `/api/status` | Full JSON status snapshot |
| POST | `/api/valve` | `{"valve":1,"open":true}` |
| POST | `/api/time` | `{"datetime":"2025-04-07T14:30:00","weekday":1}` |
| GET | `/api/schedule` | JSON array of schedule entries |
| POST | `/api/schedule` | JSON array (replaces all entries) |

WiFi connection uses `setAutoReconnect(true)`. If WiFi is unavailable at boot, the system runs in BLE-only mode. The web server is started (or restarted) automatically when a connection is established.

---

## Display

### Hardware
**SH1106** — 1.3″ OLED, 128×64, I2C, address `0x3C`.

> **Critical:** SH1106 and SSD1306 are different controllers. Using an SSD1306 driver on an SH1106 display produces a shifted, garbled image. The firmware uses **U8g2** (`olikraus/U8g2`) which supports the SH1106 natively.

### Driver initialisation
```cpp
#include <U8g2lib.h>
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
// Wire.begin(SDA_PIN, SCL_PIN) must be called before u8g2.begin()
```

### Display layout
```
Y= 0  "Watering System"                        HH:MM
Y=10  ──────────────────────────────────────────────
Y=14  "WiFi: 192.168.1.42"  OR  "BLE: Connected/Adv"
Y=24  "Rain: No  [████░]  42%"
Y=34  "V1: OPEN/closed       V2: OPEN/closed"
Y=44  ──────────────────────────────────────────────
Y=48  <IP address>  OR  "BLE only - no WiFi"
```

---

## Modules Status

| Module | Status | Notes |
|---|---|---|
| `display` | 🔄 In progress | Needs rewrite from Adafruit SSD1306 to U8g2/SH1106 |
| `ble` | ✅ Complete | BLE server visible on iOS, all 5 characteristics working |
| `valves` | ✅ Complete | GPIO control, open/close/status functions |
| `rain` | ✅ Complete | Real GPIO + simulation mode; averaged ADC reads |
| `scheduler` | ✅ Complete | Line-by-line BLE protocol, weekday/time matching |
| `rtc` | ✅ Complete | DS3231 via I2C; time sync via BLE TIME characteristic |
| `wifi_manager` | 🔄 In progress | Needs rewrite with `setAutoReconnect` and reconnect loop |
| `web_server` | 🔄 In progress | Needs migration from synchronous `WebServer` to `AsyncWebServer` |

---

## Migration Plan: ESP32-C3 → ESP32-S3

### Why the migration was necessary

The ESP32-C3 proved unable to establish a stable WiFi connection with the available TP-Link CPE210 router. Despite extensive troubleshooting (SSID/password, channel settings, driver updates), the connection either failed to associate or dropped within seconds. This is a known hardware compatibility issue between some C3 variants and certain router firmware. The ESP32-S3 uses a different RF front end and has a stronger track record of WiFi stability in the field.

### What changed and why

#### 1. `platformio.ini` — Board target
- `board` changed from `esp32-c3-devkitc-02` to `esp32-s3-devkitc-1`
- `board_build.mcu` changed from `esp32c3` to `esp32s3`
- `board_build.f_cpu` increased from `160000000L` to `240000000L` (S3 dual-core LX7 runs at 240 MHz)
- `lib_deps` updated: `Adafruit SSD1306` removed; `olikraus/U8g2` added for SH1106 support; `me-no-dev/ESP Async WebServer` and `me-no-dev/AsyncTCP` added

#### 2. `sdkconfig.defaults` — S3 USB console
- Added `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` so the USB-C port works for serial monitoring on the S3 devKitC-1.
- `CONFIG_BT_NIMBLE_EXT_ADV=n` retained — still required for iOS BLE visibility.

#### 3. `config.h` — Pin reassignment
The C3 and S3 devKitC boards have different GPIO layouts. All pins updated:

| Signal | C3 (old) | S3 (new) | Reason |
|---|---|---|---|
| SDA | GPIO5 | GPIO8 | S3 devKitC default I2C SDA |
| SCL | GPIO6 | GPIO9 | S3 devKitC default I2C SCL |
| VALVE1_PIN | GPIO3 | GPIO3 | unchanged |
| VALVE2_PIN | GPIO4 | GPIO4 | unchanged |
| RAIN_PIN_DIGITAL | GPIO1 | GPIO2 | GPIO1 reserved on S3 for ADC bootstrap |
| RAIN_PIN_ANALOG | GPIO0 | GPIO1 | GPIO0 used for boot mode on S3 |

> **S3 ADC caution:** Avoid GPIO19/20 for ADC — these are the USB D+/D− lines on the S3 devKitC-1.

#### 4. `display/` — Driver replacement (SSD1306 → U8g2/SH1106)
The display hardware is an **SH1106**, but the original code used `Adafruit_SSD1306`. The SSD1306 driver produces a horizontally offset, garbled image on an SH1106 panel. The display module must be rewritten to use **U8g2** (`olikraus/U8g2`), which supports the SH1106 controller natively. The display layout (Y-coordinates, content) stays the same; only the drawing API calls change.

```cpp
// OLD
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// NEW
#include <U8g2lib.h>
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
```

#### 5. `wifi_manager/` — Reliability improvements
The original `initWiFi()` blocked in a polling loop and had no reconnect logic. Updated for the S3:
- `WiFi.setAutoReconnect(true)` — the S3 RF stack handles reconnect reliably
- `WiFi.persistent(false)` — prevents unnecessary NVS flash writes on every connect
- Reconnection is monitored in `loop()`; the web server is restarted automatically on reconnect

#### 6. `web_server/` — Migration to AsyncWebServer
The synchronous `WebServer` required `handleWebServer()` to be polled every loop iteration, which competes with BLE and scheduler timing. Replaced with **ESPAsyncWebServer**, which runs on a background FreeRTOS task using the S3's second core. Benefits:
- No polling required in `loop()`
- Better concurrent request handling
- More stable under BLE + WiFi coexistence loads

### Migration sequence (recommended)
1. Update `platformio.ini` and `sdkconfig.defaults`. Delete `.pio`, rebuild, confirm USB-C serial works and board flashes correctly.
2. Update `config.h` with new pin assignments.
3. Rewrite `display/` module using U8g2 — test visually before continuing.
4. Rewrite `wifi_manager/` with reconnect logic — test WiFi stability alongside BLE.
5. Migrate `web_server/` to AsyncWebServer — test all HTTP endpoints.
6. Full integration test: scheduler + RTC + rain sensor + BLE + web server running together.

---

## Known Issues & Solutions

### BLE not visible on iOS
**Cause:** arduino-esp32 v3.x enables extended advertising by default — iOS cannot see it.  
**Fix:** Add `sdkconfig.defaults` with `CONFIG_BT_NIMBLE_EXT_ADV=n` and set `board_build.sdkconfig = sdkconfig.defaults` in `platformio.ini`. Delete `.pio` folder and rebuild.

### USB-C serial not working on ESP32-S3 devKitC-1
**Cause:** The S3 uses a USB Serial/JTAG peripheral, not a separate UART bridge chip.  
**Fix:** Add `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` to `sdkconfig.defaults`. Delete `.pio` and rebuild. On first flash after changing sdkconfig, hold the **BOOT** button while pressing **RESET**.

### pip/PlatformIO installation in China
**Cause:** Default PyPI and PlatformIO registry are blocked by GFW.  
**Fix:**
1. `C:\Users\Peter\pip\pip.ini` → Aliyun mirror
2. Use pioarduino ZIP in `platformio.ini` instead of PlatformIO registry
3. Patched `penv_setup.py` to use pip instead of broken `uv 0.11.3`
4. Marker file at `C:\Users\Peter\.platformio\penv\.deps_installed` skips pip check on every build

### uv.exe crash (exit code 0xC0000005)
**Cause:** NimBLE-pioarduino bundles `uv 0.11.3` which crashes on this system.  
**Fix:** Custom `penv_setup.py` replacing all `uv` calls with `pip` + Aliyun mirror. Patched file is in the repository root as `penv_setup.py` — must be copied to:  
`C:\Users\Peter\.platformio\platforms\espressif32\builder\penv_setup.py`

---

## Next Development Phases
1. ✅ ~~Decide on hardware platform~~ — migrated to ESP32-S3-N16R8 devKitC-1
2. 🔄 **Rewrite `display/`** — replace Adafruit SSD1306 with U8g2/SH1106 driver
3. 🔄 **Rewrite `wifi_manager/`** — add `setAutoReconnect`, reconnect-triggered web server restart
4. 🔄 **Migrate `web_server/`** — replace synchronous `WebServer` with `AsyncWebServer`
5. **iOS app** — Scanner, Dashboard, Manual control, Schedule, Settings screens
6. **OTA update support** — over-the-air firmware updates via web interface

---

## Testing

### BLE testing tool
**nRF Connect** (Nordic Semiconductor) — free iOS/Android app. Used to verify all BLE characteristics before iOS app development.

### Verified with nRF Connect (on ESP32-S3)
- [x] Device visible as "WateringSystem"
- [x] STATUS characteristic readable: `R:0,L:0,V1:0,V2:0,SCH:OK,T:--:--`
- [x] MANUAL write `V1:1` opens valve (confirmed in serial monitor)
- [x] RAIN notifications arrive every 2 s
- [x] TIME write updates RTC
- [x] BLE reconnects after disconnect

---

## Development Notes

- Always use `py -3.12 -m platformio` in terminal — not just `pio` — to ensure Python 3.12 is used
- Delete `.pio` folder when changing `sdkconfig.defaults`
- The `.deps_installed` marker file in penv must exist for fast builds — recreate with `echo ok > C:\Users\Peter\.platformio\penv\.deps_installed` if penv is deleted
- Rain sensor simulation toggles in `rain.cpp` — replace with real GPIO read when hardware is connected
- `Wire.begin(SDA_PIN, SCL_PIN)` must be called in `setup()` **before** both `u8g2.begin()` and `initRTC()`
- On first flash after sdkconfig changes: hold BOOT, press RESET, then release BOOT before uploading
