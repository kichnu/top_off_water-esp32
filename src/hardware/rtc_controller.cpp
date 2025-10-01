


// #include "rtc_controller.h"
// #include "../core/logging.h"
// #include "../hardware/hardware_pins.h"
// #include <Wire.h>
// #include <RTClib.h>
// #include <WiFi.h>
// #include <time.h>
// #include "../network/wifi_manager.h"

// // NTP Configuration
// const char* NTP_SERVER = "216.239.35.0"; 
// // const long GMT_OFFSET_SEC = 7200;   
// long currentTimezoneOffset = 3600;
// const int DAYLIGHT_OFFSET_SEC = 0;     // No DST

// RTC_DS3231 rtc;
// bool rtcInitialized = false;
// bool useInternalRTC = false;
// bool rtcNeedsSync = false;  // 🆕 Flag: RTC needed NTP sync (battery issue indicator)

// // Fallback time structure for internal RTC
// struct {
//     int year = 2024;
//     int month = 1;
//     int day = 1;
//     int hour = 12;
//     int minute = 0;
//     int second = 0;
//     unsigned long lastUpdate = 0;
// } internalTime;

// // Last NTP sync timestamp for periodic updates
// unsigned long lastNTPSync = 0;
// const unsigned long NTP_SYNC_INTERVAL = 3600000;  // 1 hour

// // DST calculation for Poland (European Union rules)
// // CET (Winter): UTC+1 (3600 seconds)
// // CEST (Summer): UTC+2 (7200 seconds)

// // Helper: Get day of week (0=Sunday, 6=Saturday)
// int getDayOfWeek(int year, int month, int day) {
//     // Zeller's congruence algorithm
//     if (month < 3) {
//         month += 12;
//         year--;
//     }
//     int q = day;
//     int m = month;
//     int k = year % 100;
//     int j = year / 100;
    
//     int h = (q + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
//     return (h + 6) % 7; // Convert to 0=Sunday format
// }

// // Helper: Get last Sunday of a month
// int getLastSunday(int year, int month) {
//     // Days in month
//     int daysInMonth;
//     if (month == 2) {
//         daysInMonth = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
//     } else if (month == 4 || month == 6 || month == 9 || month == 11) {
//         daysInMonth = 30;
//     } else {
//         daysInMonth = 31;
//     }
    
//     // Find last Sunday
//     for (int day = daysInMonth; day >= 1; day--) {
//         if (getDayOfWeek(year, month, day) == 0) {
//             return day;
//         }
//     }
//     return daysInMonth; // Fallback
// }

// // Main function: Check if DST is active for Poland
// bool isDSTActive(int year, int month, int day, int hour) {
//     // DST starts: Last Sunday of March at 2:00 AM
//     // DST ends: Last Sunday of October at 3:00 AM
    
//     int marchLastSunday = getLastSunday(year, 3);
//     int octoberLastSunday = getLastSunday(year, 10);
    
//     // Before March or after October - definitely winter time
//     if (month < 3 || month > 10) {
//         return false;
//     }
    
//     // April to September - definitely summer time
//     if (month > 3 && month < 10) {
//         return true;
//     }
    
//     // March - check if after last Sunday at 2:00
//     if (month == 3) {
//         if (day < marchLastSunday) {
//             return false; // Before DST start
//         } else if (day > marchLastSunday) {
//             return true; // After DST start
//         } else {
//             // On the transition day - check hour
//             return (hour >= 2); // DST starts at 2:00 AM
//         }
//     }
    
//     // October - check if before last Sunday at 3:00
//     if (month == 10) {
//         if (day < octoberLastSunday) {
//             return true; // Still in DST
//         } else if (day > octoberLastSunday) {
//             return false; // DST ended
//         } else {
//             // On the transition day - check hour
//             return (hour < 3); // DST ends at 3:00 AM
//         }
//     }
    
//     return false; // Fallback
// }

// // Get current timezone offset for Poland
// long getPolandTimezoneOffset(int year, int month, int day, int hour) {
//     if (isDSTActive(year, month, day, hour)) {
//         return 7200; // UTC+2 (CEST - Summer)
//     } else {
//         return 3600; // UTC+1 (CET - Winter)
//     }
// }

// // Convenience function using current date
// long getCurrentPolandOffset() {
//     time_t now;
//     time(&now);
//     struct tm* utc_time = gmtime(&now);
    
//     return getPolandTimezoneOffset(
//         utc_time->tm_year + 1900,
//         utc_time->tm_mon + 1,
//         utc_time->tm_mday,
//         utc_time->tm_hour
//     );
// }

// // ===== EXAMPLE USAGE =====

// // void testDST() {
// //     // Test cases
// //     Serial.println("=== DST Tests for Poland ===");
    
