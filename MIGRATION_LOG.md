# Watering System — Migration Log & Current Status

## Last updated: 2026-05-15

---

## Overview

This document records all problems encountered and solutions applied during the
ESP32-C3 → ESP32-S3 hardware migration, and the current status of all migration
steps as reflected in the source code.

---

## Problems Encountered & Solutions Applied

### Problem 1 — Wrong board ID

**Symptom:**
```
UnknownBoard: Unknown board ID 'esp32-s3-devkitc-1-n16r8'
```

**Cause:** There is no separate PlatformIO board ID for the N16R8 variant.
PlatformIO uses a single `esp32-s3-devkitc-1` ID for all S3 devKitC-1 variants,
with flash and PSRAM configured via override settings.

**Fix:** Keep `board = esp32-s3-devkitc-1` and override flash/PSRAM explicitly:
```ini
board = esp32-s3-devkitc-1
board_build.arduino.memory_type = qio_opi
board_build.flash_mode = qio
board_upload.flash_size = 16MB
board_upload.maximum_size = 16777216
```

---

### Problem 2 — UTF-8 BOM in platformio.ini

**Symptom:**
```
InvalidProjectConfError: File contains no section headers.
'\ufeff[platformio]\n'
```

**Cause:** PowerShell's `Set-Content -Encoding UTF8` writes a UTF-8 BOM
(byte order mark) which PlatformIO's config parser rejects.

**Fix:** In VSCode, click the encoding indicator in the bottom-right status bar
(`UTF-8 with BOM`) → **Save with Encoding** → **UTF-8** (without BOM).

**Prevention:** When writing `.ini` files from PowerShell, use
`-Encoding UTF8NoBOM` (PowerShell 6+) or edit directly in VSCode.

---

### Problem 3 — Xtensa toolchain download fails (GFW / idf_tools.py)

**Symptom:**
```
FileNotFoundError: [WinError 2] Das System kann die angegebene Datei nicht finden:
'xtensa-esp-elf-14.2.0_20251107-x86_64-w64-mingw32.zip.tmp' ->
'xtensa-esp-elf-14.2.0_20251107-x86_64-w64-mingw32.zip'
```

**Cause:** The GFW interrupted the download mid-stream, leaving a `.tmp` file
with no matching destination. `idf_tools.py` then failed to rename it.

**Fix:** Delete the `.tmp` file. The full ZIP was already present in
`C:\Users\Peter\.platformio\dist\` from a previous attempt, so PlatformIO
found it on the next run and skipped the download.

---

### Problem 4 — `me-no-dev/ESP Async WebServer` not found

**Symptom:**
```
UnknownPackageError: Could not find the package with
'me-no-dev/ESP Async WebServer' requirements
```

**Cause:** The `me-no-dev` fork is outdated and unreachable through the GFW.

**Fix:** Switch to the actively maintained community fork:
```ini
lib_deps =
    mathieucarbou/ESP Async WebServer
    mathieucarbou/AsyncTCP
