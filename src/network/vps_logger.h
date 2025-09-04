#ifndef VPS_LOGGER_H
#define VPS_LOGGER_H

#include <Arduino.h>

struct PumpCycle;

void initVPSLogger();
bool logEventToVPS(const String& eventType, uint16_t volumeML, const String& timestamp);
bool logCycleToVPS(const PumpCycle& cycle, const String& timestamp);

#endif