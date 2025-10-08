#include "time_cache.h"
#include "rtc_controller.h"
#include "../core/logging.h"

// ===============================
// GLOBAL TIME CACHE INSTANCE
// ===============================

TimeCache globalTimeCache = {
    .currentDate = "2025-01-01",  // Safe default
    .unixTimestamp = 0,
    .lastRTCRead = 0,
    .bootTimestamp = 0,
    .isValid = false
};

// ===============================
// CACHE REFRESH INTERVAL
// ===============================
const uint32_t CACHE_REFRESH_INTERVAL = 3600000UL;  // 1 hour

// ===============================
// INITIALIZATION
// ===============================

void initTimeCache() {
    LOG_INFO("====================================");
    LOG_INFO("INITIALIZING TIME CACHE");
    LOG_INFO("====================================");
    
    // 🆕 RETRY MECHANISM - 3 próby z 500ms delay
    const int MAX_INIT_RETRIES = 3;
    bool success = false;
    
    for (int attempt = 0; attempt < MAX_INIT_RETRIES; attempt++) {
        if (attempt > 0) {
            LOG_WARNING("Retrying time cache init (attempt %d/%d)...", attempt + 1, MAX_INIT_RETRIES);
            delay(500);  // Wait 500ms between attempts
        }
        
        // Read from RTC
        String timestamp = getCurrentTimestamp();
        
        // Check if valid
        if (timestamp != "RTC_ERROR" && 
            timestamp != "RTC_NOT_INITIALIZED" && 
            timestamp.length() >= 10) {
            
            // Extract date
            strncpy(globalTimeCache.currentDate, timestamp.c_str(), 10);
            globalTimeCache.currentDate[10] = '\0';
            
            // Validate year
            int year = String(globalTimeCache.currentDate).substring(0, 4).toInt();
            if (year >= 2024 && year <= 2030) {
                // SUCCESS!
                globalTimeCache.unixTimestamp = getUnixTimestamp();
                globalTimeCache.lastRTCRead = millis();
                globalTimeCache.bootTimestamp = globalTimeCache.unixTimestamp;
                globalTimeCache.isValid = true;
                
                LOG_INFO("✅ Time cache initialized successfully:");
                LOG_INFO("  Date: %s", globalTimeCache.currentDate);
                LOG_INFO("  Unix Time: %lu", (unsigned long)globalTimeCache.unixTimestamp);
                LOG_INFO("  RTC Status: %s", getRTCInfo().c_str());
                LOG_INFO("  Attempt: %d/%d", attempt + 1, MAX_INIT_RETRIES);
                LOG_INFO("====================================");
                
                success = true;
                break;
            } else {
                LOG_WARNING("Invalid year from RTC: %d (attempt %d/%d)", year, attempt + 1, MAX_INIT_RETRIES);
            }
        } else {
            LOG_WARNING("RTC not ready: '%s' (attempt %d/%d)", timestamp.c_str(), attempt + 1, MAX_INIT_RETRIES);
        }
    }
    
    if (!success) {
        // All retries failed - use fallback
        LOG_ERROR("❌ Failed to initialize time cache after %d attempts", MAX_INIT_RETRIES);
        LOG_ERROR("Using fallback date: 2025-01-01");
        LOG_ERROR("RTC Info: %s", getRTCInfo().c_str());
        
        strcpy(globalTimeCache.currentDate, "2025-01-01");
        globalTimeCache.unixTimestamp = 1735689600;  // 2025-01-01 00:00:00 UTC
        globalTimeCache.lastRTCRead = millis();
        globalTimeCache.bootTimestamp = globalTimeCache.unixTimestamp;
        globalTimeCache.isValid = false;  // Mark as INVALID
        
        LOG_ERROR("====================================");
    }
}

// ===============================
// CACHE ACCESS (O(1) - NO I2C)
// ===============================

const char* getCachedDate() {
    // Pure O(1) lookup - no I2C
    return globalTimeCache.currentDate;
}

uint32_t getCachedUnixTime() {
    // Return cached timestamp - no I2C
    return globalTimeCache.unixTimestamp;
}

uint32_t getApproxUnixTime() {
    // Estimate current time without RTC read
    uint32_t elapsedSeconds = (millis() - globalTimeCache.lastRTCRead) / 1000;
    return globalTimeCache.unixTimestamp + elapsedSeconds;
}

// ===============================
// CACHE REFRESH LOGIC
// ===============================

bool shouldRefreshCache() {
    // Refresh every hour
    return (millis() - globalTimeCache.lastRTCRead) >= CACHE_REFRESH_INTERVAL;
}

void updateTimeCache() {
    if (!globalTimeCache.isValid) {
        // First initialization
        initTimeCache();
        return;
    }
    
    LOG_INFO("Updating time cache from RTC...");
    
    String timestamp = getCurrentTimestamp();  // ← I2C read
    
    if (timestamp == "RTC_ERROR" || timestamp == "RTC_NOT_INITIALIZED" || timestamp.length() < 10) {
        LOG_WARNING("RTC read failed during cache update - keeping old cache");
        return;
    }
    
    // Extract new date
    char newDate[11];
    strncpy(newDate, timestamp.c_str(), 10);
    newDate[10] = '\0';
    
    // Check if date changed
    if (strcmp(newDate, globalTimeCache.currentDate) != 0) {
        LOG_INFO("====================================");
        LOG_INFO("📅 DATE CHANGE DETECTED IN CACHE");
        LOG_INFO("====================================");
        LOG_INFO("Old date: %s", globalTimeCache.currentDate);
        LOG_INFO("New date: %s", newDate);
        
        // Trigger callback for date change (można rozszerzyć)
        triggerDateChangeCallback(globalTimeCache.currentDate, newDate);
        
        // Update cache
        strcpy(globalTimeCache.currentDate, newDate);
        LOG_INFO("====================================");
    }
    
    // Update unix timestamp
    globalTimeCache.unixTimestamp = getUnixTimestamp();
    globalTimeCache.lastRTCRead = millis();
    
    LOG_INFO("✅ Time cache updated: %s (unix: %lu)", 
             globalTimeCache.currentDate, 
             (unsigned long)globalTimeCache.unixTimestamp);
}

// ===============================
// CALLBACK PLACEHOLDER
// ===============================

void triggerDateChangeCallback(const char* oldDate, const char* newDate) {
    // Placeholder - można podpiąć funkcję z water_algorithm.cpp
    // Na razie tylko log
    LOG_INFO("Date change callback: %s → %s", oldDate, newDate);
}