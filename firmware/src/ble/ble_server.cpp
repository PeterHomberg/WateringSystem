#include "ble_server.h"
#include "config.h"
#include "valves/valves.h"
#include "rain/rain.h"
#include "scheduler/scheduler.h"
#include "rtc/rtc.h"
#include <NimBLEDevice.h>

static NimBLEServer*         pServer       = nullptr;
static NimBLECharacteristic* pStatusChar   = nullptr;
static NimBLECharacteristic* pRainChar     = nullptr;
static bool                  bleConnected  = false;

static String buildStatusString() {
    char buf[48];
    // Format: "R:0,V1:0,V2:0,SCH:OK,T:14:30"
    // T field shows current RTC time — useful for the iOS app to verify sync
    char timeBuf[8];
    getRTCTimeString(timeBuf, sizeof(timeBuf));
    sprintf(buf, "R:%d,V1:%d,V2:%d,%s,T:%s",
        isRaining()    ? 1 : 0,
        isValveOpen(1) ? 1 : 0,
        isValveOpen(2) ? 1 : 0,
        getScheduleAck(),
        timeBuf);
    return String(buf);
}

// ── Server callbacks ──────────────────────────────────────────────────────────

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        bleConnected = true;
        Serial.println("BLE client connected");
    }
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        bleConnected = false;
        Serial.println("BLE client disconnected");
        NimBLEDevice::startAdvertising();
    }
};

// ── Characteristic callbacks ──────────────────────────────────────────────────

class ScheduleCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
        std::string value = pChar->getValue();
        String line = String(value.c_str());
        line.trim();

        bool handled = processScheduleLine(line);

        // After SCHED:END the ack is updated — notify iOS immediately
        if (line == "SCHED:END") {
            updateBLEStatus();
        }

        if (!handled && !line.startsWith("SCHED:")) {
            Serial.printf("ScheduleCallbacks: unrecognised line '%s'\n", line.c_str());
        }
    }
};

class ManualCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
        std::string value = pChar->getValue();
        String cmd = String(value.c_str());
        Serial.printf("Manual command: %s\n", cmd.c_str());

        if      (cmd == "V1:1") openValve(1);
        else if (cmd == "V1:0") closeValve(1);
        else if (cmd == "V2:1") openValve(2);
        else if (cmd == "V2:0") closeValve(2);
        else Serial.printf("Unknown command: %s\n", cmd.c_str());

        updateBLEStatus();
    }
};

// TIME characteristic — iOS writes current time to sync the DS3231.
// Write format: "TIME:2025-04-07T14:30:00W1"
//   W<n> = weekday, firmware convention: 0=Mon … 6=Sun
class TimeCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
        std::string value = pChar->getValue();
        String cmd = String(value.c_str());
        cmd.trim();
        Serial.printf("TIME write: %s\n", cmd.c_str());

        if (setRTCTimeFromString(cmd)) {
            Serial.println("RTC: time updated successfully via BLE");
        } else {
            Serial.println("RTC: time update FAILED — invalid format");
        }

        // Notify iOS with updated status so it sees the new T: field
        updateBLEStatus();
    }
};

// ── initBLE ───────────────────────────────────────────────────────────────────

void initBLE() {
    Serial.println("BLE: initializing...");
    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    Serial.println("BLE: device initialized");

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    // STATUS — ESP32 → iOS (read + notify)
    pStatusChar = pService->createCharacteristic(
        STATUS_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    pStatusChar->setValue(buildStatusString().c_str());

    // SCHEDULE — iOS → ESP32 (write)
    NimBLECharacteristic* pScheduleChar = pService->createCharacteristic(
        SCHEDULE_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    pScheduleChar->setCallbacks(new ScheduleCallbacks());

    // RAIN — ESP32 → iOS (read + notify)
    pRainChar = pService->createCharacteristic(
        RAIN_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    pRainChar->setValue("R:0");

    // MANUAL — iOS → ESP32 (write)
    NimBLECharacteristic* pManualChar = pService->createCharacteristic(
        MANUAL_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    pManualChar->setCallbacks(new ManualCallbacks());

    // TIME — iOS → ESP32 (write).  Read also supported so iOS can verify sync.
    NimBLECharacteristic* pTimeChar = pService->createCharacteristic(
        TIME_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ
    );
    pTimeChar->setCallbacks(new TimeCallbacks());
    pTimeChar->setValue("TIME:not set");

    // ── Advertising ──────────────────────────────────────────────────────────
    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();

    NimBLEAdvertisementData advData;
    advData.setName(BLE_DEVICE_NAME);
    advData.setFlags(0x06);
    pAdvertising->setAdvertisementData(advData);

    NimBLEAdvertisementData scanResponse;
    scanResponse.setCompleteServices(NimBLEUUID(SERVICE_UUID));
    pAdvertising->setScanResponseData(scanResponse);

    pAdvertising->start();

    Serial.println("BLE: advertising started as '" BLE_DEVICE_NAME "'");
}

// ── updateBLEStatus ───────────────────────────────────────────────────────────

void updateBLEStatus() {
    if (!bleConnected) return;
    String status = buildStatusString();
    pStatusChar->setValue(status.c_str());
    pStatusChar->notify();
    char rainStr[8];
    sprintf(rainStr, "R:%d", isRaining() ? 1 : 0);
    pRainChar->setValue(rainStr);
    pRainChar->notify();
}

bool isBLEConnected() {
    return bleConnected;
}