// //     // Winter time
// //     Serial.printf("2025-01-15: UTC+%d\n", 
// //         getPolandTimezoneOffset(2025, 1, 15, 12) / 3600);
    
// //     // Before DST start (March 29, 2025)
// //     Serial.printf("2025-03-29 01:00: UTC+%d\n", 
// //         getPolandTimezoneOffset(2025, 3, 29, 1) / 3600);
    
// //     // After DST start (March 30, 2025 at 3:00 - was 2:00)
// //     Serial.printf("2025-03-30 03:00: UTC+%d\n", 
// //         getPolandTimezoneOffset(2025, 3, 30, 3) / 3600);
    
// //     // Summer time
// //     Serial.printf("2025-07-15: UTC+%d\n", 
// //         getPolandTimezoneOffset(2025, 7, 15, 12) / 3600);
    
// //     // Before DST end (October 25, 2025)
// //     Serial.printf("2025-10-25 02:00: UTC+%d\n", 
// //         getPolandTimezoneOffset(2025, 10, 25, 2) / 3600);
    
// //     // After DST end (October 26, 2025 at 2:00 - was 3:00)
// //     Serial.printf("2025-10-26 02:00: UTC+%d\n", 
// //         getPolandTimezoneOffset(2025, 10, 26, 2) / 3600);
// // }

// // Helper: Initialize internal time from compile time
// void initInternalTimeFromCompileTime() {
//     char month_str[4];
//     int day, year, hour, min, sec;
//     sscanf(__DATE__, "%s %d %d", month_str, &day, &year);
//     sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);
    
//     const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
//                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
//     int month = 1;
//     for (int i = 0; i < 12; i++) {
//         if (strncmp(month_str, months[i], 3) == 0) {
//             month = i + 1;
//             break;
//         }
//     }
    
//     internalTime.year = year;
//     internalTime.month = month;
//     internalTime.day = day;
//     internalTime.hour = hour;
//     internalTime.minute = min;
//     internalTime.second = sec;
//     internalTime.lastUpdate = millis();
    
//     LOG_INFO("Internal RTC set to compile time: %s", getCurrentTimestamp().c_str());
// }

// // Synchronize with NTP server
// bool syncTimeFromNTP() {
//     if (!isWiFiConnected()) {
//         LOG_WARNING("Cannot sync NTP - WiFi not connected");
//         return false;
//     }

//     currentTimezoneOffset = getCurrentPolandOffset();
//     LOG_INFO("Current timezone offset: UTC%+d", currentTimezoneOffset / 3600);
    
//     LOG_INFO("Attempting NTP synchronization from %s...", NTP_SERVER);
    
//     configTime(currentTimezoneOffset, 0, NTP_SERVER);  // ✅ Użyj dynamicznego
    
//     LOG_INFO("Attempting NTP synchronization from %s...", NTP_SERVER);
    
//     // configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
//     int attempts = 0;
//     time_t now = 0;
//     struct tm timeinfo;
    
//     while (attempts < 30) {  // 15 seconds timeout
//         time(&now);
//         if (now > 1600000000) {
//             localtime_r(&now, &timeinfo);
            
//             char timeStr[64];
//             strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
//             LOG_INFO("NTP sync successful: %s (local time)", timeStr);
            
//             lastNTPSync = millis();
//             return true;
//         }
//         delay(500);
//         attempts++;
//     }
    
//     LOG_ERROR("NTP synchronization failed after 15 seconds");
//     return false;
// }

// // Set RTC from NTP with timezone handling
// // bool setRTCFromNTP() {
// //     if (!syncTimeFromNTP()) {
// //         return false;
// //     }
    
// //     time_t ntp_time;
// //     time(&ntp_time);
    
// //     // 🔧 CRITICAL: Add offset to store LOCAL time in RTC
// //     ntp_time += GMT_OFFSET_SEC;
    
// //     rtc.adjust(DateTime(ntp_time));
    
// //     DateTime newTime = rtc.now();
// //     LOG_INFO("RTC updated from NTP (local time): %04d-%02d-%02d %02d:%02d:%02d",
// //              newTime.year(), newTime.month(), newTime.day(),
// //              newTime.hour(), newTime.minute(), newTime.second());
    
// //     // rtcNeedsSync = false;
// //     return true;
// // }


// bool setRTCFromNTP() {
//     if (!syncTimeFromNTP()) {
//         return false;
//     }
    
//     time_t ntp_time;
//     time(&ntp_time);
    
//     // ✅ Użyj aktualnego offsetu (już obliczonego w syncTimeFromNTP)
//     ntp_time += currentTimezoneOffset;
    
//     rtc.adjust(DateTime(ntp_time));
    
