
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
// const long GMT_OFFSET_SEC = 7200;      // UTC+2 (Poland standard time)
// const int DAYLIGHT_OFFSET_SEC = 0;     // No DST adjustment

// RTC_DS3231 rtc;
// bool rtcInitialized = false;
// bool useInternalRTC = false;
// bool rtcNeedsSync = false;  // 🆕 Flag indicating RTC time may be inaccurate

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
// const unsigned long NTP_SYNC_INTERVAL = 3600000;  // 1 hour in milliseconds


// // 🔧 FIXED: Synchronize time with NTP server (single offset)
// bool syncTimeFromNTP() {
//     if (!isWiFiConnected()) {
//         LOG_WARNING("Cannot sync NTP - WiFi not connected");
//         return false;
//     }
    
//     LOG_INFO("Attempting NTP synchronization from %s...", NTP_SERVER);
    
//     // Configure NTP with timezone offset
//     configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
//     // Wait for NTP sync (max 15 seconds)
//     int attempts = 0;
//     time_t now = 0;
//     struct tm timeinfo;
    
//     while (attempts < 30) {  // 30 * 500ms = 15 seconds
//         time(&now);
//         if (now > 1600000000) {  // Valid timestamp (after 2020)
//             localtime_r(&now, &timeinfo);
            
//             char timeStr[64];
//             strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
//             LOG_INFO("✅ NTP sync successful: %s (local time)", timeStr);
            
//             lastNTPSync = millis();  // Record successful sync
//             return true;
//         }
//         delay(500);
//         attempts++;
//     }
    
//     LOG_ERROR("❌ NTP synchronization failed after 15 seconds");
//     return false;
// }

// // 🆕 NEW: Attempt to set RTC from NTP
// bool setRTCFromNTP() {
//     if (!syncTimeFromNTP()) {
//         return false;
//     }
    
//     // Get current time from NTP (already in local time due to configTime)
//     time_t ntp_time;
//     time(&ntp_time);
    
//     // Set RTC with NTP time (no additional offset needed!)
//     rtc.adjust(DateTime(ntp_time));
    
//     DateTime newTime = rtc.now();
//     LOG_INFO("✅ RTC updated from NTP: %04d-%02d-%02d %02d:%02d:%02d",
//              newTime.year(), newTime.month(), newTime.day(),
//              newTime.hour(), newTime.minute(), newTime.second());
    
//     rtcNeedsSync = false;  // Mark as synchronized
//     return true;
// }

// // 🆕 NEW: Periodic NTP sync (call from main loop)
// void periodicNTPSync() {
//     if (!rtcInitialized || useInternalRTC) {
//         return;  // Only sync hardware RTC
//     }
    
//     if (!isWiFiConnected()) {
//         return;  // Need WiFi for NTP
//     }
    
//     unsigned long now = millis();
//     if (now - lastNTPSync >= NTP_SYNC_INTERVAL) {
//         LOG_INFO("Periodic NTP synchronization...");
        
//         if (setRTCFromNTP()) {
//             LOG_INFO("Periodic sync successful");
//         } else {
//             LOG_WARNING("Periodic sync failed - will retry in 1 hour");
//             lastNTPSync = now;  // Prevent immediate retry
//         }
//     }
// }

// void initializeRTC() {
//     LOG_INFO("Starting RTC initialization...");
    
//     // Try external DS3231 first
//     LOG_INFO("Attempting to initialize external DS3231 RTC...");
    
//     // Initialize I2C with specific pins for ESP32-C3
//     Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
//     Wire.setClock(100000); // Set I2C clock to 100kHz for better stability
    
//     // Scan I2C bus for DS3231 (address 0x68)
//     Wire.beginTransmission(0x68);
//     byte error = Wire.endTransmission();
    
//     if (error == 0) {
//         LOG_INFO("DS3231 detected on I2C bus, attempting initialization...");
        
//         // Try to initialize DS3231
//         if (rtc.begin()) {

