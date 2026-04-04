#pragma once
#include <Arduino.h>

void initRainSensor();
bool isRaining();
void updateRainSensor();
void simulateRain(bool raining);  // ← add this