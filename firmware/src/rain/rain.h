#pragma once
#include <Arduino.h>

// ── Rain sensor module ────────────────────────────────────────────────────────
// Supports both outputs of a two-output rain sensor:
//
//   Digital output (GPIO1, active LOW):
//     LOW  = at least one droplet detected
//     HIGH = dry
//
//   Analog output (GPIO0 / ADC1_0):
//     Raw 12-bit ADC value, high when dry (~4095), falls as surface gets wetter.
//     Exposed as a 0–100 wetness percentage: 0 = dry, 100 = soaking wet.

void initRainSensor();

// Read both inputs and update internal state.
// Call from loop() every 2 seconds.
void updateRainSensor();

// Returns true if the digital output reports rain (active LOW triggered).
bool isRaining();

// Returns wetness level 0–100 from the analog output.
// 0 = completely dry, 100 = maximum wetness.
uint8_t getRainLevel();

// Simulation — overrides both digital and analog readings.
// Calling with raining=true sets digital=raining, level=80.
// Calling with raining=false sets digital=dry, level=0.
// Simulation mode is cleared by calling initRainSensor() again.
void simulateRain(bool raining);
