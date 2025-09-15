
#include "water_sensors.h"
#include "../hardware/hardware_pins.h"
#include "../core/logging.h"
#include "../algorithm/water_algorithm.h"
#include "../algorithm/algorithm_config.h"

void initWaterSensors() {
    pinMode(WATER_SENSOR_1_PIN, INPUT_PULLUP);
    pinMode(WATER_SENSOR_2_PIN, INPUT_PULLUP);
    LOG_INFO("Water sensors initialized on pins %d and %d", 
             WATER_SENSOR_1_PIN, WATER_SENSOR_2_PIN);
}

bool readWaterSensor1() {
    return digitalRead(WATER_SENSOR_1_PIN) == LOW;
}

bool readWaterSensor2() {
    return digitalRead(WATER_SENSOR_2_PIN) == LOW;
}

void checkWaterSensors() {
    static bool lastSensor1 = false;
    static bool lastSensor2 = false;
    static uint32_t lastDebounce1 = 0;
    static uint32_t lastDebounce2 = 0;
    
    bool currentSensor1 = digitalRead(WATER_SENSOR_1_PIN) == LOW;
    bool currentSensor2 = digitalRead(WATER_SENSOR_2_PIN) == LOW;
    uint32_t currentTimeSeconds = millis() / 1000; // <-- Konwersja do sekund
    
    // Sensor 1 with debouncing
    if (currentSensor1 != lastSensor1) {
        if (currentTimeSeconds - lastDebounce1 > SENSOR_DEBOUNCE_TIME) { // <-- USUŃ * 1000
            lastDebounce1 = currentTimeSeconds;
            lastSensor1 = currentSensor1;
            
            // Notify algorithm
            waterAlgorithm.onSensorStateChange(1, currentSensor1);
            
            LOG_INFO("Sensor 1: %s", currentSensor1 ? "TRIGGERED" : "NORMAL");
        }
    }
    
    // Sensor 2 with debouncing
    if (currentSensor2 != lastSensor2) {
        if (currentTimeSeconds - lastDebounce2 > SENSOR_DEBOUNCE_TIME) { // <-- USUŃ * 1000
            lastDebounce2 = currentTimeSeconds;
            lastSensor2 = currentSensor2;
            
            // Notify algorithm
            waterAlgorithm.onSensorStateChange(2, currentSensor2);
            
            LOG_INFO("Sensor 2: %s", currentSensor2 ? "TRIGGERED" : "NORMAL");
        }
    }
}
// Compatibility functions for old code
void updateWaterSensors() {
    // This is now handled by checkWaterSensors
    checkWaterSensors();
}

String getWaterStatus() {
    bool sensor1 = readWaterSensor1();
    bool sensor2 = readWaterSensor2();
    
    if (sensor1 && sensor2) {
        return "BOTH_LOW";
    } else if (sensor1) {
        return "SENSOR1_LOW";
    } else if (sensor2) {
        return "SENSOR2_LOW";
    } else {
        return "NORMAL";
    }
}

bool shouldActivatePump() {
    // Now handled by water algorithm - return false to disable old logic
    // The algorithm will handle pump activation
    return false;
}