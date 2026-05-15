#include "wifi_manager.h"
#include "config.h"
#include "../web_server/web_server.h"
#include <WiFi.h>
#include <esp_wifi.h>

static bool wifiConnected = false;
static bool wasConnected  = false;

void initWiFi() {
    //esp_log_level_set("wifi", ESP_LOG_VERBOSE);   // ← add as very first line
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);

    // ── Force WPA2-only association ───────────────────────────────────────────
    // The CPE210 advertises WPA-PSK/WPA2-PSK mixed mode. The ESP32 stack
    // fails silently (status 6) when it sees the WPA3/PMF flags in the beacon.
    // Setting threshold.authmode = WIFI_AUTH_WPA2_PSK and disabling PMF forces
    // a clean WPA2-only handshake regardless of what the AP beacon advertises.
    wifi_config_t wifi_config = {};
    memcpy(wifi_config.sta.ssid,     WIFI_SSID,     strlen(WIFI_SSID));
    memcpy(wifi_config.sta.password, WIFI_PASSWORD, strlen(WIFI_PASSWORD));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable    = false;
    wifi_config.sta.pmf_cfg.required   = false;
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    // ─────────────────────────────────────────────────────────────────────────

    const int MAX_ATTEMPTS = 3;

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        Serial.printf("WiFi: connecting to '%s' (attempt %d/%d)...\n",
                      WIFI_SSID, attempt, MAX_ATTEMPTS);

        WiFi.disconnect(true);
        delay(100);
        WiFi.begin();   // credentials already loaded via esp_wifi_set_config

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
                Serial.printf("\nWiFi: attempt %d timed out  status code: %d\n",
                              attempt, (int)WiFi.status());
                break;
            }
            delay(250);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            wasConnected  = true;
            Serial.printf("\nWiFi: connected — IP %s\n",
                          WiFi.localIP().toString().c_str());
            initWebServer();
            return;
        }
    }

    Serial.println("WiFi: all attempts failed — running in BLE-only mode");
    wifiConnected = false;
    wasConnected  = false;
}

// Call once per loop() iteration.
// Detects reconnection after dropout and restarts the web server.
void checkWiFiReconnect() {
    bool now = (WiFi.status() == WL_CONNECTED);

    if (now && !wasConnected) {
        wifiConnected = true;
        wasConnected  = true;
        Serial.printf("WiFi: reconnected — IP %s\n",
                      WiFi.localIP().toString().c_str());
        initWebServer();
    } else if (!now && wasConnected) {
        wifiConnected = false;
        wasConnected  = false;
        Serial.println("WiFi: connection lost — waiting for reconnect");
    }
}

bool isWiFiConnected() {
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    return wifiConnected;
}

String getWiFiIP() {
    if (!isWiFiConnected()) return "";
    return WiFi.localIP().toString();
}
