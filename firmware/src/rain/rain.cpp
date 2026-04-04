#include "rain.h"
#include "config.h"

static bool rainState   = false;
static bool simMode     = false;
static bool simState    = false;

void initRainSensor() {
    pinMode(RAIN_PIN, INPUT);
    Serial.println("Rain sensor initialized");
}

void simulateRain(bool raining) {
    simMode  = true;
    simState = raining;
    rainState = simState;
}

void updateRainSensor() {
    if (simMode) {
        rainState = simState;  // use simulated value
    } else {
        rainState = (digitalRead(RAIN_PIN) == LOW);
    }
}

bool isRaining() {
    return rainState;
}