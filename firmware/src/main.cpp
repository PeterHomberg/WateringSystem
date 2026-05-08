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
#include "wifi_manager/wifi_manager.h"
#include "web_server/web_server.h"

static void refreshDisplay() {
    char timeBuf[8];
    getRTCTimeString(timeBuf, sizeof(timeBuf));
    String ip = getWiFiIP();
    updateDisplayStatus(
        isBLEConnected(),
        isWiFiConnected(), ip.c_str(),
        isRaining(), getRainLevel(),
        isValveOpen(1), isValveOpen(2),
        timeBuf
    );
}

void setup() {
    Serial.begin(115200);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    Serial.println("NVS initialized");

    initWiFi();                    // ← moved here, before anything else

    Wire.begin(SDA_PIN, SCL_PIN);
    initDisplay();
    initRTC();
    initValves();
    initRainSensor();
    initScheduler();

    if (isWiFiConnected()) {
        initWebServer();
    }

    //initBLE();
    refreshDisplay();
    Serial.println("Watering system started");
}
void loop() {
    // ── Web server — must be polled every loop iteration ─────────────────────
    if (isWiFiConnected()) {
        handleWebServer();
    }

    // ── Rain sensor — every 2 s ───────────────────────────────────────────────
    static unsigned long lastRain = 0;
    if (millis() - lastRain > 2000) {
        lastRain = millis();
        updateRainSensor();
        //updateBLEStatus();
    }

    // ── Scheduler — every 30 s ────────────────────────────────────────────────
    static unsigned long lastSched = 0;
    if (millis() - lastSched > 30000) {
        lastSched = millis();
        RtcTime now;
        if (isRTCReady() && getRTCTime(now)) {
            checkSchedule(now.weekday, now.hour, now.minute);
        } else {
            Serial.println("Schedule tick — RTC not ready");
        }
    }

    // ── Display — every second ────────────────────────────────────────────────
    static unsigned long lastDisplay = 0;
    if (millis() - lastDisplay > 1000) {
        lastDisplay = millis();
        refreshDisplay();
    }

    // ── BLE status — every 5 s ────────────────────────────────────────────────
    static unsigned long lastBLE = 0;
    if (millis() - lastBLE > 5000) {
        lastBLE = millis();
        //updateBLEStatus();
    }
}
