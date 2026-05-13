#include "display.h"
#include "config.h"
#include <Wire.h>
#include <U8g2lib.h>

// SH1106 — 1.3" OLED, 128x64, I2C address 0x3C
// U8G2_R0        = no rotation
// U8X8_PIN_NONE  = no reset pin
// HW_I2C         = use hardware I2C (Wire)
// _F_            = full framebuffer mode (draw entire frame then flush)
static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ── Internal helpers ──────────────────────────────────────────────────────────

// Draw text at pixel position using current font (size 1 = 6x8 font)
static void _drawStr(int x, int y, const char* text) {
    // U8g2 y is the baseline, not the top — add font ascent (7px for u8g2_font_5x7)
    u8g2.drawStr(x, y + 7, text);
}

// Draw a horizontal rule across the full width
static void _drawHLine(int y) {
    u8g2.drawHLine(0, y, 128);
}

// Draw 5-segment wetness bar identical to original layout
// Each segment: 5px wide, 6px tall, 1px gap
static void _drawWetnessBar(int x, int y, uint8_t level) {
    const int segW = 5, segH = 6, gap = 1;
    uint8_t filled = (level * 5 + 50) / 100;   // 0–5 filled segments
    for (uint8_t i = 0; i < 5; i++) {
        int sx = x + i * (segW + gap);
        if (i < filled) u8g2.drawBox(sx, y, segW, segH);
        else            u8g2.drawFrame(sx, y, segW, segH);
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void initDisplay() {
    // Wire must be initialised before U8g2
    Wire.begin(SDA_PIN, SCL_PIN);
    u8g2.begin();
    u8g2.setFont(u8g2_font_5x7_tr);    // 5x7 pixel font — same visual weight
                                        // as Adafruit size-1 font
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    Serial.println("Display: SH1106 initialised via U8g2");
}

// showText / clearArea kept for compatibility with any direct calls in main.cpp
void showText(int x, int y, const char* text, int size) {
    // size parameter kept for API compatibility — U8g2 font is fixed
    // for a different size, swap the font before calling
    (void)size;
    u8g2.setFont(u8g2_font_5x7_tr);
    _drawStr(x, y, text);
    u8g2.sendBuffer();
}

void showText(int x, int y, String text, int size) {
    showText(x, y, text.c_str(), size);
}

void clearArea(int x, int y, int w, int h) {
    u8g2.setDrawColor(0);               // draw in black
    u8g2.drawBox(x, y, w, h);
    u8g2.setDrawColor(1);               // restore white
    u8g2.sendBuffer();
}

// ── Main status screen ────────────────────────────────────────────────────────
//
// Layout (128×64):
//   Y= 0  "Watering System"                        HH:MM
//   Y=10  ────────────────────────────────────────────────
//   Y=14  "WiFi: 192.168.1.42"  OR  "BLE:  Connected/Adv"
//   Y=24  "Rain: No  [████░]  42%"
//   Y=34  "V1: OPEN/closed       V2: OPEN/closed"
//   Y=44  ────────────────────────────────────────────────
//   Y=48  <IP address>  OR  "BLE only - no WiFi"

void updateDisplayStatus(bool bleConnected,
                         bool wifiConnected, const char* wifiIP,
                         bool rain, uint8_t rainLevel,
                         bool valve1, bool valve2,
                         const char* timeStr) {

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.setDrawColor(1);

    // ── Title + clock ─────────────────────────────────────
    _drawStr(0,  0, "Watering System");
    _drawStr(98, 0, timeStr);
    _drawHLine(10);

    // ── Network row — WiFi takes priority ─────────────────
    if (wifiConnected) {
        _drawStr(0,  14, "WiFi:");
        _drawStr(36, 14, wifiIP);
    } else {
        _drawStr(0,  14, "BLE: ");
        _drawStr(36, 14, bleConnected ? "Connected   " : "Advertising ");
    }

    // ── Rain row — digital flag + wetness bar + percentage ─
    _drawStr(0,  24, "Rain:");
    _drawStr(36, 24, rain ? "YES " : "No  ");
    _drawWetnessBar(60, 24, rainLevel);
    char pct[6];
    snprintf(pct, sizeof(pct), "%3d%%", rainLevel);
    _drawStr(94, 24, pct);

    // ── Valve row ─────────────────────────────────────────
    _drawStr(0,  34, "V1:");
    _drawStr(20, 34, valve1 ? "OPEN  " : "closed");
    _drawStr(68, 34, "V2:");
    _drawStr(88, 34, valve2 ? "OPEN  " : "closed");

    _drawHLine(44);

    // ── Bottom hint ───────────────────────────────────────
    if (wifiConnected) {
        _drawStr(0, 48, wifiIP);
    } else {
        _drawStr(0, 48, "BLE only - no WiFi");
    }

    u8g2.sendBuffer();
}
