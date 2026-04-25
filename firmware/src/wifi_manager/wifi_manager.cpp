#include "wifi_manager.h"
#include "config.h"
#include <WiFi.h>

static bool wifiConnected = false;

void initWiFi() {
    Serial.printf("WiFi: connecting to '%s'...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println("WiFi: timed out — running in BLE-only mode");
            wifiConnected = false;
            return;
        }
        delay(500);
        Serial.print(".");
    }
    wifiConnected = true;
    Serial.printf("\nWiFi: connected — IP %s\n", WiFi.localIP().toString().c_str());
}

bool isWiFiConnected() {
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    return wifiConnected;
}

String getWiFiIP() {
    if (!isWiFiConnected()) return "";
    return WiFi.localIP().toString();
}
