#pragma once
#include <Arduino.h>

// WiFi manager using ESP-IDF API directly — bypasses Arduino WiFi library
// entirely to avoid state machine issues on ESP32-C3.

void initWiFi();
bool   isWiFiConnected();
String getWiFiIP();
