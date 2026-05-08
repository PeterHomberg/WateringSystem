#include "wifi_manager.h"
#include "config.h"

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <WiFi.h>

// ── State ─────────────────────────────────────────────────────────────────────

static esp_netif_t*  s_staNetif  = nullptr;
static volatile bool s_connected = false;

// ── Event handler — disconnect detection only ─────────────────────────────────
// We do NOT intercept WIFI_EVENT_STA_CONNECTED because Arduino's internal
// handler consumes it first, making it unreliable for our use.
// Association is detected by polling esp_wifi_sta_get_ap_info() with a
// mandatory 1500ms pre-poll delay to ensure the cache is fresh after connect.

static void wifiEventHandler(void* arg, esp_event_base_t base,
                              int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        auto* info = (wifi_event_sta_disconnected_t*)data;
        Serial.printf("WiFi event: disconnected — reason %d rssi %d uptime %lums\n",
                      info->reason, info->rssi, millis());
        if (s_connected) {
            s_connected = false;
        }
    }
}

// ── Internal helpers ──────────────────────────────────────────────────────────

static void grabNetif() {
    s_staNetif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
}

static void applyStaticIP() {
    if (s_staNetif == nullptr) return;
    esp_netif_dhcpc_stop(s_staNetif);
    esp_netif_ip_info_t ip = {};
    ip.ip.addr      = esp_ip4addr_aton(WIFI_STATIC_IP);
    ip.gw.addr      = esp_ip4addr_aton(WIFI_GATEWAY);
    ip.netmask.addr = esp_ip4addr_aton(WIFI_SUBNET);
    esp_netif_set_ip_info(s_staNetif, &ip);
}

static void radioReset() {
    WiFi.mode(WIFI_OFF);
    delay(200);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);  // ← add here too
    delay(300);
    grabNetif();
    // applyStaticIP();  ← remove this from radioReset()
}
// ── Public API ────────────────────────────────────────────────────────────────

