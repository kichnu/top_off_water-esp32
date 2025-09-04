#ifndef WATER_SENSORS_H
#define WATER_SENSORS_H

#include <Arduino.h>

struct SensorState {
    bool currentReading;
    bool stableReading;
    unsigned long lastChangeTime;
    bool isStable;
};

void initWaterSensors();
void updateWaterSensors();
String getWaterStatus();
bool isWaterLevelLow();
bool shouldActivatePump();
void checkWaterSensors();
bool readWaterSensor1();
bool readWaterSensor2();

#endif