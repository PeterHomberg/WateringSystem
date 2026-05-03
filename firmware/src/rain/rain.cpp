#include "rain.h"
#include "config.h"

// ── Internal state ────────────────────────────────────────────────────────────
static bool    rainDetected = false;   // from digital pin
static uint8_t rainLevel    = 0;       // 0–100, from analog pin
static bool    simMode      = false;
static bool    simState     = false;

// ── ADC helpers ───────────────────────────────────────────────────────────────
// The ESP32-C3 ADC is noisy. We take several samples and average them.
static int readAnalogAvg(uint8_t pin, uint8_t samples = 8) {
    long sum = 0;
    for (uint8_t i = 0; i < samples; i++) {
        sum += analogRead(pin);
        delay(2);
    }
    return (int)(sum / samples);
}

// Map raw ADC value to wetness percentage 0–100.
// Raw value is INVERSELY proportional to wetness:
//   ~4095 = dry (0%)
//   ~0    = soaking (100%)
// We clamp to the calibrated thresholds defined in config.h.
static uint8_t adcToWetness(int raw) {
    if (raw >= RAIN_ANALOG_DRY_THRESHOLD) return 0;
    if (raw <= RAIN_ANALOG_WET_THRESHOLD) return 100;
    // Linear interpolation between the two thresholds
    int   range  = RAIN_ANALOG_DRY_THRESHOLD - RAIN_ANALOG_WET_THRESHOLD;
    int   offset = RAIN_ANALOG_DRY_THRESHOLD - raw;
    return (uint8_t)((offset * 100) / range);
}

// ── Public API ────────────────────────────────────────────────────────────────

void initRainSensor() {
    pinMode(RAIN_PIN_DIGITAL, INPUT);
    // Analog pin — analogRead() works without explicit pinMode on ESP32,
    // but setting INPUT keeps it explicit.
    pinMode(RAIN_PIN_ANALOG, INPUT);

    simMode      = false;
    simState     = false;
    rainDetected = false;
    rainLevel    = 0;

    Serial.println("Rain sensor initialized (digital GPIO1, analog GPIO0)");
}

bool oldRainDetected = false;
int oldRaw = 0;
uint8_t oldRainLevel =0;

void updateRainSensor() {
    if (simMode) {
        rainDetected = simState;
        rainLevel    = simState ? 80 : 0;
        return;
    }

    // Digital — active LOW
    rainDetected = (digitalRead(RAIN_PIN_DIGITAL) == LOW);

    // Analog — averaged and mapped to 0–100
    int raw  = readAnalogAvg(RAIN_PIN_ANALOG);
    rainLevel = adcToWetness(raw);

    if(rainDetected != oldRainDetected || raw != oldRaw || rainLevel != oldRainLevel)
    {
        oldRainDetected = rainDetected;
        oldRainLevel = rainLevel;
        oldRaw = raw;
        Serial.printf("Rain sensor — digital: %s  analog raw: %d  level: %d%%\n",
            rainDetected ? "RAIN" : "dry", raw, rainLevel);

    }
}

bool isRaining() {
    return rainDetected;
}

uint8_t getRainLevel() {
    return rainLevel;
}

void simulateRain(bool raining) {
    simMode  = true;
    simState = raining;
    // Apply immediately so callers don't have to wait for updateRainSensor()
    rainDetected = raining;
    rainLevel    = raining ? 80 : 0;
}
