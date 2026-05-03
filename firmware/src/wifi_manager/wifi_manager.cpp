#include "wifi_manager.h"
#include "config.h"
#include <WiFi.h>
#include <esp_wifi.h>

static bool wifiConnected = false;

void initWiFi() {
    Serial.printf("WiFi: connecting to '%s'...\n", WIFI_SSID);
    WiFi.persistent(false);

    const int MAX_ATTEMPTS = 3;
    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        Serial.printf("WiFi: attempt %d/%d\n", attempt, MAX_ATTEMPTS);

        // Full stop/start cycle to clear any stuck state
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_OFF);
        delay(500);
        WiFi.mode(WIFI_STA);
        delay(200);

        // Scan first to let radio settle
        int n = WiFi.scanNetworks();
        bool found = false;
        for (int i = 0; i < n; i++) {
            if (WiFi.SSID(i) == WIFI_SSID) {
                Serial.printf("WiFi: found '%s' RSSI=%d\n", WIFI_SSID, WiFi.RSSI(i));
                found = true;
                break;
            }
        }
        if (!found) {
            Serial.println("WiFi: AP not visible, retrying...");
            continue;
        }
        esp_wifi_set_ps(WIFI_PS_NONE);          // disable power saving     
        WiFi.setTxPower(WIFI_POWER_19_5dBm);    // maximum TX power
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - start > 10000) {
                Serial.printf("WiFi: attempt %d failed — status %d\n",
                    attempt, WiFi.status());
                break;
            }
            delay(500);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            Serial.printf("WiFi: connected — IP %s\n",
                WiFi.localIP().toString().c_str());
            return;
        }
    }

    Serial.println("WiFi: all attempts failed — BLE-only mode");
    wifiConnected = false;
}


bool isWiFiConnected() {
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    return wifiConnected;
}

String getWiFiIP() {
    if (!isWiFiConnected()) return "";
    return WiFi.localIP().toString();
}