//             // 🔧 FIXED: Check if RTC lost power (OSF flag)
//             if (rtc.lostPower()) {
//                 LOG_WARNING("⚠️ RTC lost power - battery dead or removed");
//                 LOG_INFO("Setting RTC to compile time to clear OSF flag...");

//                 DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
//                 rtc.adjust(compileTime);
//                 delay(100);

//                 // Verify OSF cleared
//                 if (rtc.lostPower()) {
//                     LOG_ERROR("Failed to clear RTC OSF flag - hardware problem");
//                     LOG_INFO("Switching to system time fallback");
                    
//                     useInternalRTC = true;
//                     rtcInitialized = true;
//                     rtcNeedsSync = true;
                    
//                     // Initialize from compile time
//                     initInternalTimeFromCompileTime();
//                     return;
//                 }
                
//                 LOG_INFO("OSF flag cleared successfully");
//                 rtcNeedsSync = true;  // Mark that we need NTP sync
                
//                 // 🆕 FIXED: Immediately try NTP sync after battery loss
//                 LOG_INFO("Attempting immediate NTP sync after battery loss...");
                
//                 if (setRTCFromNTP()) {
//                     LOG_INFO("✅ RTC successfully synchronized with NTP");
//                     rtcInitialized = true;
//                     useInternalRTC = false;
//                     return;
//                 } else {
//                     LOG_WARNING("NTP sync failed - RTC has compile time (inaccurate!)");
//                     rtcNeedsSync = true;  // Keep trying later
//                 }
//             }

//             // Verify RTC time is reasonable
//             DateTime now = rtc.now();
//             DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
            
//             LOG_INFO("RTC current time: %04d-%02d-%02d %02d:%02d:%02d", 
//                      now.year(), now.month(), now.day(),
//                      now.hour(), now.minute(), now.second());
//             LOG_INFO("Compile time:     %04d-%02d-%02d %02d:%02d:%02d",
//                      compileTime.year(), compileTime.month(), compileTime.day(),
//                      compileTime.hour(), compileTime.minute(), compileTime.second());

//             // 🔧 FIXED: Check if RTC time is older than or equal to compile time
//             if (now.unixtime() <= compileTime.unixtime() + 60) {  // +60s tolerance
//                 LOG_WARNING("⚠️ RTC time is NOT newer than compile time!");
//                 LOG_WARNING("   This indicates stale/inaccurate time");
//                 LOG_INFO("Attempting to fix RTC with NTP...");
                
//                 rtcNeedsSync = true;

//                 if (setRTCFromNTP()) {
//                     LOG_INFO("✅ RTC successfully synchronized with NTP");
//                     rtcInitialized = true;
//                     useInternalRTC = false;
//                     return;
//                 } else {
//                     LOG_WARNING("NTP sync failed - using RTC with stale time");
//                     LOG_WARNING("Will retry NTP sync periodically");
//                     // Continue with RTC anyway, but mark as needing sync
//                 }
//             }

//             // Check time component validity
//             bool timeValid = true;
//             if (now.year() < 2020 || now.year() > 2035) {
//                 LOG_ERROR("RTC year invalid: %d (expected 2020-2035)", now.year());
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
//                     LOG_INFO("✅ RTC restored from NTP");
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

//             // RTC time looks valid
//             LOG_INFO("✅ RTC time verification successful");
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
    
//     // 🔧 DS3231 not available - use system time fallback
//     LOG_INFO("Falling back to ESP32 system time");
//     useInternalRTC = true;
//     rtcInitialized = true;
//     rtcNeedsSync = true;
    
//     struct tm timeinfo;
//     if (!getLocalTime(&timeinfo, 100)) {
//         LOG_WARNING("Failed to get system time, using compile time");
//         initInternalTimeFromCompileTime();
//     } else {
//         // Use system time if available
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

// // 🆕 Helper function to initialize internal time from compile time
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

// String getCurrentTimestamp() {
//     if (!rtcInitialized) {
//         return "RTC_NOT_INITIALIZED";
//     }
    