//     DateTime newTime = rtc.now();
//     LOG_INFO("RTC updated from NTP (local time UTC%+d): %04d-%02d-%02d %02d:%02d:%02d",
//              currentTimezoneOffset / 3600,
//              newTime.year(), newTime.month(), newTime.day(),
//              newTime.hour(), newTime.minute(), newTime.second());
    
//     return true;
// }

// // Periodic NTP sync (call from main loop)
// void periodicNTPSync() {
//     if (!rtcInitialized || useInternalRTC) {
//         return;
//     }
    
//     if (!isWiFiConnected()) {
//         return;
//     }
    
//     unsigned long now = millis();
//     if (now - lastNTPSync >= NTP_SYNC_INTERVAL) {
//         LOG_INFO("Periodic NTP synchronization...");
        
//         if (setRTCFromNTP()) {
//             LOG_INFO("Periodic sync successful");
//         } else {
//             LOG_WARNING("Periodic sync failed - will retry in 1 hour");
//             lastNTPSync = now;
//         }
//     }
// }

// void initializeRTC() {
//     LOG_INFO("Starting RTC initialization...");

//     currentTimezoneOffset = 3600;  // CET domyślnie


//     LOG_INFO("Attempting to initialize external DS3231 RTC...");
    
//     Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
//     Wire.setClock(100000);
    
//     Wire.beginTransmission(0x68);
//     byte error = Wire.endTransmission();
    
//     if (error == 0) {
//         LOG_INFO("DS3231 detected on I2C bus, attempting initialization...");
        
//         if (rtc.begin()) {
            
//             // 🔧 Check battery/power loss (OSF flag)
//             if (rtc.lostPower()) {
//                 LOG_WARNING("RTC lost power - battery dead or removed");
//                 LOG_INFO("Setting RTC to compile time to clear OSF flag...");
                
//                 rtcNeedsSync = true;  // 🆕 FLAG: Battery issue detected!

//                 DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
//                 rtc.adjust(compileTime);
//                 delay(100);

//                 if (rtc.lostPower()) {
//                     LOG_ERROR("Failed to clear RTC OSF flag - hardware problem");
//                     LOG_INFO("Switching to system time fallback");
                    
//                     useInternalRTC = true;
//                     rtcInitialized = true;
                    
//                     initInternalTimeFromCompileTime();
//                     return;
//                 }
                
//                 LOG_INFO("OSF flag cleared successfully");
//                 LOG_WARNING("Battery issue detected - replace CR2032 battery"); 
                
//                 // Immediately try NTP sync
//                 LOG_INFO("Attempting immediate NTP sync after battery loss...");
                
//                 if (setRTCFromNTP()) {
//                     LOG_INFO("RTC successfully synchronized with NTP");
//                     LOG_WARNING("Battery warning will remain until battery is replaced");
//                     rtcInitialized = true;
//                     useInternalRTC = false;
//                     return;
//                 } else {
//                     LOG_WARNING("NTP sync failed - RTC has compile time (inaccurate!)");
//                     LOG_WARNING("Will retry NTP sync periodically");
//                     // rtcNeedsSync remains true
//                 }
//             }

//             // Verify RTC time (both in local timezone now)
//             DateTime now = rtc.now();
//             DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
            
//             LOG_INFO("RTC current time: %04d-%02d-%02d %02d:%02d:%02d", 
//                      now.year(), now.month(), now.day(),
//                      now.hour(), now.minute(), now.second());
//             LOG_INFO("Compile time:     %04d-%02d-%02d %02d:%02d:%02d",
//                      compileTime.year(), compileTime.month(), compileTime.day(),
//                      compileTime.hour(), compileTime.minute(), compileTime.second());

//             // Check if RTC older than compile time
//             if (now.unixtime() <= compileTime.unixtime() + 60) {
//                 LOG_WARNING("RTC time is NOT newer than compile time!");
//                 LOG_WARNING("   This indicates stale/inaccurate time");
//                 LOG_INFO("Attempting to fix RTC with NTP...");
                
//                 rtcNeedsSync = true;  // 🆕 FLAG: Time issue detected!

//                 if (setRTCFromNTP()) {
//                     LOG_INFO("RTC successfully synchronized with NTP");
//                     rtcInitialized = true;
//                     useInternalRTC = false;
//                     return;
//                 } else {
//                     LOG_WARNING("NTP sync failed - using RTC with stale time");
//                     LOG_WARNING("Will retry NTP sync periodically");
//                 }
//             }

