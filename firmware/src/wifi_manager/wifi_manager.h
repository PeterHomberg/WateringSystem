#pragma once
#include <Arduino.h>

// Attempts to connect using credentials in config.h.
// Blocks until connected or WIFI_CONNECT_TIMEOUT_MS — then returns.
// System continues in BLE-only mode if connection fails.
void initWiFi();

bool   isWiFiConnected();
String getWiFiIP();
