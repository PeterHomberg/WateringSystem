#pragma once
#include <Arduino.h>

void initValves();
void openValve(int valve);
void closeValve(int valve);
void closeAllValves();
bool isValveOpen(int valve);

