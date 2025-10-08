// #ifndef VPS_LOGGER_H
// #define VPS_LOGGER_H

// #include <Arduino.h>

// struct PumpCycle;

// class WaterAlgorithm;

// void initVPSLogger();
// bool logEventToVPS(const String& eventType, uint16_t volumeML, const String& timestamp);
// bool logCycleToVPS(const PumpCycle& cycle, const String& timestamp);

// #endif


#ifndef VPS_LOGGER_H
#define VPS_LOGGER_H

#include <Arduino.h>

struct PumpCycle;
class WaterAlgorithm;

void initVPSLogger();
bool logEventToVPS(const String& eventType, uint16_t volumeML, const String& timestamp);

// 🆕 P1: CHANGED SIGNATURE - przyjmuje unix timestamp zamiast string
bool logCycleToVPS(const PumpCycle& cycle, uint32_t unixTime);

#endif