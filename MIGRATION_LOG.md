# Watering System — Migration Log & Current Status

## Session Date: 2026-05-12

---

## Overview

This document records the problems encountered and solutions applied during
migration Step 1 of the ESP32-C3 → ESP32-S3 hardware migration:
**`platformio.ini` + `sdkconfig.defaults` — get the board compiling and flashing.**

---

## Problems Encountered & Solutions Applied

### Problem 1 — Wrong board ID

**Symptom:**
```
UnknownBoard: Unknown board ID 'esp32-s3-devkitc-1-n16r8'
```

**Cause:** There is no separate PlatformIO board ID for the N16R8 variant.
PlatformIO uses a single `esp32-s3-devkitc-1` ID for all S3 devKitC-1 variants,
with the flash and PSRAM configured via override settings.

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
crashes when it finds two. This leaves the compiler buried one level too deep
at `toolchain-xtensa-esp-elf\xtensa-esp-elf\bin\` instead of
`toolchain-xtensa-esp-elf\bin\`.

**Root fix:** Switch from the floating `stable` tag to a **pinned release**
`53.03.11` which uses an older packaging format without this issue:
```ini
platform = https://github.com/pioarduino/platform-espressif32/releases/download/53.03.11/platform-espressif32.zip
```

**Temporary workarounds applied during diagnosis:**

1. Manually moved the contents of `xtensa-esp-elf\` up one level using
   PowerShell `Move-Item`.
2. Patched `idf_tools.py` line 1635 to replace the `do_strip_container_dirs()`
   call with `pass` — this prevents the crash but leaves the nested structure
   in place, which still broke the build.
3. Added picolibc include path to `build_flags` — this caused new conflicts
   with the ESP32 Arduino newlib headers and was reverted.

**Final fix:** Pinning to release `53.03.11` resolved all toolchain issues
cleanly without any manual patching.

---

### Problem 6 — `Adafruit_SSD1306.h` not found

**Symptom:**
```
src/display/display.cpp:5:10: fatal error: Adafruit_SSD1306.h: No such file or directory
```

**Cause:** `display.cpp` still included the old SSD1306 driver which was
intentionally removed from `lib_deps` as part of the SH1106 migration.
The display module rewrite (migration Step 2) had not yet been done.

**Fix:** Replaced `display.cpp` and `display.h` with a stub implementation
that compiles cleanly and prints a serial message, allowing the rest of the
firmware to build while the SH1106/U8g2 rewrite is pending.

---

### Warnings (non-blocking, to be fixed in later steps)

These appeared in the successful build and are noted for future cleanup:

| Warning | Location | Fix planned in |
|---|---|---|
| `StaticJsonDocument` deprecated — use `JsonDocument` | `web_server.cpp` | Migration Step 4 (web server rewrite) |
| `DynamicJsonDocument` deprecated — use `JsonDocument` | `web_server.cpp` | Migration Step 4 |
| `NimBLEService::start()` deprecated — no effect | `ble_server.cpp` | Minor cleanup pass |

---

## Final platformio.ini (working)

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
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1

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
```

---

## Final sdkconfig.defaults (working)

```
CONFIG_BT_ENABLED=y
CONFIG_BT_BLE_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_EXT_ADV=n
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=3

CONFIG_ESP_WIFI_ENABLED=y
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=4
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=8

CONFIG_LWIP_TCPIP_TASK_STACK_SIZE=4096

CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y

CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
```

---

## Current Project Status

### Build status: ✅ SUCCESS

```
PLATFORM: Espressif 32 > Espressif ESP32-S3-DevKitC-1 (16MB Flash)
HARDWARE: ESP32S3 240MHz
framework-arduinoespressif32 @ 3.3.7
```

### Migration steps status

| Step | Description | Status |
|---|---|---|
| 1 | `platformio.ini` + `sdkconfig.defaults` — board compiling | ✅ Complete |
| 2 | Rewrite `display/` — SSD1306 → U8g2/SH1106 | ⏳ Next |
| 3 | Rewrite `wifi_manager/` — add `setAutoReconnect`, reconnect loop | ⏳ Pending |
| 4 | Migrate `web_server/` — sync `WebServer` → `AsyncWebServer` | ⏳ Pending |
| 5 | Integration test — all modules running together | ⏳ Pending |
| 6 | Flash to hardware and verify serial monitor output | ⏳ Pending |

### Module status

| Module | Compiles | Hardware tested | Notes |
|---|---|---|---|
| `ble` | ✅ | ⏳ | One deprecation warning (`pService->start()`) — harmless |
| `valves` | ✅ | ⏳ | No changes needed |
| `rain` | ✅ | ⏳ | No changes needed |
| `rtc` | ✅ | ⏳ | No changes needed |
| `scheduler` | ✅ | ⏳ | No changes needed |
| `wifi_manager` | ✅ | ⏳ | Rewrite pending (Step 3) |
| `web_server` | ✅ | ⏳ | Deprecated ArduinoJson calls — rewrite pending (Step 4) |
| `display` | ✅ | ⏳ | **Stub only** — SH1106/U8g2 rewrite pending (Step 2) |

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