//     if (useInternalRTC) {
//         // Update internal time based on millis()
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
//                         // Simplified day overflow (not accounting for month lengths)
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
//         // Use external DS3231
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
//         return true; // Internal RTC is always "working"
//     } else {
//         // Test external RTC
//         DateTime now = rtc.now();
//         return (now.year() >= 2020 && now.year() <= 2035);
//     }
// }

// unsigned long getUnixTimestamp() {
//     if (!rtcInitialized) {
//         return 1609459200; // Default: 2021-01-01
//     }
    
//     if (useInternalRTC) {
//         // Convert internal time to Unix timestamp
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
//         return now.unixtime();
//     }
// }

// // 🔧 IMPROVED: More detailed RTC info
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
//             return "DS3231 RTC (⚠️ time may be inaccurate)";
//         }
//         return "DS3231 RTC (synchronized)";
//     }
// }

// String getTimeSourceInfo() {
//     return getRTCInfo();  // Alias for compatibility
// }

// // Check if using hardware RTC
// bool isRTCHardware() {
//     return rtcInitialized && !useInternalRTC;
// }

// // 🆕 NEW: Check if RTC needs synchronization
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
const long GMT_OFFSET_SEC = 7200;      // UTC+2 (Poland standard time)
const int DAYLIGHT_OFFSET_SEC = 0;     // No DST adjustment

RTC_DS3231 rtc;
bool rtcInitialized = false;
bool useInternalRTC = false;
bool rtcNeedsSync = false;  // Flag indicating RTC time may be inaccurate

// Fallback time structure for internal RTC
struct {
    int year = 2024;
    int month = 1;
    int day = 1;
    int hour = 12;
    int minute = 0;
    int second = 0;
    unsigned long lastUpdate = 0;
} internalTime;

// Last NTP sync timestamp for periodic updates
unsigned long lastNTPSync = 0;
const unsigned long NTP_SYNC_INTERVAL = 3600000;  // 1 hour in milliseconds


// Helper function to initialize internal time from compile time
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
    
    LOG_INFO("Internal RTC set to compile time: %s", getCurrentTimestamp().c_str());
}

// Synchronize time with NTP server
bool syncTimeFromNTP() {
    if (!isWiFiConnected()) {
        LOG_WARNING("Cannot sync NTP - WiFi not connected");
        return false;
    }
    
    LOG_INFO("Attempting NTP synchronization from %s...", NTP_SERVER);
    
    // Configure NTP with timezone offset (for display purposes only)
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
    // Wait for NTP sync (max 15 seconds)
    int attempts = 0;
    time_t now = 0;
    struct tm timeinfo;
    
    while (attempts < 30) {  // 30 * 500ms = 15 seconds
        time(&now);
        if (now > 1600000000) {  // Valid timestamp (after 2020)
            localtime_r(&now, &timeinfo);
            
            char timeStr[64];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
            LOG_INFO("NTP sync successful: %s (local time)", timeStr);
            
            lastNTPSync = millis();  // Record successful sync
            return true;
        }
        delay(500);
        attempts++;
    }
    
    LOG_ERROR("NTP synchronization failed after 15 seconds");
    return false;
}

// Attempt to set RTC from NTP with proper timezone handling
bool setRTCFromNTP() {
    if (!syncTimeFromNTP()) {
        return false;
    }
    
    // Get current time from NTP (always returns UTC timestamp)
    time_t ntp_time;
    time(&ntp_time);
    
    // CRITICAL: Add timezone offset to store LOCAL time in RTC
    // The RTC will store Poland local time, not UTC
    ntp_time += GMT_OFFSET_SEC;
    
    // Set RTC with LOCAL time
    rtc.adjust(DateTime(ntp_time));
    
    DateTime newTime = rtc.now();
    LOG_INFO("RTC updated from NTP (local time): %04d-%02d-%02d %02d:%02d:%02d",
             newTime.year(), newTime.month(), newTime.day(),
             newTime.hour(), newTime.minute(), newTime.second());
    
    rtcNeedsSync = false;  // Mark as synchronized
    return true;
}

