#pragma once
#include <Arduino.h>

void initDisplay();
void showText(int x, int y, const char* text, int size = 1);
void showText(int x, int y, String text, int size = 1);
void clearArea(int x, int y, int w, int h);

// Main status screen — now includes time string from RTC.
// timeStr: "HH:MM" or "--:--" if RTC not set
void updateDisplayStatus(bool connected, bool rain,
                         bool valve1, bool valve2,
                         const char* timeStr = "--:--");