//             // Validate time components
//             bool timeValid = true;
//             if (now.year() < 2020 || now.year() > 2035) {
//                 LOG_ERROR("RTC year invalid: %d", now.year());
//                 timeValid = false;
//             }
//             if (now.month() < 1 || now.month() > 12) {
//                 LOG_ERROR("RTC month invalid: %d", now.month());
//                 timeValid = false;
//             }
//             if (now.day() < 1 || now.day() > 31) {
//                 LOG_ERROR("RTC day invalid: %d", now.day());
//                 timeValid = false;
//             }

//             if (!timeValid) {
//                 LOG_ERROR("RTC has invalid time components");
//                 LOG_INFO("Attempting to fix RTC with NTP time...");
                
//                 rtcNeedsSync = true;
                
//                 if (setRTCFromNTP()) {
//                     LOG_INFO("RTC restored from NTP");
//                     rtcInitialized = true;
//                     useInternalRTC = false;
//                     return;
//                 } else {
//                     LOG_WARNING("NTP sync failed, switching to system time fallback");
//                     useInternalRTC = true;
//                     rtcInitialized = true;
//                     initInternalTimeFromCompileTime();
//                     return;
//                 }
//             }

//             // RTC valid
//             LOG_INFO("RTC time verification successful");
//             rtcInitialized = true;
//             useInternalRTC = false;
            
//             if (!rtcNeedsSync) {
//                 LOG_INFO("RTC time appears accurate");
//             } else {
//                 LOG_WARNING("RTC time may be inaccurate - will sync with NTP when possible");
//             }
            
//             return;

//         } else {
//             LOG_ERROR("Failed to initialize DS3231 RTC");
//         }

//     } else {
//         LOG_WARNING("DS3231 not found on I2C bus (error: %d)", error);
//     }
    
//     // Fallback to system time
//     LOG_INFO("Falling back to ESP32 system time");
//     useInternalRTC = true;
//     rtcInitialized = true;
//     rtcNeedsSync = true;
    
//     struct tm timeinfo;
//     if (!getLocalTime(&timeinfo, 100)) {
//         LOG_WARNING("Failed to get system time, using compile time");
//         initInternalTimeFromCompileTime();
//     } else {
//         internalTime.year = timeinfo.tm_year + 1900;
//         internalTime.month = timeinfo.tm_mon + 1;
//         internalTime.day = timeinfo.tm_mday;
//         internalTime.hour = timeinfo.tm_hour;
//         internalTime.minute = timeinfo.tm_min;
//         internalTime.second = timeinfo.tm_sec;
//         internalTime.lastUpdate = millis();
        
//         LOG_INFO("Internal RTC set from system time: %s", getCurrentTimestamp().c_str());
//     }
// }

// String getCurrentTimestamp() {
//     if (!rtcInitialized) {
//         return "RTC_NOT_INITIALIZED";
//     }
    
//     if (useInternalRTC) {
//         unsigned long now = millis();
//         unsigned long elapsed = (now - internalTime.lastUpdate) / 1000;
        
//         if (elapsed > 0) {
//             internalTime.second += elapsed;
//             while (internalTime.second >= 60) {
//                 internalTime.second -= 60;
//                 internalTime.minute++;
//                 if (internalTime.minute >= 60) {
//                     internalTime.minute = 0;
//                     internalTime.hour++;
//                     if (internalTime.hour >= 24) {
//                         internalTime.hour = 0;
//                         internalTime.day++;
//                         if (internalTime.day > 28) {
//                             internalTime.day = 1;
//                             internalTime.month++;
//                             if (internalTime.month > 12) {
//                                 internalTime.month = 1;
//                                 internalTime.year++;
//                             }
//                         }
//                     }
//                 }
//             }
//             internalTime.lastUpdate = now;
//         }
        
//         char timeBuffer[32];
//         snprintf(timeBuffer, sizeof(timeBuffer), "%04d-%02d-%02d %02d:%02d:%02d",
//                  internalTime.year, internalTime.month, internalTime.day,
//                  internalTime.hour, internalTime.minute, internalTime.second);
        
//         return String(timeBuffer);
//     } else {
//         DateTime now = rtc.now();
        
//         char timeBuffer[32];
//         snprintf(timeBuffer, sizeof(timeBuffer), "%04d-%02d-%02d %02d:%02d:%02d",
//                  now.year(), now.month(), now.day(),
//                  now.hour(), now.minute(), now.second());
        
//         return String(timeBuffer);
//     }
// }

// bool isRTCWorking() {
//     if (!rtcInitialized) {
//         return false;
//     }
    
//     if (useInternalRTC) {
//         return true;
//     } else {
//         DateTime now = rtc.now();
//         return (now.year() >= 2020 && now.year() <= 2035);
//     }
// }

// // unsigned long getUnixTimestamp() {
// //     if (!rtcInitialized) {
// //         return 1609459200;
// //     }
    
