// #ifndef RTC_CONTROLLER_H
// #define RTC_CONTROLLER_H

// #include <Arduino.h>

// void initializeRTC();
// String getCurrentTimestamp();
// bool isRTCWorking();
// unsigned long getUnixTimestamp();
// String getRTCInfo(); // New diagnostic function

// String getTimeSourceInfo();
// bool isRTCHardware();

// // Dodaj na końcu pliku .h:
// void periodicNTPSync();                    // Call from main loop
// bool rtcNeedsSynchronization();            // Check if time needs sync
// void initInternalTimeFromCompileTime();    // Helper function
// bool setRTCFromNTP();                      // Attempt NTP sync and set RTC

// bool rtcNeedsSynchronization();
// bool isBatteryIssueDetected();

// #endif



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