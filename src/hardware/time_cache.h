#ifndef TIME_CACHE_H
#define TIME_CACHE_H

#include <Arduino.h>

// ===============================
// TIME CACHE - SINGLE SOURCE OF TRUTH
// ===============================

struct TimeCache {
    char currentDate[12];        // "YYYY-MM-DD\0"
    uint32_t unixTimestamp;      // UTC timestamp
    uint32_t lastRTCRead;        // millis() ostatniego odczytu
    uint32_t bootTimestamp;      // Unix timestamp przy starcie
    bool isValid;                // Czy cache jest ważny
};

extern TimeCache globalTimeCache;

// ===============================
// PUBLIC API
// ===============================

// Initialize cache - call ONCE after RTC init
void initTimeCache();

// Get cached date (O(1), no I2C)
const char* getCachedDate();

// Get cached unix timestamp (O(1), no I2C)
uint32_t getCachedUnixTime();

// Get approximate current unix time (no I2C, estimated)
uint32_t getApproxUnixTime();

// Check if cache needs refresh (every 1 hour)
bool shouldRefreshCache();

// Force refresh cache from RTC (I2C read)
void updateTimeCache();

// Manual trigger for date change callback
void triggerDateChangeCallback(const char* oldDate, const char* newDate);

#endif