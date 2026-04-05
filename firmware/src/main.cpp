#include <Arduino.h>
#include <Wire.h>
#include <nvs_flash.h>
#include "config.h"
#include "display/display.h"
#include "ble/ble_server.h"
#include "valves/valves.h"
#include "rain/rain.h"
#include "scheduler/scheduler.h"

void setup() {
    Serial.begin(115200);

    // Initialize NVS — required for BLE stack and Preferences (scheduler flash)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    Serial.println("NVS initialized");

    Wire.begin(SDA_PIN, SCL_PIN);
    initDisplay();
    initValves();
    initRainSensor();
    initScheduler();   // ← loads persisted schedule from flash
    initBLE();

    updateDisplayStatus(isBLEConnected(), isRaining(),
                        isValveOpen(1), isValveOpen(2));

    Serial.println("Watering system started");
}

void loop() {
    // Simulate rain toggling every 10 seconds for testing
    static unsigned long lastRainToggle = 0;
    static bool simRaining = false;
    if (millis() - lastRainToggle > 10000) {
        lastRainToggle = millis();
        simRaining = !simRaining;
        simulateRain(simRaining);
        Serial.printf("Rain simulated: %s\n", simRaining ? "YES" : "No");
        updateBLEStatus();
    }

    // Update rain sensor every 2 seconds
    static unsigned long lastRainCheck = 0;
    if (millis() - lastRainCheck > 2000) {
        lastRainCheck = millis();
        updateRainSensor();
    }

    // Check schedule every minute
    // TODO: replace with real DS3231 time once RTC module is implemented
    // When RTC is ready, replace the lines below with:
    //   DateTime now = rtc.now();
    //   uint8_t weekday = now.dayOfTheWeek();  // DS3231: 0=Sun … map to 0=Mon if needed
    //   checkSchedule(weekday, now.hour(), now.minute());
    static unsigned long lastScheduleCheck = 0;
    if (millis() - lastScheduleCheck > 60000) {
        lastScheduleCheck = millis();
        // Placeholder — scheduler tick does nothing without real time yet
        // checkSchedule(weekday, hour, minute);
        Serial.println("Schedule tick — waiting for RTC");
    }

    // Update display every second
    static unsigned long lastDisplay = 0;
    if (millis() - lastDisplay > 1000) {
        lastDisplay = millis();
        updateDisplayStatus(isBLEConnected(), isRaining(),
                            isValveOpen(1), isValveOpen(2));
    }

    // Update BLE status every 5 seconds
    static unsigned long lastBLE = 0;
    if (millis() - lastBLE > 5000) {
        lastBLE = millis();
        updateBLEStatus();
    }
}
