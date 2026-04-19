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

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    Serial.println("NVS initialized");

    Wire.begin(SDA_PIN, SCL_PIN);
    initDisplay();
    initRTC();
    initValves();
    initRainSensor();      // real hardware — simulation loop removed
    initScheduler();
    initBLE();

    char timeBuf[8];
    getRTCTimeString(timeBuf, sizeof(timeBuf));
    updateDisplayStatus(isBLEConnected(), isRaining(), getRainLevel(),
                        isValveOpen(1), isValveOpen(2), timeBuf);

    Serial.println("Watering system started");
}

void loop() {
    // ── Rain sensor — read both digital and analog every 2 s ─────────────────
    static unsigned long lastRainCheck = 0;
    if (millis() - lastRainCheck > 2000) {
        lastRainCheck = millis();
        updateRainSensor();
        updateBLEStatus();   // push updated R/L values to iOS immediately
    }

    // ── Scheduler tick — every 30 s ───────────────────────────────────────────
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

    // ── Display refresh — every second ────────────────────────────────────────
    static unsigned long lastDisplay = 0;
    if (millis() - lastDisplay > 1000) {
        lastDisplay = millis();
        char timeBuf[8];
        getRTCTimeString(timeBuf, sizeof(timeBuf));
        updateDisplayStatus(isBLEConnected(), isRaining(), getRainLevel(),
                            isValveOpen(1), isValveOpen(2), timeBuf);
    }

    // ── BLE status notify — every 5 s ─────────────────────────────────────────
    static unsigned long lastBLE = 0;
    if (millis() - lastBLE > 5000) {
        lastBLE = millis();
        updateBLEStatus();
    }
}
