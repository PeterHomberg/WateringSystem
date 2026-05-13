#pragma once
#include <Arduino.h>

// Attempts to connect using credentials in config.h.
// Blocks until connected or WIFI_CONNECT_TIMEOUT_MS — then returns.
// System continues in BLE-only mode if connection fails.
// Auto-reconnect is enabled — call checkWiFiReconnect() from loop().
void initWiFi();

// Call from loop() — restarts web server automatically on reconnect.
void checkWiFiReconnect();

bool   isWiFiConnected();
String getWiFiIP();
