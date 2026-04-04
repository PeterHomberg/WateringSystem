# Watering System — Project Context for Claude

## Project Overview
An automated garden watering controller consisting of:
- **ESP32-C3 firmware** (C++/Arduino, developed on Windows 11 with VSCode + PlatformIO)
- **iOS app** (Swift/CoreBluetooth, developed on Mac with Xcode)

The two components communicate exclusively via **Bluetooth LE (BLE)**.
There is no WiFi available at the installation location — BLE is the only communication channel.

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
| Board | ESP32-C3-DevKitC-02 (Espressif) |
| Architecture | 32-bit RISC-V, single core, 160 MHz |
| Flash | 4MB |
| Antenna | External antenna via IPEX connector |
| Operating voltage | 3.3V GPIO |
| USB | USB-C |

### Peripherals
| Component | Type | Interface | Pins | Notes |
|---|---|---|---|---|
| SSD1312 OLED | 128x64 display | I2C | SDA=GPIO5, SCL=GPIO6 | Address 0x3C, needs 0xA1+0xC0 init commands to fix mirroring |
| Valve 1 | Solenoid valve | GPIO output | GPIO3 | Via relay module |
| Valve 2 | Solenoid valve | GPIO output | GPIO4 | Via relay module |
| Rain sensor | Digital rain detector | GPIO input | GPIO7 | Active LOW |
| DS3231 RTC | Real time clock | I2C | SDA=GPIO5, SCL=GPIO6 | Shared I2C bus, not yet implemented |

### Power
- Valves driven via relay module (relay coil powered separately)
- ESP32 powered via USB-C or external 3.3-5V supply

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

### platformio.ini
```ini
[platformio]
default_envs = esp32c3

[env:esp32c3]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32-c3-devkitc-02
framework = arduino
monitor_speed = 115200
board_build.mcu = esp32c3
board_build.f_cpu = 160000000L
build_flags = -I include
board_build.sdkconfig = sdkconfig.defaults
lib_deps =
    adafruit/Adafruit SSD1306
    adafruit/Adafruit GFX Library
    h2zero/NimBLE-Arduino
```

### sdkconfig.defaults
```
CONFIG_BT_ENABLED=y
CONFIG_BT_BLE_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_EXT_ADV=n
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=3
```
> **Important:** `CONFIG_BT_NIMBLE_EXT_ADV=n` is mandatory — extended advertising
> is enabled by default in arduino-esp32 v3.x and makes the device invisible to iOS.

### Project Structure
```
firmware/
├── platformio.ini
├── sdkconfig.defaults
├── include/
│   └── config.h              ← all pin definitions, UUIDs, constants
└── src/
    ├── main.cpp              ← setup() and loop() only
    ├── display/
    │   ├── display.h
    │   └── display.cpp       ← OLED functions
    ├── ble/
    │   ├── ble_server.h
    │   └── ble_server.cpp    ← BLE server, characteristics, callbacks
    ├── valves/
    │   ├── valves.h
    │   └── valves.cpp        ← valve open/close control
    ├── rain/
    │   ├── rain.h
    │   └── rain.cpp          ← rain sensor + simulation mode
    └── scheduler/            ← NOT YET IMPLEMENTED
        ├── scheduler.h
        └── scheduler.cpp
```

### config.h (include/config.h)
```cpp
#pragma once

// Display
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C
#define SDA_PIN         5
#define SCL_PIN         6

// BLE
#define BLE_DEVICE_NAME     "WateringSystem"
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789000"
#define STATUS_UUID         "12345678-1234-1234-1234-123456789001"
#define SCHEDULE_UUID       "12345678-1234-1234-1234-123456789002"
#define RAIN_UUID           "12345678-1234-1234-1234-123456789003"
#define MANUAL_UUID         "12345678-1234-1234-1234-123456789004"

// Valves
#define VALVE1_PIN      3
#define VALVE2_PIN      4

// Rain sensor
#define RAIN_PIN        7
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
| `...9001` | STATUS | ESP32 → iOS | READ + NOTIFY | `R:0,V1:0,V2:0` |
| `...9002` | SCHEDULE | iOS → ESP32 | WRITE | TBD |
| `...9003` | RAIN | ESP32 → iOS | READ + NOTIFY | `R:0` or `R:1` |
| `...9004` | MANUAL | iOS → ESP32 | WRITE | `V1:1`, `V1:0`, `V2:1`, `V2:0` |

### STATUS format
```
R:0,V1:0,V2:0
│    │    └── Valve 2 state (0=closed, 1=open)
│    └─────── Valve 1 state (0=closed, 1=open)
└──────────── Rain detected (0=no rain, 1=raining)
```

### MANUAL command format
```
V1:1  → Open valve 1
V1:0  → Close valve 1
V2:1  → Open valve 2
V2:0  → Close valve 2
```

### SCHEDULE format
Not yet defined — to be designed when scheduler module is implemented.

### NimBLE v2.x API notes
The following signatures are required (different from v1.x):
```cpp
// Server callbacks
void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;

