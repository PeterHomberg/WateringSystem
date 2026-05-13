#include "wifi_manager.h"
#include "config.h"
#include "../web_server/web_server.h"
#include <WiFi.h>

static bool wifiConnected  = false;
static bool wasConnected   = false;   // tracks previous state for reconnect detection

void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);      // S3 RF stack handles reconnect reliably
    WiFi.persistent(false);           // don't write credentials to NVS on every connect

    Serial.printf("WiFi: connecting to '%s'...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println("WiFi: timed out — running in BLE-only mode");
            wifiConnected = false;
            wasConnected  = false;
            return;
        }
        delay(250);
        Serial.print(".");
    }

    wifiConnected = true;
    wasConnected  = true;
    Serial.printf("\nWiFi: connected — IP %s\n", WiFi.localIP().toString().c_str());

    // Start web server on first successful connection
    initWebServer();
}

// Call once per loop() iteration.
// Detects reconnection after dropout and restarts the web server.
void checkWiFiReconnect() {
    bool now = (WiFi.status() == WL_CONNECTED);

    if (now && !wasConnected) {
        // Just reconnected
        wifiConnected = true;
        wasConnected  = true;
        Serial.printf("WiFi: reconnected — IP %s\n", WiFi.localIP().toString().c_str());
        initWebServer();    // restart AsyncWebServer on the new connection
    } else if (!now && wasConnected) {
        // Just lost connection
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
