#include "ble_server.h"
#include "config.h"
#include "valves/valves.h"
#include "rain/rain.h"
#include <NimBLEDevice.h>

static NimBLEServer*         pServer       = nullptr;
static NimBLECharacteristic* pStatusChar   = nullptr;
static NimBLECharacteristic* pRainChar     = nullptr;
static bool                  bleConnected  = false;

static String buildStatusString() {
    char buf[32];
    sprintf(buf, "R:%d,V1:%d,V2:%d",
        isRaining()      ? 1 : 0,
        isValveOpen(1)   ? 1 : 0,
        isValveOpen(2)   ? 1 : 0);
    return String(buf);
}

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
class ScheduleCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
        std::string value = pChar->getValue();
        Serial.printf("Schedule received: %s\n", value.c_str());
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
void initBLE() {
    Serial.println("BLE: initializing...");
    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    Serial.println("BLE: device initialized");

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    pStatusChar = pService->createCharacteristic(
        STATUS_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    pStatusChar->setValue(buildStatusString().c_str());

    NimBLECharacteristic* pScheduleChar = pService->createCharacteristic(
        SCHEDULE_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    pScheduleChar->setCallbacks(new ScheduleCallbacks());

    pRainChar = pService->createCharacteristic(
        RAIN_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    pRainChar->setValue("R:0");

    NimBLECharacteristic* pManualChar = pService->createCharacteristic(
        MANUAL_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    pManualChar->setCallbacks(new ManualCallbacks());

    // ── Advertising ──────────────────────────────────────
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();

    // Put name explicitly in the advertising data packet
    NimBLEAdvertisementData advData;
    advData.setName(BLE_DEVICE_NAME);
    advData.setFlags(0x06); // LE General Discoverable + BR/EDR not supported
    pAdvertising->setAdvertisementData(advData);

    // Put service UUID in scan response
    NimBLEAdvertisementData scanResponse;
    scanResponse.setCompleteServices(NimBLEUUID(SERVICE_UUID));
    pAdvertising->setScanResponseData(scanResponse);

    pAdvertising->start();

    Serial.println("BLE: advertising started as '" BLE_DEVICE_NAME "'");
}



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