// Characteristic callbacks
void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override;
```

---

## Display

### Initialization quirk
The SSD1312 requires two extra I2C commands after `display.begin()` to fix
horizontal mirroring caused by the physical mounting orientation:
```cpp
sendOLEDCommand(0xA1); // segment remap reversed — fixes horizontal mirror
sendOLEDCommand(0xC0); // COM scan normal — top to bottom
```

### ShowText function
```cpp
void showText(int x, int y, const char* text, int size = 1);
```
- `x`, `y` = pixel position (0,0 = top left)
- `size` = 1 (6x8px, 21 chars/line), 2 (12x16px), 3 (18x24px)
- To update a value without flicker: use `display.fillRect()` to clear the area first

### Display layout (current)
```
Y=0  : "Watering System"
Y=10 : ─────────────────── (divider line)
Y=14 : "BLE: Connected / Advertising"
Y=26 : "Rain: No / YES - inhibit"
Y=38 : "V1: open/closed   V2: open/closed"
Y=52 : "Use nRF Connect"
```

---

## Modules Status

| Module | Status | Notes |
|---|---|---|
| display | ✅ Complete | OLED working with position text |
| ble | ✅ Complete | BLE server visible on iOS, all 4 characteristics working |
| valves | ✅ Complete | GPIO control, open/close/status functions |
| rain | ✅ Complete | Real GPIO + simulation mode for testing |
| scheduler | ❌ Not started | Needs DS3231 RTC first |
| rtc (DS3231) | ❌ Not started | Next phase |

---

## Known Issues & Solutions

### BLE not visible on iOS
**Cause:** arduino-esp32 v3.x enables extended advertising by default — iOS cannot see it.
**Fix:** Add `sdkconfig.defaults` with `CONFIG_BT_NIMBLE_EXT_ADV=n` and set `board_build.sdkconfig = sdkconfig.defaults` in `platformio.ini`. Delete `.pio` folder and rebuild.

### pip/PlatformIO installation in China
**Cause:** Default PyPI and PlatformIO registry are blocked by GFW.
**Fix:**
1. `C:\Users\Peter\pip\pip.ini` → Aliyun mirror
2. Use pioarduino ZIP in `platformio.ini` instead of PlatformIO registry
3. Patched `penv_setup.py` to use pip instead of broken `uv 0.11.3`
4. Marker file at `C:\Users\Peter\.platformio\penv\.deps_installed` skips pip check on every build

### uv.exe crash (exit code 0xC0000005)
**Cause:** NimBLE-pioarduino bundles `uv 0.11.3` which crashes on this system.
**Fix:** Custom `penv_setup.py` replacing all `uv` calls with `pip` + Aliyun mirror.
Patched file is in the repository root as `penv_setup.py` — must be copied to:
`C:\Users\Peter\.platformio\platforms\espressif32\builder\penv_setup.py`

---

## Next Development Phases

### Phase 1 — DS3231 RTC (next)
- Add DS3231 library
- Read/write current time via I2C
- Display time on OLED
- Foundation for scheduler

### Phase 2 — Scheduler
- Define schedule data structure
- Store schedule in flash (LittleFS)
- Check RTC time every minute in loop()
- Open valve at scheduled time, close after duration
- Skip if rain sensor active (inhibit logic)
- Safety: maximum valve open time regardless of schedule

### Phase 3 — BLE Schedule Transfer
- Define SCHEDULE characteristic data format
- Parse schedule written from iOS
- Store persistently to flash

### Phase 4 — iOS App
- Swift / CoreBluetooth
- Scan and connect to "WateringSystem"
- Read STATUS and RAIN with live updates via notifications
- Manual valve control (open/close buttons)
- Schedule editor (days of week, time, duration per zone)
- Show next watering event and countdown
- Rain inhibit status display

---

## iOS App (Planned)

### Requirements
- Minimum iOS: TBD
- Language: Swift
- Framework: CoreBluetooth
- No internet required — pure BLE

### Key screens planned
1. **Scanner** — find and connect to WateringSystem
2. **Dashboard** — live status, rain indicator, valve states
3. **Manual control** — open/close each valve
4. **Schedule** — set watering times and duration per zone
5. **Settings** — device name, connection preferences

---

## Testing

### BLE testing tool
**nRF Connect** (Nordic Semiconductor) — free iOS/Android app.
Used to verify all BLE characteristics before iOS app development.

### Verified with nRF Connect
- [x] Device visible as "WateringSystem"
- [x] STATUS characteristic readable: `R:0,V1:0,V2:0`
- [x] MANUAL write `V1:1` opens valve (confirmed in serial monitor)
- [x] RAIN notifications arrive automatically every 10s (simulated)
- [x] BLE reconnects after disconnect

---

## Development Notes

- Always use `py -3.12 -m platformio` in terminal — not just `pio` — to ensure Python 3.12 is used
- Delete `.pio` folder when changing `sdkconfig.defaults`
- The `.deps_installed` marker file in penv must exist for fast builds — recreate with `echo ok > C:\Users\Peter\.platformio\penv\.deps_installed` if penv is deleted
- Rain sensor simulation toggles every 10 seconds in `main.cpp` — replace with real GPIO read when hardware is connected
