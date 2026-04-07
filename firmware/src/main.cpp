#include <Arduino.h>
#include <Wire.h>
#include <nvs_flash.h>
#include "config.h"
#include "display/display.h"
#include "ble/ble_server.h"
#include "valves/valves.h"
#include "rain/rain.h"
#include "rtc/rtc.h"
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
    initRTC();           // DS3231 — must be after Wire.begin()
    initValves();
    initRainSensor();
    initScheduler();     // loads persisted schedule from flash
    initBLE();

    char timeBuf[8];
    getRTCTimeString(timeBuf, sizeof(timeBuf));
    updateDisplayStatus(isBLEConnected(), isRaining(),
                        isValveOpen(1), isValveOpen(2), timeBuf);

    Serial.println("Watering system started");
}

void loop() {
    // ── Rain simulation (testing only) ───────────────────────────────────────
    // Remove or gate on a DEBUG flag when real rain sensor hardware is connected
    static unsigned long lastRainToggle = 0;
    static bool simRaining = false;
    if (millis() - lastRainToggle > 10000) {
        lastRainToggle = millis();
        simRaining = !simRaining;
        simulateRain(simRaining);
        Serial.printf("Rain simulated: %s\n", simRaining ? "YES" : "No");
        updateBLEStatus();
    }

    // ── Rain sensor (real GPIO) ───────────────────────────────────────────────
    static unsigned long lastRainCheck = 0;
    if (millis() - lastRainCheck > 2000) {
        lastRainCheck = millis();
        updateRainSensor();
    }

    // ── Scheduler tick — every 30 s is enough (minute resolution) ────────────
    static unsigned long lastScheduleCheck = 0;
    if (millis() - lastScheduleCheck > 30000) {
        lastScheduleCheck = millis();

        RtcTime now;
        if (isRTCReady() && getRTCTime(now)) {
            checkSchedule(now.weekday, now.hour, now.minute);
        } else {
            Serial.println("Schedule tick — RTC not ready or time not set");
        }
    }

    // ── Display refresh — every second ───────────────────────────────────────
    static unsigned long lastDisplay = 0;
    if (millis() - lastDisplay > 1000) {
        lastDisplay = millis();
        char timeBuf[8];
        getRTCTimeString(timeBuf, sizeof(timeBuf));
        updateDisplayStatus(isBLEConnected(), isRaining(),
                            isValveOpen(1), isValveOpen(2), timeBuf);
    }

    // ── BLE status notify — every 5 s ────────────────────────────────────────
    static unsigned long lastBLE = 0;
    if (millis() - lastBLE > 5000) {
        lastBLE = millis();
        updateBLEStatus();
    }
}
