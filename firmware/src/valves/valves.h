#pragma once
#include <Arduino.h>

void initValves();
void openValve(int valve);
void closeValve(int valve);
void closeAllValves();
bool isValveOpen(int valve);

#define OPEN 0x0
#define CLOSE 0x1