// //     if (useInternalRTC) {
// //         struct tm tm_time;
// //         tm_time.tm_year = internalTime.year - 1900;
// //         tm_time.tm_mon = internalTime.month - 1;
// //         tm_time.tm_mday = internalTime.day;
// //         tm_time.tm_hour = internalTime.hour;
// //         tm_time.tm_min = internalTime.minute;
// //         tm_time.tm_sec = internalTime.second;
// //         tm_time.tm_isdst = 0;
        
// //         return mktime(&tm_time);
// //     } else {
// //         // RTC stores local time, convert to UTC
// //         DateTime now = rtc.now();
// //         unsigned long localTimestamp = now.unixtime();
// //         return localTimestamp - GMT_OFFSET_SEC;
// //     }
// // }


// unsigned long getUnixTimestamp() {
//     if (!rtcInitialized) {
//         return 1609459200;
//     }
    
//     if (useInternalRTC) {
//         struct tm tm_time;
//         tm_time.tm_year = internalTime.year - 1900;
//         tm_time.tm_mon = internalTime.month - 1;
//         tm_time.tm_mday = internalTime.day;
//         tm_time.tm_hour = internalTime.hour;
//         tm_time.tm_min = internalTime.minute;
//         tm_time.tm_sec = internalTime.second;
//         tm_time.tm_isdst = 0;
        
//         return mktime(&tm_time);
//     } else {
//         DateTime now = rtc.now();
        
//         // ✅ Oblicz aktualny offset dla tej daty
//         long offset = getPolandTimezoneOffset(
//             now.year(), now.month(), now.day(), now.hour()
//         );
        
//         // RTC ma czas lokalny, konwertuj na UTC
//         unsigned long localTimestamp = now.unixtime();
//         return localTimestamp - offset;
//     }
// }

// // 🔧 IMPROVED: Better status messages
// String getRTCInfo() {
//     if (!rtcInitialized) {
//         return "RTC not initialized";
//     }
    
//     if (useInternalRTC) {
//         if (rtcNeedsSync) {
//             return "ESP32 system time (needs NTP sync)";
//         }
//         return "ESP32 system time (fallback)";
//     } else {
//         if (rtcNeedsSync) {
//             return "DS3231 RTC (battery may be dead)";  // 🆕 Clear warning!
//         }
//         return "DS3231 RTC (synchronized)";
//     }
// }

// String getTimeSourceInfo() {
//     return getRTCInfo();
// }

// bool isRTCHardware() {
//     return rtcInitialized && !useInternalRTC;
// }

// // 🆕 NEW: Check if battery issue was detected
// bool rtcNeedsSynchronization() {
//     return rtcNeedsSync;
// }









#include "rtc_controller.h"
#include "../core/logging.h"
#include "../hardware/hardware_pins.h"
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <time.h>
#include "../network/wifi_manager.h"

// NTP Configuration
const char* NTP_SERVER = "216.239.35.0";

RTC_DS3231 rtc;
bool rtcInitialized = false;
bool useInternalRTC = false;
bool rtcNeedsSync = false;
bool batteryIssueDetected = false;  // 🆕 Osobna flaga dla problemu baterii!

long currentTimezoneOffset = 3600;  // Dynamiczny offset (domyślnie CET)

struct {
    int year = 2024;
    int month = 1;
    int day = 1;
    int hour = 12;
    int minute = 0;
    int second = 0;
    unsigned long lastUpdate = 0;
} internalTime;

unsigned long lastNTPSync = 0;
const unsigned long NTP_SYNC_INTERVAL = 3600000;

// ===== DST CALCULATION =====

