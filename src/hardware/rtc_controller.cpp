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
const long GMT_OFFSET_SEC = 3600;      // UTC+1 (CET) - dostosuj do swojej strefy
const int DAYLIGHT_OFFSET_SEC = 0;     // Bez DST

RTC_DS3231 rtc;
bool rtcInitialized = false;
bool useInternalRTC = false;

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


// 🆕 NEW: Synchronize time with NTP server
bool syncTimeFromNTP() {
    if (!isWiFiConnected()) {
        LOG_WARNING("Cannot sync NTP - WiFi not connected");
        return false;
    }
    
    LOG_INFO("Attempting NTP synchronization from %s...", NTP_SERVER);
    
    // Configure NTP
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
    // Wait for NTP sync (max 10 seconds)
    int attempts = 0;
    time_t now = 0;
    struct tm timeinfo;
    
    while (attempts < 40) {  // 40 * 500ms = 20 seconds
        time(&now);
        if (now > 1600000000) {  // Valid timestamp (after 2020)
            localtime_r(&now, &timeinfo);
            
            char timeStr[64];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
            LOG_INFO("✅ NTP sync successful: %s", timeStr);
            
            return true;
        }
        delay(500);
        attempts++;
    }
    
    LOG_ERROR("❌ NTP synchronization failed after 10 seconds");
    return false;
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
                LOG_WARNING("RTC lost power detected - battery was removed or died");
                LOG_INFO("Attempting to restore RTC from compile time...");

                // Try to set RTC time from compile time to clear OSF flag
                DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
                rtc.adjust(compileTime);

                delay(100);  // Give RTC time to update

                // Check if OSF flag was cleared
                if (rtc.lostPower()) {
                    // OSF still set - RTC hardware problem or battery really dead
                    LOG_ERROR("Failed to clear RTC OSF flag - switching to system time fallback");

                    useInternalRTC = true;
                    rtcInitialized = true;

                    // Initialize internal time
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

                    LOG_INFO("System time fallback active");
                    return;
                } else {
                    // OSF cleared successfully - RTC restored!
                    LOG_INFO("✅ RTC OSF flag cleared - DS3231 restored from compile time");
                    LOG_WARNING("⚠️ RTC time may be inaccurate (compile time: %s)", 
                               compileTime.timestamp().c_str());
                    // Continue with normal RTC verification below
                }
            }

            // Verify RTC time is reasonable
            DateTime now = rtc.now();
            LOG_INFO("RTC current time: %04d-%02d-%02d %02d:%02d:%02d", 
                     now.year(), now.month(), now.day(),
                     now.hour(), now.minute(), now.second());


            // 🆕 NEW: Check if RTC is not older than compile time
            DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
            if (now.unixtime() < compileTime.unixtime()) {
                LOG_WARNING("⚠️ RTC time is OLDER than compile time!");
                LOG_WARNING("   RTC:     %s", now.timestamp().c_str());
                LOG_WARNING("   Compile: %s", compileTime.timestamp().c_str());
                LOG_WARNING("   This indicates RTC lost power or has stale time");
                LOG_INFO("Attempting to fix RTC with NTP...");

                if (syncTimeFromNTP()) {
                    time_t ntp_time;
                    time(&ntp_time);
                    rtc.adjust(DateTime(ntp_time));

                    DateTime newTime = rtc.now();
                    LOG_INFO("✅ RTC restored from NTP: %04d-%02d-%02d %02d:%02d:%02d",
                             newTime.year(), newTime.month(), newTime.day(),
                             newTime.hour(), newTime.minute(), newTime.second());
                    
                    rtcInitialized = true;
                    useInternalRTC = false;
                    LOG_INFO("External RTC verification successful");
                    return;
                } else {
                    LOG_WARNING("NTP sync failed, switching to system time fallback");
                    useInternalRTC = true;
                    rtcInitialized = true;

                    // Initialize from compile time
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

                    LOG_INFO("System time fallback active from compile time");
                    return;
                }
            }         












            
            // Check all time components
            bool timeValid = true;
            if (now.year() < 2020 || now.year() > 2030) {
                LOG_ERROR("RTC year invalid: %d (expected 2020-2030)", now.year());
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
                
                // Try NTP sync first
                if (syncTimeFromNTP()) {
                    // NTP sync successful - set RTC
                    time_t ntp_time;
                    time(&ntp_time);
                    
                    rtc.adjust(DateTime(ntp_time));
                    
                    DateTime newTime = rtc.now();
                    LOG_INFO("✅ RTC restored from NTP: %04d-%02d-%02d %02d:%02d:%02d",
                             newTime.year(), newTime.month(), newTime.day(),
                             newTime.hour(), newTime.minute(), newTime.second());
                    
                    // RTC now has valid time - use it!
                    rtcInitialized = true;
                    useInternalRTC = false;
                    LOG_INFO("External RTC verification successful");
                    return;
                } else {
                    // NTP failed - use system time fallback
                    LOG_WARNING("NTP sync failed, switching to system time fallback");
                    
                    useInternalRTC = true;
                    rtcInitialized = true;
                    
                    // Initialize from compile time
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
                    
                    LOG_INFO("System time fallback active from compile time");
                    return;
                }
            } else {
                // RTC time is VALID - use it!
                LOG_INFO("✅ RTC time verification successful");
                rtcInitialized = true;
                useInternalRTC = false;
                return;
            }

        } else {
            LOG_ERROR("Failed to initialize DS3231 RTC");
        }

    } else {
        LOG_WARNING("DS3231 not found on I2C bus (error: %d)", error);
    }
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) {
        LOG_WARNING("Failed to get system time, using compile time");
        // Parse compile time
        char month_str[4];
        int day, year, hour, min, sec;
        sscanf(__DATE__, "%s %d %d", month_str, &day, &year);
        sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);
        
        // Convert month string to number
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
        // Use external DS3231
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
        return (now.year() >= 2020 && now.year() <= 2030);
    }
}

unsigned long getUnixTimestamp() {
    if (!rtcInitialized) {
        return 1609459200; // Default: 2021-01-01
    }
    
    if (useInternalRTC) {
        // Convert internal time to Unix timestamp (approximate)
        // This is a simplified calculation
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
        return now.unixtime();
    }
}

// Additional diagnostic function
String getRTCInfo() {
    if (!rtcInitialized) {
        return "RTC not initialized";
    }
    
    if (useInternalRTC) {
        return "Using ESP32 internal RTC (fallback mode)";
    } else {
        return "Using DS3231 external RTC";
    }
}

String getTimeSourceInfo() {
    if (!rtcInitialized) {
        return "RTC not initialized";
    }
    
    if (useInternalRTC) {
        return "Using ESP32 system time";
    } else {
        return "Using DS3231 external RTC";
    }
}

// 🆕 NEW: Check if using hardware RTC
bool isRTCHardware() {
    return rtcInitialized && !useInternalRTC;
}