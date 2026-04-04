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
    sendOLEDCommand(0xA1);
    sendOLEDCommand(0xC0);
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

void updateDisplayStatus(bool connected, bool rain,
                         bool valve1, bool valve2) {
    display.clearDisplay();
    showText(0,  0, "Watering System", 1);
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
    showText(0, 14, "BLE:");
    showText(30, 14, connected ? "Connected   " : "Advertising ");
    showText(0, 26, "Rain:");
    showText(36, 26, rain ? "YES - inhibit" : "No          ");
    showText(0, 38, "V1:");
    showText(20, 38, valve1 ? "OPEN " : "closed");
    showText(64, 38, "V2:");
    showText(84, 38, valve2 ? "OPEN " : "closed");
    showText(0, 52, "Use nRF Connect");
    display.display();
}