int getDayOfWeek(int year, int month, int day) {
    if (month < 3) {
        month += 12;
        year--;
    }
    int q = day;
    int m = month;
    int k = year % 100;
    int j = year / 100;
    int h = (q + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
    return (h + 6) % 7;
}

int getLastSunday(int year, int month) {
    int daysInMonth;
    if (month == 2) {
        daysInMonth = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
    } else if (month == 4 || month == 6 || month == 9 || month == 11) {
        daysInMonth = 30;
    } else {
        daysInMonth = 31;
    }
    
    for (int day = daysInMonth; day >= 1; day--) {
        if (getDayOfWeek(year, month, day) == 0) {
            return day;
        }
    }
    return daysInMonth;
}

bool isDSTActive(int year, int month, int day, int hour) {
    int marchLastSunday = getLastSunday(year, 3);
    int octoberLastSunday = getLastSunday(year, 10);
    
    if (month < 3 || month > 10) return false;
    if (month > 3 && month < 10) return true;
    
    if (month == 3) {
        if (day < marchLastSunday) return false;
        if (day > marchLastSunday) return true;
        return (hour >= 2);
    }
    
    if (month == 10) {
        if (day < octoberLastSunday) return true;
        if (day > octoberLastSunday) return false;
        return (hour < 3);
    }
    
    return false;
}

long getPolandTimezoneOffset(int year, int month, int day, int hour) {
    if (isDSTActive(year, month, day, hour)) {
        return 7200;
    } else {
        return 3600;
    }
}

long getCurrentPolandOffset() {
    time_t now;
    time(&now);
    struct tm* utc_time = gmtime(&now);
    
    return getPolandTimezoneOffset(
        utc_time->tm_year + 1900,
        utc_time->tm_mon + 1,
        utc_time->tm_mday,
        utc_time->tm_hour
    );
}

// ===== HELPER FUNCTIONS =====

void initInternalTimeFromCompileTime() {
    char month_str[4];
    int day, year, hour, min, sec;
    sscanf(__DATE__, "%s %d %d", month_str, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);
    
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int month = 1;
    for (int i = 0; i < 12; i++) {
        if (strncmp(month_str, months[i], 3) == 0) {
            month = i + 1;
            break;
        }
    }
    
    internalTime.year = year;
    internalTime.month = month;
    internalTime.day = day;
    internalTime.hour = hour;
    internalTime.minute = min;
    internalTime.second = sec;
    internalTime.lastUpdate = millis();
    
    LOG_INFO("Internal RTC set to compile time");
}

bool syncTimeFromNTP() {
    if (!isWiFiConnected()) {
        LOG_WARNING("Cannot sync NTP - WiFi not connected");
        return false;
    }


        int year, month, day, hour;
    
    if (rtcInitialized && !useInternalRTC) {
        // Use RTC time to determine offset
        DateTime now = rtc.now();
        year = now.year();
        month = now.month();
        day = now.day();
        hour = now.hour();
    } else {
        // Use compile time as fallback
        char month_str[4];
        sscanf(__DATE__, "%s %d %d", month_str, &day, &year);
        sscanf(__TIME__, "%d:%d:%d", &hour, &month, &month);  // hour tylko
        
        const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        month = 1;
        for (int i = 0; i < 12; i++) {
            if (strncmp(month_str, months[i], 3) == 0) {
                month = i + 1;
                break;
            }
        }
    }
    
    // 🔧 Update timezone offset BEFORE NTP sync
    currentTimezoneOffset = getCurrentPolandOffset();
    LOG_INFO("Timezone offset: UTC%+d (%s)", 
             currentTimezoneOffset / 3600,
             (currentTimezoneOffset == 7200) ? "CEST" : "CET");
    
    LOG_INFO("Attempting NTP synchronization from %s...", NTP_SERVER);
    
    configTime(currentTimezoneOffset, 0, NTP_SERVER);
    
    int attempts = 0;
    time_t now = 0;
    struct tm timeinfo;
    
    while (attempts < 30) {
        time(&now);
        if (now > 1600000000) {
            localtime_r(&now, &timeinfo);
            
            char timeStr[64];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
            LOG_INFO("NTP sync successful: %s (Poland local time)", timeStr);
            
            lastNTPSync = millis();
            return true;
        }
        delay(500);
        attempts++;
    }
    
    LOG_ERROR("NTP synchronization failed after 15 seconds");
    return false;
}

bool setRTCFromNTP() {
    if (!syncTimeFromNTP()) {
        return false;
    }
    
    time_t ntp_time;
    time(&ntp_time);
    
    // 🔧 CRITICAL: Add offset to store LOCAL time in RTC
    LOG_INFO("NTP returned UTC timestamp: %lu", (unsigned long)ntp_time);
    ntp_time += currentTimezoneOffset;
    LOG_INFO("Adding offset %ld seconds -> Local timestamp: %lu", 
             currentTimezoneOffset, (unsigned long)ntp_time);
    
    rtc.adjust(DateTime(ntp_time));
    
    DateTime newTime = rtc.now();
    LOG_INFO("RTC now contains: %04d-%02d-%02d %02d:%02d:%02d (Poland local)",
             newTime.year(), newTime.month(), newTime.day(),
             newTime.hour(), newTime.minute(), newTime.second());
    
    return true;
}

void periodicNTPSync() {
    if (!rtcInitialized || useInternalRTC) {
        return;
    }
    
    if (!isWiFiConnected()) {
        return;
    }
    
    unsigned long now = millis();
    if (now - lastNTPSync >= NTP_SYNC_INTERVAL) {
        LOG_INFO("Periodic NTP synchronization...");
        
        if (setRTCFromNTP()) {
            LOG_INFO("Periodic sync successful");
            rtcNeedsSync = false;  // Reset flag po udanym periodic sync
        } else {
            LOG_WARNING("Periodic sync failed - will retry in 1 hour");
            lastNTPSync = now;
        }
    }
}

void initializeRTC() {
    LOG_INFO("Starting RTC initialization...");
    
    // 🔧 Reset flags at start
    rtcNeedsSync = false;
    batteryIssueDetected = false;
    currentTimezoneOffset = 3600;  // Default CET
    
    LOG_INFO("Attempting to initialize external DS3231 RTC...");
    
    Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
    Wire.setClock(100000);
    
    Wire.beginTransmission(0x68);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
        LOG_INFO("DS3231 detected on I2C bus, attempting initialization...");
        
        if (rtc.begin()) {

            // DateTime now = rtc.now();
        
            // // Early detection of completely invalid time
            // if (now.year() < 2024) {
            //     LOG_ERROR("RTC date too old (%d) - likely battery dead", now.year());
            //     LOG_ERROR("Forcing restart in 10s to retry with fresh data...");
            //     delay(10000);
            //     ESP.restart();
            // }
            
            // 🔧 Check battery/power loss (OSF flag)
            if (rtc.lostPower()) {
                LOG_WARNING("RTC lost power - battery dead or removed");
                LOG_INFO("Setting RTC to compile time to clear OSF flag...");
                
                batteryIssueDetected = true;  // 🆕 Battery problem!
                rtcNeedsSync = true;

                DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
                rtc.adjust(compileTime);
                delay(100);

                if (rtc.lostPower()) {
                    LOG_ERROR("Failed to clear RTC OSF flag - hardware problem");
                    useInternalRTC = true;
                    rtcInitialized = true;
                    initInternalTimeFromCompileTime();
                    return;
                }
                
                LOG_INFO("OSF flag cleared");
                LOG_WARNING("⚠️ Battery issue detected - replace CR2032 battery");
                
                LOG_INFO("Attempting immediate NTP sync after battery loss...");
                
                if (setRTCFromNTP()) {
                    LOG_INFO("RTC synchronized with NTP");
                    LOG_WARNING("Battery warning will remain until battery is replaced");
                    rtcInitialized = true;
                    useInternalRTC = false;
                    return;
                } else {
                    LOG_WARNING("NTP sync failed - will retry periodically");
                }
            }

            // Verify RTC time
            DateTime now = rtc.now();
            DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
            
            LOG_INFO("RTC current time: %04d-%02d-%02d %02d:%02d:%02d", 
                     now.year(), now.month(), now.day(),
                     now.hour(), now.minute(), now.second());
            LOG_INFO("Compile time:     %04d-%02d-%02d %02d:%02d:%02d",
                     compileTime.year(), compileTime.month(), compileTime.day(),
                     compileTime.hour(), compileTime.minute(), compileTime.second());

            // 🔧 Check if RTC older than compile time (NOT a battery issue!)
            if (now.unixtime() <= compileTime.unixtime() + 60) {
                LOG_WARNING("RTC time is NOT newer than compile time!");
                LOG_INFO("Attempting NTP sync to update time...");
                
                rtcNeedsSync = true;  // Temporary - will be reset if sync succeeds

                if (setRTCFromNTP()) {
                    LOG_INFO("RTC successfully updated from NTP");
                    rtcNeedsSync = false;  // 🔧 Reset flag - not a battery issue!
                    rtcInitialized = true;
                    useInternalRTC = false;
                    return;
                } else {
                    LOG_WARNING("NTP sync failed - using RTC with stale time");
                }
            }

            // Validate time components
            bool timeValid = true;
            if (now.year() < 2020 || now.year() > 2035) {
                LOG_ERROR("RTC year invalid: %d", now.year());
                timeValid = false;
            }
            if (now.month() < 1 || now.month() > 12) {
                LOG_ERROR("RTC month invalid: %d", now.month());
                timeValid = false;
            }
            if (now.day() < 1 || now.day() > 31) {
                LOG_ERROR("RTC day invalid: %d", now.day());
                timeValid = false;
            }

            if (!timeValid) {
                LOG_ERROR("RTC has invalid time components");
                rtcNeedsSync = true;
                
                if (setRTCFromNTP()) {
                    LOG_INFO("RTC restored from NTP");
                    rtcNeedsSync = false;  // Reset if not battery issue
                    rtcInitialized = true;
                    useInternalRTC = false;
                    return;
                } else {
                    LOG_WARNING("NTP sync failed, switching to fallback");
                    useInternalRTC = true;
                    rtcInitialized = true;
                    initInternalTimeFromCompileTime();
                    return;
                }
            }

            // RTC valid
            LOG_INFO("RTC time verification successful");
            rtcInitialized = true;
            useInternalRTC = false;
            
            if (!batteryIssueDetected && !rtcNeedsSync) {
                LOG_INFO("RTC appears accurate and battery OK");
            }
            
            return;

        } else {
            LOG_ERROR("Failed to initialize DS3231 RTC");
        }

    } else {
        LOG_WARNING("DS3231 not found on I2C bus (error: %d)", error);
    }
    
    // Fallback
    LOG_INFO("Falling back to ESP32 system time");
    useInternalRTC = true;
    rtcInitialized = true;
    rtcNeedsSync = true;
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) {
        LOG_WARNING("Failed to get system time, using compile time");
        initInternalTimeFromCompileTime();
    } else {
        internalTime.year = timeinfo.tm_year + 1900;
        internalTime.month = timeinfo.tm_mon + 1;
        internalTime.day = timeinfo.tm_mday;
        internalTime.hour = timeinfo.tm_hour;
        internalTime.minute = timeinfo.tm_min;
        internalTime.second = timeinfo.tm_sec;
        internalTime.lastUpdate = millis();
        
        LOG_INFO("Internal RTC set from system time");
    }
}