```

---

### Problem 5 — Toolchain extracted with wrong structure (idf_tools.py strip bug)

**Symptom:**
```
RuntimeError: at level 0, expected 1 entry, got ['package.json', 'xtensa-esp-elf']
```
followed by:
```
Der Befehl "xtensa-esp32s3-elf-g++" ist entweder falsch geschrieben oder
konnte nicht gefunden werden.
```

**Cause:** The pioarduino `stable` toolchain ZIP contains both a `package.json`
file and an `xtensa-esp-elf/` folder at the root level. The `idf_tools.py`
`do_strip_container_dirs()` function expects exactly one entry at the root and
crashes when it finds two.

**Root fix:** Switch from the floating `stable` tag to a **pinned release**
`53.03.11` which uses an older packaging format without this issue:
```ini
platform = https://github.com/pioarduino/platform-espressif32/releases/download/53.03.11/platform-espressif32.zip
```

**Temporary workarounds applied during diagnosis:**

1. Manually moved the contents of `xtensa-esp-elf\` up one level using
   PowerShell `Move-Item`.
2. Patched `idf_tools.py` line 1635 to replace the `do_strip_container_dirs()`
   call with `pass` — prevents the crash but leaves the nested structure broken.
3. Added picolibc include path to `build_flags` — caused conflicts with ESP32
   Arduino newlib headers and was reverted.

**Final fix:** Pinning to release `53.03.11` resolved all toolchain issues
cleanly without any manual patching.

---

### Problem 6 — `Adafruit_SSD1306.h` not found

**Symptom:**
```
src/display/display.cpp:5:10: fatal error: Adafruit_SSD1306.h: No such file or directory
```

**Cause:** `display.cpp` still included the old SSD1306 driver which was
intentionally removed from `lib_deps`. The display module rewrite had not yet
been done.

**Fix:** Replaced `display.cpp` and `display.h` with a full U8g2/SH1106
implementation (see Problem 7 below — display rewrite and temporary stub were
done in the same session).

---

### Problem 7 — WiFi connection fails against TP-LINK CPE210 (status 6)

**Session date: 2026-05-15**

**Symptom:** ESP32-S3 connects successfully to the house router ("Rainy") but
times out on every connection attempt to the rooftop TP-LINK CPE210
(`TP-LINK_2.4G_AAE2`). Status code 6 (`WL_DISCONNECTED`) on every attempt.

**Diagnosis steps:**

1. `netsh wlan show networks` on Windows confirmed the CPE210 is visible at
   99% signal on channel 11 — ruled out range and channel issues.
2. Added scan block to firmware — ESP32 could see the SSID, Auth type 4
   (`WIFI_AUTH_WPA2_WPA3_PSK`), RSSI = -8 dBm.
3. Enabled `ESP_LOG_VERBOSE` + `CORE_DEBUG_LEVEL=5` — IDF log showed:
   ```
   Reason: 15 - 4WAY_HANDSHAKE_TIMEOUT
   ```
   on every attempt. Association succeeded; WPA2 4-way handshake timed out.

**Root cause:** Password mismatch between `config.h` and the CPE210.
The 4-way handshake timeout at reason 15 is the standard symptom when the PSK
derived from the passphrase does not match the AP's stored key.

**Fix:** Corrected `WIFI_PASSWORD` in `config.h` to match the CPE210's actual
passphrase. Connected on first attempt after the fix.

**Other changes made during diagnosis (retained as improvements):**

- `initWiFi()` now uses a **3-attempt retry loop** with `WiFi.disconnect(true)`
  + 100 ms reset between attempts, making the connection more robust against
  transient association failures.
- `wifi_config_t` with `threshold.authmode = WIFI_AUTH_WPA2_PSK` and
  `pmf_cfg.capable = false` — forces WPA2-only association, preventing silent
  failures if the AP advertises WPA2/WPA3 mixed mode in future.
- `<esp_wifi.h>` added to includes.

---

## Final platformio.ini (current)

```ini
[platformio]
default_envs = esp32s3

[env:esp32s3]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/53.03.11/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200

board_build.mcu = esp32s3
board_build.f_cpu = 240000000L
board_build.arduino.memory_type = qio_opi
board_build.flash_mode = qio
board_upload.flash_size = 16MB
board_upload.maximum_size = 16777216

build_flags =
    -I include
    -DBOARD_HAS_PSRAM

board_build.sdkconfig   = sdkconfig.defaults
board_build.partitions  = partitions.csv

lib_deps =
    h2zero/NimBLE-Arduino
    adafruit/Adafruit GFX Library
    olikraus/U8g2
    mathieucarbou/ESP Async WebServer
    mathieucarbou/AsyncTCP
    adafruit/RTClib
    bblanchon/ArduinoJson

upload_port = COM7
monitor_port = COM7
```

**Notes vs previous version:**
- `-DARDUINO_USB_MODE=1` and `-DARDUINO_USB_CDC_ON_BOOT=1` removed from
  `build_flags` — these are now handled by `sdkconfig.defaults` entries
  (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`) and do not need to be duplicated.
- `upload_port` and `monitor_port` pinned to `COM7`.

---

## Final sdkconfig.defaults (current)

