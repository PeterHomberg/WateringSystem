# WiFi Connection — Problems & Findings

**Project:** WateringSystem — ESP32-C3-DevKitC-02  
**Framework:** Arduino (arduino-esp32 v3.x) + PlatformIO  
**Date:** May 2026

---

## Hardware

| Component | Details |
|---|---|
| Module | ESP32-C3-DevKitC-02 |
| MAC Address | E8:3D:C1:8D:CA:90 |
| Radio | Single 2.4 GHz radio shared between WiFi and BLE |

---

## Networks Tested

| SSID | Result | Notes |
|---|---|---|
| `Rainy` | ✅ Working (with static IP) | WPA2-PSK, channel 9, two BSSIDs (master + repeater) |
| `CPE2G_2G1` | ❌ Failed | CPE router, status 6 every attempt |
| `Benny` | ❌ Failed | Status 6 |

---

## Problem 1 — Intermittent Connection (Every Other Flash)

### Symptom
The ESP32 connected successfully on every other flash/reboot — alternating between success and failure on the same network with the same credentials.

### Root Cause
WiFi initialization order in `setup()`. When `initWiFi()` was called after other modules (`initRTC()`, `initValves()`, etc.), the radio was not fully settled. Moving `initWiFi()` to the very first call after NVS initialization resolved the alternating pattern.

### Fix
Call `initWiFi()` immediately after `nvs_flash_init()`, before `Wire.begin()` and all other module initializations.

---

## Problem 2 — Stuck "STA Connecting" State

### Symptom
After a failed connection attempt, subsequent attempts produced:
```
E wifi: sta is connecting, cannot set config
E STA.cpp: STA clear config failed! 0x3006: ESP_ERR_WIFI_STATE
```

### Root Cause
The Arduino WiFi state machine entered a "connecting" state on the first attempt and could not be reset by `WiFi.disconnect()` alone. The driver rejected all subsequent configuration changes.

### Fix
A retry loop that performs a full `WiFi.mode(WIFI_OFF)` → `WiFi.mode(WIFI_STA)` cycle between attempts to force a complete radio reset. Later replaced by the ESP-IDF takeover approach (see Problem 5).

---

## Problem 3 — CPE Router (`CPE2G_2G1`) — Total Failure

### Symptom
Status code 6 (`WL_DISCONNECTED`) on every attempt. All other devices (phone, notebook) connected without issues.

### Investigation
- MAC filtering ruled out — other devices connected fine
- 2.4 GHz band confirmed available
- WPA2-PSK confirmed in router settings
- AP isolation was enabled in the router virtual interface settings — **disabling it had no effect**
- Static IP tried — no effect
- 802.11b/g protocol forcing tried — no effect
- Strong signal confirmed (RSSI ≈ -34 to -36)

### Router Configuration Found
The CPE router admin panel showed a virtual interface with `Isolate: Enable`. Disabling isolation did not resolve the issue. The router appeared to reject the ESP32-C3's association request at the driver level regardless of settings.

### Status
**Unresolved.** This router is incompatible with the ESP32-C3. The installation will use the `Rainy` network instead.

---

## Problem 4 — AsyncWebServer Crash

### Symptom
ESP32 crashed immediately after WiFi connected:
```
assert failed: tcp_alloc /IDF/components/lwip/lwip/src/core/tcp.c:1854
(Required to lock TCPIP core functionality!)
```

### Root Cause
`mathieucarbou/ESP Async WebServer` uses AsyncTCP which calls lwIP TCP functions from outside the lwIP task context. The ESP32-C3 is single-core and this causes an assertion failure in the lwIP stack.

### Fix
Replaced `ESP Async WebServer` with the built-in synchronous `WebServer` library (part of arduino-esp32). The synchronous server requires `server.handleClient()` to be called from `loop()` but is fully stable on ESP32-C3. `ArduinoJson` was retained for the REST API JSON handling.

---

## Problem 5 — Arduino WiFi Library Reliability

### Symptom
Even after fixing the initialization order and retry loop, connections remained intermittent. The Arduino `WiFi.h` library's internal state machine interfered with retry logic.

### Investigation
Switched from Arduino `WiFi.h` to direct ESP-IDF WiFi API calls to gain full control. Key findings during this investigation:

**Disconnect reason codes observed:**