String getCurrentTimestamp() {
    if (!rtcInitialized) {
        return "RTC_NOT_INITIALIZED";
    }
    
    if (useInternalRTC) {
        unsigned long now = millis();
        unsigned long elapsed = (now - internalTime.lastUpdate) / 1000;
        
        if (elapsed > 0) {
            internalTime.second += elapsed;
            while (internalTime.second >= 60) {
                internalTime.second -= 60;
                internalTime.minute++;
                if (internalTime.minute >= 60) {
                    internalTime.minute = 0;
                    internalTime.hour++;
                    if (internalTime.hour >= 24) {
                        internalTime.hour = 0;
                        internalTime.day++;
                        if (internalTime.day > 28) {
                            internalTime.day = 1;
                            internalTime.month++;
                            if (internalTime.month > 12) {
                                internalTime.month = 1;
                                internalTime.year++;
                            }
                        }
                    }
                }
            }
            internalTime.lastUpdate = now;
        }
        
        char timeBuffer[32];
        snprintf(timeBuffer, sizeof(timeBuffer), "%04d-%02d-%02d %02d:%02d:%02d",
                 internalTime.year, internalTime.month, internalTime.day,
                 internalTime.hour, internalTime.minute, internalTime.second);
        
        return String(timeBuffer);
    } else {
        DateTime now = rtc.now();
        
        char timeBuffer[32];
        snprintf(timeBuffer, sizeof(timeBuffer), "%04d-%02d-%02d %02d:%02d:%02d",
                 now.year(), now.month(), now.day(),
                 now.hour(), now.minute(), now.second());
        
        return String(timeBuffer);
    }
}