// Periodic NTP sync (call from main loop)
void periodicNTPSync() {
    if (!rtcInitialized || useInternalRTC) {
        return;  // Only sync hardware RTC
    }
    
    if (!isWiFiConnected()) {
        return;  // Need WiFi for NTP
    }
    
    unsigned long now = millis();
    if (now - lastNTPSync >= NTP_SYNC_INTERVAL) {
        LOG_INFO("Periodic NTP synchronization...");
        
        if (setRTCFromNTP()) {
            LOG_INFO("Periodic sync successful");
        } else {
            LOG_WARNING("Periodic sync failed - will retry in 1 hour");
            lastNTPSync = now;  // Prevent immediate retry
        }
    }
}

void initializeRTC() {
    LOG_INFO("Starting RTC initialization...");
    
    // Try external DS3231 first
    LOG_INFO("Attempting to initialize external DS3231 RTC...");
    
    // Initialize I2C with specific pins for ESP32-C3
    Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
    Wire.setClock(100000); // Set I2C clock to 100kHz for better stability
    
    // Scan I2C bus for DS3231 (address 0x68)
    Wire.beginTransmission(0x68);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
        LOG_INFO("DS3231 detected on I2C bus, attempting initialization...");
        
        // Try to initialize DS3231
        if (rtc.begin()) {

            // Check if RTC lost power (OSF flag)
            if (rtc.lostPower()) {
                LOG_WARNING("RTC lost power - battery dead or removed");
                LOG_INFO("Setting RTC to compile time to clear OSF flag...");

                DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
                rtc.adjust(compileTime);
                delay(100);

                // Verify OSF cleared
                if (rtc.lostPower()) {
                    LOG_ERROR("Failed to clear RTC OSF flag - hardware problem");
                    LOG_INFO("Switching to system time fallback");
                    
                    useInternalRTC = true;
                    rtcInitialized = true;
                    rtcNeedsSync = true;
                    
                    initInternalTimeFromCompileTime();
                    return;
                }
                
                LOG_INFO("OSF flag cleared successfully");
                rtcNeedsSync = true;  // Mark that we need NTP sync
                
                // Immediately try NTP sync after battery loss
                LOG_INFO("Attempting immediate NTP sync after battery loss...");
                
                if (setRTCFromNTP()) {
                    LOG_INFO("RTC successfully synchronized with NTP");
                    rtcInitialized = true;
                    useInternalRTC = false;
                    return;
                } else {
                    LOG_WARNING("NTP sync failed - RTC has compile time (inaccurate!)");
                    LOG_WARNING("Will retry NTP sync periodically");
                    rtcNeedsSync = true;  // Keep trying later
                }
            }

            // Verify RTC time is reasonable
            // Both times are now in LOCAL timezone (Poland)
            DateTime now = rtc.now();
            DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
            
            LOG_INFO("RTC current time: %04d-%02d-%02d %02d:%02d:%02d", 
                     now.year(), now.month(), now.day(),
                     now.hour(), now.minute(), now.second());
            LOG_INFO("Compile time:     %04d-%02d-%02d %02d:%02d:%02d",
                     compileTime.year(), compileTime.month(), compileTime.day(),
                     compileTime.hour(), compileTime.minute(), compileTime.second());

            // Check if RTC time is older than or equal to compile time
            // Now both are in local time, so comparison makes sense
            if (now.unixtime() <= compileTime.unixtime() + 60) {  // +60s tolerance
                LOG_WARNING("RTC time is NOT newer than compile time!");
                LOG_WARNING("   This indicates stale/inaccurate time");
                LOG_INFO("Attempting to fix RTC with NTP...");
                
                rtcNeedsSync = true;

                if (setRTCFromNTP()) {
                    LOG_INFO("RTC successfully synchronized with NTP");
                    rtcInitialized = true;
                    useInternalRTC = false;
                    return;
                } else {
                    LOG_WARNING("NTP sync failed - using RTC with stale time");
                    LOG_WARNING("Will retry NTP sync periodically");
                    // Continue with RTC anyway, but mark as needing sync
                }
            }

            // Check time component validity
            bool timeValid = true;
            if (now.year() < 2020 || now.year() > 2035) {
                LOG_ERROR("RTC year invalid: %d (expected 2020-2035)", now.year());
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
                LOG_INFO("Attempting to fix RTC with NTP time...");
                
                rtcNeedsSync = true;
                
                if (setRTCFromNTP()) {
                    LOG_INFO("RTC restored from NTP");
                    rtcInitialized = true;
                    useInternalRTC = false;
                    return;
                } else {
                    LOG_WARNING("NTP sync failed, switching to system time fallback");
                    useInternalRTC = true;
                    rtcInitialized = true;
                    initInternalTimeFromCompileTime();
                    return;
                }
            }

            // RTC time looks valid
            LOG_INFO("RTC time verification successful");
            rtcInitialized = true;
            useInternalRTC = false;
            
            if (!rtcNeedsSync) {
                LOG_INFO("RTC time appears accurate");
            } else {
                LOG_WARNING("RTC time may be inaccurate - will sync with NTP when possible");
            }
            
            return;

        } else {
            LOG_ERROR("Failed to initialize DS3231 RTC");
        }

    } else {
        LOG_WARNING("DS3231 not found on I2C bus (error: %d)", error);
    }
    
    // DS3231 not available - use system time fallback
    LOG_INFO("Falling back to ESP32 system time");
    useInternalRTC = true;
    rtcInitialized = true;
    rtcNeedsSync = true;
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) {
        LOG_WARNING("Failed to get system time, using compile time");
        initInternalTimeFromCompileTime();
    } else {
        // Use system time if available
        internalTime.year = timeinfo.tm_year + 1900;
        internalTime.month = timeinfo.tm_mon + 1;
        internalTime.day = timeinfo.tm_mday;
        internalTime.hour = timeinfo.tm_hour;
        internalTime.minute = timeinfo.tm_min;
        internalTime.second = timeinfo.tm_sec;
        internalTime.lastUpdate = millis();
        
        LOG_INFO("Internal RTC set from system time: %s", getCurrentTimestamp().c_str());
    }
}

