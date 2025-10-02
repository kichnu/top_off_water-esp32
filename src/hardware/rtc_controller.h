#ifndef RTC_CONTROLLER_H
#define RTC_CONTROLLER_H

#include <Arduino.h>

void initializeRTC();
String getCurrentTimestamp();
bool isRTCWorking();
unsigned long getUnixTimestamp();
String getRTCInfo(); // New diagnostic function

String getTimeSourceInfo();
bool isRTCHardware();

// Dodaj na końcu pliku .h:
void periodicNTPSync();                    // Call from main loop
bool rtcNeedsSynchronization();            // Check if time needs sync
void initInternalTimeFromCompileTime();    // Helper function
bool setRTCFromNTP();                      // Attempt NTP sync and set RTC

bool rtcNeedsSynchronization();
bool isBatteryIssueDetected();

#endif