bool isRTCWorking() {
    if (!rtcInitialized) {
        return false;
    }
    
    if (useInternalRTC) {
        return true;
    } else {
        DateTime now = rtc.now();
        return (now.year() >= 2020 && now.year() <= 2035);
    }
}

unsigned long getUnixTimestamp() {
    if (!rtcInitialized) {
        return 1609459200;
    }
    
    if (useInternalRTC) {
        struct tm tm_time;
        tm_time.tm_year = internalTime.year - 1900;
        tm_time.tm_mon = internalTime.month - 1;
        tm_time.tm_mday = internalTime.day;
        tm_time.tm_hour = internalTime.hour;
        tm_time.tm_min = internalTime.minute;
        tm_time.tm_sec = internalTime.second;
        tm_time.tm_isdst = 0;
        
        return mktime(&tm_time);
    } else {
        DateTime now = rtc.now();
        
        // Calculate offset for this specific date/time
        long offset = getPolandTimezoneOffset(
            now.year(), now.month(), now.day(), now.hour()
        );
        
        // RTC has local time, convert to UTC
        unsigned long localTimestamp = now.unixtime();
        return localTimestamp - offset;
    }
}

String getRTCInfo() {
    if (!rtcInitialized) {
        return "RTC not initialized";
    }
    
    if (useInternalRTC) {
        if (rtcNeedsSync) {
            return "ESP32 system time (needs NTP sync)";
        }
        return "ESP32 system time (fallback)";
    } else {
        if (batteryIssueDetected) {
            return "DS3231 RTC (battery may be dead)";
        } else if (rtcNeedsSync) {
            return "DS3231 RTC (syncing...)";
        }
        return "DS3231 RTC (synchronized)";
    }
}

String getTimeSourceInfo() {
    return getRTCInfo();
}

bool isRTCHardware() {
    return rtcInitialized && !useInternalRTC;
}

bool rtcNeedsSynchronization() {
    return batteryIssueDetected;  // 🔧 Return battery flag, not sync flag!
}