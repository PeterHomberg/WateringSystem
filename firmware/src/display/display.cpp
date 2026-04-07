#include "display.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, 
                                 &Wire, OLED_RESET);

static void sendOLEDCommand(uint8_t cmd) {
    Wire.beginTransmission(SCREEN_ADDRESS);
    Wire.write(0x00);
    Wire.write(cmd);
    Wire.endTransmission();
}

void initDisplay() {
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("Display not found!");
        while (true);
    }
    sendOLEDCommand(0xA1);  // fix horizontal mirror (SSD1312 mounting quirk)
    sendOLEDCommand(0xC0);  // COM scan normal — top to bottom
    display.clearDisplay();
    display.display();
}

void showText(int x, int y, const char* text, int size) {
    display.setTextSize(size);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(x, y);
    display.print(text);
}

void showText(int x, int y, String text, int size) {
    showText(x, y, text.c_str(), size);
}

void clearArea(int x, int y, int w, int h) {
    display.fillRect(x, y, w, h, SSD1306_BLACK);
    display.display();
}

// Display layout (128×64):
//
//   Y= 0  "Watering System"        +  "HH:MM" right-aligned
//   Y=10  ─────────────────────────────────────── divider
//   Y=14  "BLE: Connected / Advertising"
//   Y=24  "Rain: No / YES - inhibit"
//   Y=34  "V1: OPEN/closed    V2: OPEN/closed"
//   Y=44  ─────────────────────────────────────── divider
//   Y=48  "Use nRF Connect"
//
// The time is shown in the top-right corner (size 1 = 6px/char, "HH:MM" = 30px wide).
// X start = 128 - 5*6 = 98.

void updateDisplayStatus(bool connected, bool rain,
                         bool valve1, bool valve2,
                         const char* timeStr) {
    display.clearDisplay();

    // Title row
    showText(0, 0, "Watering System", 1);

    // Time — right-aligned in title row
    // "HH:MM" is 5 chars × 6px = 30px wide at size 1
    showText(98, 0, timeStr, 1);

    // Divider
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // BLE status
    showText(0,  14, "BLE:");
    showText(30, 14, connected ? "Connected   " : "Advertising ");

    // Rain status
    showText(0,  24, "Rain:");
    showText(36, 24, rain ? "YES - inhibit" : "No          ");

    // Valve states
    showText(0,  34, "V1:");
    showText(20, 34, valve1 ? "OPEN  " : "closed");
    showText(68, 34, "V2:");
    showText(88, 34, valve2 ? "OPEN  " : "closed");

    // Lower divider
    display.drawLine(0, 44, 127, 44, SSD1306_WHITE);

    // Hint line
    showText(0, 48, "Use nRF Connect");

    display.display();
}