| Code | Meaning | Frequency |
|---|---|---|
| 2 | `AUTH_EXPIRE` — authentication timed out | Common |
| 36 | `UNSUPP_RSN` — unsupported RSN capability | Every attempt |
| 201 | `NO_AP_FOUND` — AP not visible at connect time | Occasional |
| 205 | `NO_AP_FOUND_W_COMPATIBLE_SECURITY` | After reason 36 |

**Reason 36 analysis:** The ESP32-C3 driver rejected the AP's RSN (Robust Security Network) information element. This occurred even with `authmode = WIFI_AUTH_OPEN`, `pmf_cfg.capable = false`, and `sae_pwe_h2e = WPA3_SAE_PWE_UNSPECIFIED`. The `Rainy` network advertises `encryptionType = 3` (WPA2-PSK) but something in the beacon's RSN IE is triggering the rejection intermittently.

**Arduino vs ESP-IDF interference:** When registering custom ESP-IDF event handlers alongside Arduino's internal handlers, both called `esp_wifi_connect()` on disconnect simultaneously, causing `STA config failed` errors on retry attempts.

### ESP-IDF Takeover Approach
The final working approach:
1. Call `WiFi.mode(WIFI_STA)` to trigger Arduino's internal `esp_netif_create_default_wifi_sta()` and `esp_wifi_init()`
2. Call `WiFi.setAutoReconnect(false)` to disable Arduino's internal retry handler
3. Grab the netif with `esp_netif_next_unsafe(nullptr)`
4. Take over from `esp_wifi_set_config()` onwards using pure ESP-IDF calls
5. Poll `esp_wifi_sta_get_ap_info()` for association status

---

## Problem 6 — DHCP Failure

### Symptom
After switching to the ESP-IDF takeover approach, the polling loop showed:
```
..xxxxxxxxxxxxxxxxxxxxxxx
```
Two dots (associated) then immediate disconnection before IP assignment.

### Root Cause
The `Rainy` network's DHCP server was not responding reliably to the ESP32's DHCP requests. The association itself succeeded but no IP was assigned within the timeout.

### Fix
Static IP configuration — bypasses DHCP entirely:

```cpp
esp_netif_dhcpc_stop(s_staNetif);
esp_netif_set_ip_info(s_staNetif, &ipInfo);   // 10.0.0.45
```

With static IP, the connection succeeds on the first attempt consistently.

---

## Current Working Configuration

### `config.h`
```cpp
#define WIFI_SSID        "Rainy"
#define WIFI_STATIC_IP   "10.0.0.45"
#define WIFI_GATEWAY     "10.0.0.1"
#define WIFI_SUBNET      "255.255.255.0"
#define WIFI_DNS         "8.8.8.8"
```

### `wifi_manager.cpp` — Key steps in `initWiFi()`
1. `WiFi.mode(WIFI_STA)` — trigger Arduino's internal init
2. `WiFi.setAutoReconnect(false)` — disable Arduino's retry handler
3. `esp_netif_dhcpc_stop()` + `esp_netif_set_ip_info()` — set static IP
4. Scan for strongest BSSID — lock connection to best node
5. `esp_wifi_set_config()` with `bssid_set = true`, `pmf_cfg.capable = false`
6. `esp_wifi_connect()` — direct ESP-IDF call
7. Poll `esp_wifi_sta_get_ap_info()` for association

### `main.cpp` — Initialization order
```
nvs_flash_init()
initWiFi()          ← must be first, before Wire.begin()
initWebServer()
Wire.begin()
initDisplay()
initRTC()
initValves()
initRainSensor()
initScheduler()
initBLE()
```

---

## Open Issues

- **`Rainy` BSSID lock** — the static BSSID `A6:40:A0:66:E1:AD` is hardcoded via scan. If the master node changes its BSSID (e.g. after a router firmware update) the lock needs to be updated. Consider removing `bssid_set` once the connection is stable.
- **`CPE2G_2G1` incompatibility** — root cause not definitively identified. Likely a router firmware issue with the ESP32-C3's WiFi chipset. No software workaround found.
- **Static IP conflict risk** — `10.0.0.45` must remain outside the router's DHCP pool. Verify the DHCP range in the `Rainy` router admin panel.
- **WiFi dropout recovery** — `isWiFiConnected()` checks association state but there is no automatic reconnect logic if WiFi drops during operation. This should be added to `loop()` for production use.
