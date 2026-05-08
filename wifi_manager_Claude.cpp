#include "wifi_manager.h"
#include "config.h"

#include "esp_wifi.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <WiFi.h>

static esp_netif_t* s_staNetif  = nullptr;
static bool         s_connected = false;

void initWiFi() {
    Serial.printf("WiFi: connecting to '%s'\n", WIFI_SSID);
    s_connected = false;

    // Let Arduino init the WiFi stack fully
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false);   // ← false, not true
    delay(300);
    s_staNetif = esp_netif_next_unsafe(nullptr);
    if (s_staNetif == nullptr) {
        Serial.println("WiFi: no netif — aborting");
        return;
    }

    const int MAX_ATTEMPTS = 5;
    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        Serial.printf("WiFi: attempt %d/%d\n", attempt, MAX_ATTEMPTS);

        // Scan for strongest BSSID
        int n = WiFi.scanNetworks();
        uint8_t bssid[6];
        int bestRSSI = -999;
        bool found = false;
        for (int i = 0; i < n; i++) {
            if (WiFi.SSID(i) == WIFI_SSID) {
                int rssi = WiFi.RSSI(i);
                Serial.printf("  found RSSI=%d BSSID=%s encType=%d\n",
                    rssi, WiFi.BSSIDstr(i).c_str(), WiFi.encryptionType(i));
                if (rssi > bestRSSI) {
                    bestRSSI = rssi;
                    memcpy(bssid, WiFi.BSSID(i), 6);
                    found = true;
                }
            }
        }
        WiFi.scanDelete();

        if (!found) {
            Serial.println("  AP not found");
            delay(1000);
            continue;
        }

        // Configure with BSSID lock — try all authmode combinations
        wifi_config_t cfg = {};
        strncpy((char*)cfg.sta.ssid,     WIFI_SSID,     sizeof(cfg.sta.ssid)     - 1);
        strncpy((char*)cfg.sta.password, WIFI_PASSWORD, sizeof(cfg.sta.password) - 1);
        cfg.sta.threshold.authmode  = WIFI_AUTH_OPEN;
        cfg.sta.pmf_cfg.capable     = false;
        cfg.sta.pmf_cfg.required    = false;
        cfg.sta.sae_pwe_h2e         = WPA3_SAE_PWE_UNSPECIFIED;
        cfg.sta.bssid_set           = true;
        memcpy(cfg.sta.bssid, bssid, 6);

        // Set config without stop/start — keep the netif binding intact
        esp_wifi_disconnect();
        delay(200);
        esp_wifi_set_config(WIFI_IF_STA, &cfg);
        delay(100);
        // Try static IP to bypass DHCP
        esp_netif_ip_info_t ipInfo;
        ipInfo.ip.addr      = esp_ip4addr_aton("10.0.0.45");
        ipInfo.gw.addr      = esp_ip4addr_aton("10.0.0.1");
        ipInfo.netmask.addr = esp_ip4addr_aton("255.255.255.0");
        esp_netif_dhcpc_stop(s_staNetif);   // stop DHCP client
        esp_netif_set_ip_info(s_staNetif, &ipInfo);

        esp_err_t err = esp_wifi_connect();
        Serial.printf("  esp_wifi_connect() = 0x%x\n", err);

        // Poll for IP
        unsigned long start = millis();
        bool success = false;
        while (millis() - start < 12000) {
            esp_netif_ip_info_t ipInfo;
            if (esp_netif_get_ip_info(s_staNetif, &ipInfo) == ESP_OK
                    && ipInfo.ip.addr != 0) {
                success = true;
                break;
            }
            delay(500);
            wifi_ap_record_t ap;
            Serial.print(esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? "." : "x");
        }
        Serial.println();

        if (success) {
            s_connected = true;
            Serial.printf("WiFi: connected — IP %s\n", getWiFiIP().c_str());
            return;
        }

        Serial.printf("  attempt %d failed\n", attempt);
        esp_wifi_disconnect();
        delay(500);
    }

    Serial.println("WiFi: all attempts failed — BLE-only mode");
}

bool isWiFiConnected() {
    if (s_staNetif == nullptr) return false;
    esp_netif_ip_info_t ipInfo;
    if (esp_netif_get_ip_info(s_staNetif, &ipInfo) != ESP_OK) return false;
    s_connected = (ipInfo.ip.addr != 0);
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
