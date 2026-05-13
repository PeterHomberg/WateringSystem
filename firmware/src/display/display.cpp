#include "display.h"
#include "config.h"
#include <Wire.h>

// TODO: SH1106 display rewrite pending (migration step 2)
// Adafruit SSD1306 removed — U8g2 driver to be implemented

void initDisplay() {
    Serial.println("Display: stub — SH1106 rewrite pending");
}

void showText(int x, int y, const char* text, int size) {}
void showText(int x, int y, String text, int size) {}
void clearArea(int x, int y, int w, int h) {}

void updateDisplayStatus(bool bleConnected,
                         bool wifiConnected, const char* wifiIP,
                         bool rain, uint8_t rainLevel,
                         bool valve1, bool valve2,
                         const char* timeStr) {
    // stub — no display output until SH1106 driver is implemented
}
