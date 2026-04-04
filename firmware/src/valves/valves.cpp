#include "valves.h"
#include "config.h"

static bool valve1State = false;
static bool valve2State = false;

void initValves() {
    pinMode(VALVE1_PIN, OUTPUT);
    pinMode(VALVE2_PIN, OUTPUT);
    closeAllValves();
}

void openValve(int valve) {
    if (valve == 1) {
        digitalWrite(VALVE1_PIN, HIGH);
        valve1State = true;
        Serial.println("Valve 1 opened");
    } else if (valve == 2) {
        digitalWrite(VALVE2_PIN, HIGH);
        valve2State = true;
        Serial.println("Valve 2 opened");
    }
}

void closeValve(int valve) {
    if (valve == 1) {
        digitalWrite(VALVE1_PIN, LOW);
        valve1State = false;
        Serial.println("Valve 1 closed");
    } else if (valve == 2) {
        digitalWrite(VALVE2_PIN, LOW);
        valve2State = false;
        Serial.println("Valve 2 closed");
    }
}

void closeAllValves() {
    closeValve(1);
    closeValve(2);
}

bool isValveOpen(int valve) {
    if (valve == 1) return valve1State;
    if (valve == 2) return valve2State;
    return false;
}