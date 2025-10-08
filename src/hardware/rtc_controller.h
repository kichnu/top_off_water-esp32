

#ifndef RTC_CONTROLLER_H
#define RTC_CONTROLLER_H

#include <Arduino.h>

void initializeRTC();
String getCurrentTimestamp();  // ⚠️ Teraz używa cache!
bool isRTCWorking();
unsigned long getUnixTimestamp();  // ⚠️ Teraz używa cache!
String getRTCInfo();

String getTimeSourceInfo();
bool isRTCHardware();

void periodicNTPSync();
bool rtcNeedsSynchronization();
void initInternalTimeFromCompileTime();
bool setRTCFromNTP();

bool rtcNeedsSynchronization();
bool isBatteryIssueDetected();

// 🆕 P2: Cache management
void updateTimeCacheFromRTC();  // Force refresh cache

#endif