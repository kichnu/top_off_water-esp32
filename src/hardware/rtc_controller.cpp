#include "rtc_controller.h"
#include "../core/logging.h"
#include "../hardware/hardware_pins.h"
#include <Wire.h>
#include <RTClib.h>

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
            LOG_INFO("DS3231 RTC initialized successfully");
            
            // Check if RTC lost power
            if (rtc.lostPower()) {
                LOG_WARNING("RTC lost power, setting time from compile time");
                rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            }
            
            // Verify RTC is working by reading time
            DateTime now = rtc.now();
            if (now.year() >= 2020 && now.year() <= 2030) {
                rtcInitialized = true;
                useInternalRTC = false;
                LOG_INFO("External RTC verification successful");
                LOG_INFO("Current time: %s", getCurrentTimestamp().c_str());
                return;
            } else {
                LOG_ERROR("RTC time invalid: %d-%02d-%02d", now.year(), now.month(), now.day());
            }
        } else {
            LOG_ERROR("Failed to initialize DS3231 RTC");
        }
    } else {
        LOG_WARNING("DS3231 not found on I2C bus (error: %d)", error);
    }
    
    // Fallback to ESP32 internal RTC
    LOG_INFO("Falling back to ESP32 internal RTC");
    useInternalRTC = true;
    rtcInitialized = true;
    
    // Set initial time from compile time
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
        snprintf(timeBuffer, sizeof(timeBuffer), "%04d-%02d-%02d %02d:%02d:%02d (INT)",
                 internalTime.year, internalTime.month, internalTime.day,
                 internalTime.hour, internalTime.minute, internalTime.second);
        
        return String(timeBuffer);
    } else {
        // Use external DS3231
        DateTime now = rtc.now();
        
        char timeBuffer[32];
        snprintf(timeBuffer, sizeof(timeBuffer), "%04d-%02d-%02d %02d:%02d:%02d (EXT)",
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