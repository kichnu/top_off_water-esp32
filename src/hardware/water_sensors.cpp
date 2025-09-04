// #include "water_sensors.h"
// #include "../config/hardware_pins.h"
// #include "../core/logging.h"

// #include "../algorithm/water_algorithm.h"  // <-- DODAJ



// SensorState sensor1;
// SensorState sensor2;

// void initWaterSensors() {
//     pinMode(WATER_SENSOR_1_PIN, INPUT_PULLUP);
//     pinMode(WATER_SENSOR_2_PIN, INPUT_PULLUP);
    
//     // Initialize sensor states
//     sensor1.currentReading = (digitalRead(WATER_SENSOR_1_PIN) == LOW);
//     sensor2.currentReading = (digitalRead(WATER_SENSOR_2_PIN) == LOW);
//     sensor1.stableReading = sensor1.currentReading;
//     sensor2.stableReading = sensor2.currentReading;
//     sensor1.isStable = true;
//     sensor2.isStable = true;
//     sensor1.lastChangeTime = 0;
//     sensor2.lastChangeTime = 0;
    
//     LOG_INFO("Water sensors initialized - Sensor1: %s, Sensor2: %s",
//              sensor1.currentReading ? "LOW" : "OK",
//              sensor2.currentReading ? "LOW" : "OK");
// }

// void updateWaterSensors() {
//     unsigned long now = millis();
    
//     // Read sensors (LOW = water needed due to pull-up)
//     bool s1_raw = (digitalRead(WATER_SENSOR_1_PIN) == LOW);
//     bool s2_raw = (digitalRead(WATER_SENSOR_2_PIN) == LOW);
    
//     // Debounce sensor 1
//     if (s1_raw != sensor1.currentReading) {
//         sensor1.currentReading = s1_raw;
//         sensor1.lastChangeTime = now;
//         sensor1.isStable = false;
//     } else if (!sensor1.isStable && (now - sensor1.lastChangeTime > DEBOUNCE_DELAY_MS)) {
//         sensor1.stableReading = s1_raw;
//         sensor1.isStable = true;
//         LOG_INFO("Sensor1 stabilized: %s", s1_raw ? "LOW" : "OK");
//     }
    
//     // Debounce sensor 2
//     if (s2_raw != sensor2.currentReading) {
//         sensor2.currentReading = s2_raw;
//         sensor2.lastChangeTime = now;
//         sensor2.isStable = false;
//     } else if (!sensor2.isStable && (now - sensor2.lastChangeTime > DEBOUNCE_DELAY_MS)) {
//         sensor2.stableReading = s2_raw;
//         sensor2.isStable = true;
//         LOG_INFO("Sensor2 stabilized: %s", s2_raw ? "LOW" : "OK");
//     }
// }

// String getWaterStatus() {
//     if (!sensor1.isStable || !sensor2.isStable) {
//         return "CHECKING";
//     }
    
//     if (sensor1.stableReading && sensor2.stableReading) {
//         return "LOW";
//     } else if (!sensor1.stableReading && !sensor2.stableReading) {
//         return "OK";
//     } else {
//         return "PARTIAL"; // One sensor triggered
//     }
// }

// bool isWaterLevelLow() {
//     return sensor1.isStable && sensor2.isStable && 
//            sensor1.stableReading && sensor2.stableReading;
// }

// bool shouldActivatePump() {
//     return isWaterLevelLow(); // Future: add more logic here
// }


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

// void checkWaterSensors() {
//     static bool lastSensor1 = false;
//     static bool lastSensor2 = false;
//     static uint32_t lastDebounce1 = 0;
//     static uint32_t lastDebounce2 = 0;
    
//     bool currentSensor1 = digitalRead(WATER_SENSOR_1_PIN) == LOW;
//     bool currentSensor2 = digitalRead(WATER_SENSOR_2_PIN) == LOW;
//     uint32_t currentTime = millis();
    
//     // Sensor 1 with 1 second debouncing
//     if (currentSensor1 != lastSensor1) {
//         if (currentTime - lastDebounce1 > SENSOR_DEBOUNCE_TIME * 1000) {
//             lastDebounce1 = currentTime;
//             lastSensor1 = currentSensor1;
            
//             // Notify algorithm
//             waterAlgorithm.onSensorStateChange(1, currentSensor1);
            
//             LOG_INFO("Sensor 1: %s", currentSensor1 ? "TRIGGERED" : "NORMAL");
//         }
//     }
    
//     // Sensor 2 with 1 second debouncing
//     if (currentSensor2 != lastSensor2) {
//         if (currentTime - lastDebounce2 > SENSOR_DEBOUNCE_TIME * 1000) {
//             lastDebounce2 = currentTime;
//             lastSensor2 = currentSensor2;
            
//             // Notify algorithm
//             waterAlgorithm.onSensorStateChange(2, currentSensor2);
            
//             LOG_INFO("Sensor 2: %s", currentSensor2 ? "TRIGGERED" : "NORMAL");
//         }
//     }
// }

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