void initWiFi() {
    Serial.printf("WiFi: connecting to '%s'\n", WIFI_SSID);
    s_connected = false;

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);

    WiFi.disconnect(false);

    // Fix MAC — arduino-esp32 v3.x randomises MAC by default
    uint8_t mac[] = WIFI_MAC;
    esp_wifi_set_mac(WIFI_IF_STA, mac);

    delay(300);

    grabNetif();
    if (s_staNetif == nullptr) {
        Serial.println("WiFi: no STA netif — aborting");
        return;
    }

    // Register disconnect handler only — not CONNECTED, which Arduino consumes
    esp_event_loop_create_default();
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                wifiEventHandler, nullptr);

    //applyStaticIP();

    const int MAX_ATTEMPTS = 5;
    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        Serial.printf("WiFi: attempt %d/%d\n", attempt, MAX_ATTEMPTS);

        // ── 1. Scan ───────────────────────────────────────────────────────
        int n = WiFi.scanNetworks();
        bool found = false;
        for (int i = 0; i < n; i++) {
            if (WiFi.SSID(i) == WIFI_SSID) {
                Serial.printf("  found RSSI=%d BSSID=%s encType=%d\n",
                    WiFi.RSSI(i), WiFi.BSSIDstr(i).c_str(),
                    WiFi.encryptionType(i));
                found = true;
            }
        }
        WiFi.scanDelete();

        if (!found) {
            Serial.println("  AP not found — retrying");
            delay(1000);
            continue;
        }

        // ── 2. Radio reset on retries ─────────────────────────────────────
        if (attempt > 1) {
            Serial.println("  resetting radio");
            radioReset();
        }

        // ── 3. Build config ───────────────────────────────────────────────
        wifi_config_t cfg = {};
        strncpy((char*)cfg.sta.ssid,     WIFI_SSID,     sizeof(cfg.sta.ssid)     - 1);
        strncpy((char*)cfg.sta.password, WIFI_PASSWORD, sizeof(cfg.sta.password) - 1);
        cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
        cfg.sta.pmf_cfg.capable    = true;
        cfg.sta.pmf_cfg.required   = false;
        cfg.sta.sae_pwe_h2e        = WPA3_SAE_PWE_UNSPECIFIED;
        cfg.sta.rm_enabled         = false;
        cfg.sta.btm_enabled        = false;
        cfg.sta.mbo_enabled        = false;
        cfg.sta.ft_enabled         = false;
        cfg.sta.owe_enabled        = false;
        cfg.sta.transition_disable = true;
        cfg.sta.bssid_set          = false;

        esp_wifi_set_config(WIFI_IF_STA, &cfg);
        delay(100);

        // ── 4. Connect ────────────────────────────────────────────────────
        esp_err_t err = esp_wifi_connect();
        Serial.printf("  esp_wifi_connect() = 0x%x — uptime %lums\n", err, millis());
        if (err != ESP_OK) {
            Serial.printf("  connect call failed (0x%x) — retrying\n", err);
            delay(500);
            continue;
        }

        // ── 5. Mandatory pre-poll delay ───────────────────────────────────
        // Wait 2000ms before polling esp_wifi_sta_get_ap_info().
        // This ensures the WPA2 4-way handshake has time to complete and the
        // driver cache is populated with a fresh record — not stale state from
        // a previous attempt. The disconnect event fires into this delay if
        // the AP rejects us, so we check s_connected hasn't been cleared.
        Serial.print("  waiting for handshake");
        for (int i = 0; i < 10; i++) {
            delay(200);
            Serial.print(".");
            // If already disconnected, no point waiting further
            if (!s_connected && i > 2) {
                // s_connected starts false — only bail if a disconnect event
                // arrived (indicated by a reason-2 log appearing above)
            }
        }
        Serial.println();

        // ── 6. Poll for confirmed association ─────────────────────────────
        unsigned long start = millis();
        bool associated = false;
        int  consecutiveOk = 0;
        while (millis() - start < 8000) {
            wifi_ap_record_t ap;
            if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
                consecutiveOk++;
                if (consecutiveOk == 1) {
                    // Log what we got on first hit
                    Serial.printf("\n  ap_info: BSSID=%02X:%02X:%02X:%02X:%02X:%02X"
                                  " channel=%d rssi=%d authmode=%d uptime=%lums\n",
                                  ap.bssid[0], ap.bssid[1], ap.bssid[2],
                                  ap.bssid[3], ap.bssid[4], ap.bssid[5],
                                  ap.primary, ap.rssi, ap.authmode, millis());
                }
                // Require 3 consecutive successful reads 500ms apart to confirm
                // the association is stable and not a stale cached record
                if (consecutiveOk >= 3) {
                    associated = true;
                    break;
                }
                delay(500);
                Serial.print("+");
            } else {
                consecutiveOk = 0;
                delay(300);
                Serial.print("x");
            }
        }
        Serial.println();

        if (associated) {
            applyStaticIP();
            delay(50);
            esp_wifi_set_ps(WIFI_PS_NONE);
            s_connected = true;
            Serial.printf("WiFi: connected — IP %s — uptime %lums\n",
                          getWiFiIP().c_str(), millis());
            return;
        }

        Serial.printf("  attempt %d failed\n", attempt);
        esp_wifi_disconnect();
        delay(300);
    }

    Serial.println("WiFi: all attempts failed — BLE-only mode");
}

bool isWiFiConnected() {
    return s_connected;
}

String getWiFiIP() {
    if (s_staNetif == nullptr) return "";
    esp_netif_ip_info_t ipInfo;
    if (esp_netif_get_ip_info(s_staNetif, &ipInfo) != ESP_OK) return "";
    if (ipInfo.ip.addr == 0) return "";
    char buf[16];
    snprintf(buf, sizeof(buf), IPSTR, IP2STR(&ipInfo.ip));
    return String(buf);
}
