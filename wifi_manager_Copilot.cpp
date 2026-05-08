// created by Copilot

#define USE_STATIC_IP

#include "wifi_manager.h"
#include "config.h"

#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_netif.h"

static esp_netif_t* s_staNetif = nullptr;
static bool s_connected = false;

// ---------------------------------------------------------------------------
// Helper: return IP as string
// ---------------------------------------------------------------------------
String getWiFiIP() {
    if (WiFi.status() != WL_CONNECTED) return "";
    return WiFi.localIP().toString();
}

// ---------------------------------------------------------------------------
// Helper: check connection
// ---------------------------------------------------------------------------
bool isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

// ---------------------------------------------------------------------------
// Main WiFi initialization
// ---------------------------------------------------------------------------
void initWiFi() {
    Serial.printf("WiFi: connecting to '%s'\n", WIFI_SSID);
    s_connected = false;

    // Basic WiFi setup
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);          // Important for ESP32‑C3 stability
    WiFi.setAutoReconnect(true);
    WiFi.disconnect(true);
    delay(300);

    // Optional: set country (fixes channel mismatch issues)
    wifi_country_t country = {
        .cc = "CN",
        .schan = 1,
        .nchan = 13,
        .policy = WIFI_COUNTRY_POLICY_AUTO
    };
    esp_wifi_set_country(&country);
    // -----------------------------------------------------------------------
    // Scan for strongest BSSID
    // -----------------------------------------------------------------------
    Serial.println("WiFi: scanning...");
    int n = WiFi.scanNetworks();
    uint8_t bestBSSID[6];
    int bestRSSI = -999;
    bool found = false;

    for (int i = 0; i < n; i++) {
        if (WiFi.SSID(i) == WIFI_SSID) {
            int rssi = WiFi.RSSI(i);
            Serial.printf("  found RSSI=%d BSSID=%s\n",
                rssi, WiFi.BSSIDstr(i).c_str());

            if (rssi > bestRSSI) {
                bestRSSI = rssi;
                memcpy(bestBSSID, WiFi.BSSID(i), 6);
                found = true;
            }
        }
    }
    WiFi.scanDelete();

    if (!found) {
        Serial.println("WiFi: AP not found");
        return;
    }

    // -----------------------------------------------------------------------
    // Build WiFi config
    // -----------------------------------------------------------------------
    wifi_config_t cfg = {};
    strncpy((char*)cfg.sta.ssid, WIFI_SSID, sizeof(cfg.sta.ssid) - 1);
    strncpy((char*)cfg.sta.password, WIFI_PASSWORD, sizeof(cfg.sta.password) - 1);

    cfg.sta.bssid_set = true;
    memcpy(cfg.sta.bssid, bestBSSID, 6);

    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    cfg.sta.pmf_cfg.capable = false;
    cfg.sta.pmf_cfg.required = false;

    esp_wifi_set_config(WIFI_IF_STA, &cfg);

    // -----------------------------------------------------------------------
    // DHCP or static IP
    // -----------------------------------------------------------------------
    s_staNetif = esp_netif_next_unsafe(nullptr);

#ifdef USE_STATIC_IP
    {
        esp_netif_ip_info_t ipInfo;
        ipInfo.ip.addr      = esp_ip4addr_aton(STATIC_IP);
        ipInfo.gw.addr      = esp_ip4addr_aton(GATEWAY_IP);
        ipInfo.netmask.addr = esp_ip4addr_aton(NETMASK_IP);

        esp_netif_dhcpc_stop(s_staNetif);
        esp_netif_set_ip_info(s_staNetif, &ipInfo);

        Serial.printf("WiFi: using static IP %s\n", STATIC_IP);
    }
#else
    {
        esp_netif_dhcpc_start(s_staNetif);
        Serial.println("WiFi: DHCP enabled");
    }
#endif

    // -----------------------------------------------------------------------
    // Connect
    // -----------------------------------------------------------------------
    Serial.println("WiFi: connecting...");
    esp_wifi_connect();

    // Wait for IP
    unsigned long start = millis();
    while (millis() - start < 12000) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("WiFi: connected — IP %s\n",
                WiFi.localIP().toString().c_str());
            s_connected = true;
            return;
        }
        delay(300);
        Serial.print(".");
    }

    Serial.println("\nWiFi: connection failed");
}