String getCurrentTimestamp() {
    if (!rtcInitialized) {
        return "RTC_NOT_INITIALIZED";
    }
    
    if (useInternalRTC) {
        // Update internal time based on millis()
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
                        // Simplified day overflow (not accounting for month lengths)
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
        // Use external DS3231 (stores local time)
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
        return true; // Internal RTC is always "working"
    } else {
        // Test external RTC
        DateTime now = rtc.now();
        return (now.year() >= 2020 && now.year() <= 2035);
    }
}

unsigned long getUnixTimestamp() {
    if (!rtcInitialized) {
        return 1609459200; // Default: 2021-01-01
    }
    
    if (useInternalRTC) {
        // Convert internal time to Unix timestamp
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
        // RTC stores local time, but Unix timestamp should be UTC
        DateTime now = rtc.now();
        unsigned long localTimestamp = now.unixtime();
        
        // Convert local timestamp back to UTC by subtracting offset
        return localTimestamp - GMT_OFFSET_SEC;
    }
}

// More detailed RTC info
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
        if (rtcNeedsSync) {
            return "DS3231 RTC (time may be inaccurate)";
        }
        return "DS3231 RTC (synchronized)";
    }
}

String getTimeSourceInfo() {
    return getRTCInfo();  // Alias for compatibility
}

// Check if using hardware RTC
bool isRTCHardware() {
    return rtcInitialized && !useInternalRTC;
}

// Check if RTC needs synchronization
bool rtcNeedsSynchronization() {
    return rtcNeedsSync;
}