```ini
# --- BLE -----------------------------------------------------------------------
# Legacy advertising required — iOS cannot see extended (BT5) advertisements.
CONFIG_BT_ENABLED=y
CONFIG_BT_BLE_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_EXT_ADV=n
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=3

# --- WiFi ----------------------------------------------------------------------
CONFIG_ESP_WIFI_ENABLED=y
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=4
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=8

# --- lwIP — larger stack required by AsyncWebServer ----------------------------
CONFIG_LWIP_TCPIP_TASK_STACK_SIZE=4096

# --- USB serial (ESP32-S3 specific) --------------------------------------------
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y

# --- PSRAM — OPI mode for N16R8 variant ----------------------------------------
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
```

---

## Current Project Status

### Build status: ✅ SUCCESS

```
PLATFORM: Espressif 32 > Espressif ESP32-S3-DevKitC-1 (16MB Flash)
HARDWARE: ESP32S3 240MHz
ESP-IDF: v5.3.2
Arduino: 3.1.1
```

### Migration steps status

| Step | Description | Status |
|---|---|---|
| 1 | `platformio.ini` + `sdkconfig.defaults` — board compiling | ✅ Complete |
| 2 | Rewrite `display/` — SSD1306 → U8g2/SH1106 | ✅ Complete |
| 3 | Rewrite `wifi_manager/` — WPA2 fix, retry loop, reconnect | ✅ Complete |
| 4 | Migrate `web_server/` — sync `WebServer` → `AsyncWebServer` | ✅ Complete |
| 5 | Integration test — all modules running together | ⏳ Pending |
| 6 | Flash to hardware and verify all peripherals | ⏳ Pending |

### Module status

| Module | Compiles | Hardware tested | Notes |
|---|---|---|---|
| `ble` | ✅ | ⏳ | Visible on iOS via nRF Connect; all 5 characteristics working |
| `valves` | ✅ | ⏳ | No changes needed from C3 version |
| `rain` | ✅ | ⏳ | No changes needed from C3 version |
| `rtc` | ✅ | ⏳ | No changes needed from C3 version |
| `scheduler` | ✅ | ⏳ | No changes needed from C3 version |
| `wifi_manager` | ✅ | ✅ | Connected to CPE210 — IP 10.0.0.5 confirmed in serial monitor |
| `web_server` | ✅ | ⏳ | AsyncWebServer + full dashboard HTML; all endpoints implemented |
| `display` | ✅ | ⏳ | Full U8g2/SH1106 rewrite complete; hardware render not yet verified |

### Warnings (non-blocking)

All `StaticJsonDocument` / `DynamicJsonDocument` deprecation warnings from the
previous session have been resolved — `web_server.cpp` now uses `JsonDocument`
throughout. The `NimBLEService::start()` deprecation warning in `ble_server.cpp`
remains but is harmless.

---

## Known Gotchas for Future Sessions

- **Never use `stable` ZIP for pioarduino** on this system — the toolchain
  packaging format is incompatible with `idf_tools.py`. Always pin to a
  specific numbered release (e.g. `53.03.11`).

- **PowerShell `Set-Content -Encoding UTF8` writes BOM** — always edit
  `platformio.ini` directly in VSCode and verify encoding in the status bar.

- **Toolchain reinstalls on every `.pio` delete** — avoid deleting `.pio`
  unless necessary. Use `py -3.12 -m platformio run` without `--target clean`
  for incremental builds.

- **First flash after sdkconfig change** — hold BOOT, press RESET, release
  BOOT before uploading. Auto-download mode works on subsequent flashes.

- **Always use `py -3.12 -m platformio`** — not just `pio` — to ensure
  Python 3.12 is used (3.9 and 3.14 do not work on this system).

- **CPE210 WiFi password** — the correct passphrase is stored in `config.h`
  as `WIFI_PASSWORD`. A mismatch produces `Reason: 15 - 4WAY_HANDSHAKE_TIMEOUT`
  in the IDF verbose log, not a generic connection failure message.

- **AsyncWebServer cannot be re-initialised** — `initWebServer()` is guarded
  by a `serverStarted` flag. On WiFi reconnect the server keeps running; only
  the reconnect IP is logged. Do not delete and recreate the server object.

- **`Wire.begin()` is called inside `initDisplay()`** — do not call it again
  in `main.cpp` or `initRTC()`. Both I2C devices share the same bus instance.

- **`config.h` contains two SSID definitions** — `Rainy` (house router) is
  commented out; `TP-LINK_2.4G_AAE2` (CPE210) is active. Swap comments to
  switch networks during development.
