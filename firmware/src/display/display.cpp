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
//   Y= 0  "Watering System"              HH:MM (right-aligned)
//   Y=10  ──────────────────────────────────────
//   Y=14  "BLE: Connected / Advertising"
//   Y=24  "Rain: No / YES  [███░░] 42%"
//   Y=34  "V1: OPEN/closed    V2: OPEN/closed"
//   Y=44  ──────────────────────────────────────
//   Y=48  "Use nRF Connect"

static void drawWetnessBar(int x, int y, uint8_t level) {
    // 5-segment bar, each segment 5px wide × 6px tall, 1px gap between
    // Total width = 5*5 + 4*1 = 29px
    const int segW = 5, segH = 6, gap = 1;
    uint8_t filled = (level * 5 + 50) / 100;  // 0–5 segments
    for (uint8_t i = 0; i < 5; i++) {
        int sx = x + i * (segW + gap);
        if (i < filled) {
            display.fillRect(sx, y, segW, segH, SSD1306_WHITE);
        } else {
            display.drawRect(sx, y, segW, segH, SSD1306_WHITE);
        }
    }
}

void updateDisplayStatus(bool connected, bool rain, uint8_t rainLevel,
                         bool valve1, bool valve2,
                         const char* timeStr) {
    display.clearDisplay();

    // Title + time
    showText(0, 0, "Watering System", 1);
    showText(98, 0, timeStr, 1);

    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // BLE
    showText(0,  14, "BLE:");
    showText(30, 14, connected ? "Connected   " : "Advertising ");

    // Rain — digital status + analog bar + percentage
    showText(0, 24, "Rain:");
    if (rain) {
        showText(36, 24, "YES ");
    } else {
        showText(36, 24, "No  ");
    }
    drawWetnessBar(60, 24, rainLevel);   // bar at x=60
    char pctBuf[6];
    snprintf(pctBuf, sizeof(pctBuf), "%3d%%", rainLevel);
    showText(94, 24, pctBuf);           // "42%" at x=94

    // Valves
    showText(0,  34, "V1:");
    showText(20, 34, valve1 ? "OPEN  " : "closed");
    showText(68, 34, "V2:");
    showText(88, 34, valve2 ? "OPEN  " : "closed");

    display.drawLine(0, 44, 127, 44, SSD1306_WHITE);

    showText(0, 48, "Use nRF Connect");

    display.display();
}
