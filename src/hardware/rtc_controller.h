#ifndef RTC_CONTROLLER_H
#define RTC_CONTROLLER_H

#include <Arduino.h>

void initializeRTC();
String getCurrentTimestamp();
bool isRTCWorking();
unsigned long getUnixTimestamp();
String getRTCInfo(); // New diagnostic function

#endif