#pragma once
#include <Arduino.h>

void initDisplay();
void showText(int x, int y, const char* text, int size = 1);
void showText(int x, int y, String text, int size = 1);
void clearArea(int x, int y, int w, int h);

// wifiConnected: true  → show IP on network row and bottom hint
// wifiIP:        IP string e.g. "192.168.1.42", or "" when not connected
// rainLevel:     0–100 from analog sensor
void updateDisplayStatus(bool bleConnected,
                         bool wifiConnected, const char* wifiIP,
                         bool rain, uint8_t rainLevel,
                         bool valve1, bool valve2,
                         const char* timeStr = "--:--");
