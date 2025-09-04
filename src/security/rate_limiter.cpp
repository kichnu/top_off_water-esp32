#include "rate_limiter.h"
#include "../config/config.h"
#include "../security/auth_manager.h"
#include "../core/logging.h"
#include <map>
#include <vector>



struct RateLimitData {
    std::vector<unsigned long> requestTimes;
    int failedAttempts;
    unsigned long blockUntil;
    unsigned long lastRequest;
};

std::map<String, RateLimitData> rateLimitData;

void initRateLimiter() {
    rateLimitData.clear();
    LOG_INFO("Rate limiter initialized");
}

void updateRateLimiter() {
    unsigned long now = millis();
    
    // Clean old data every 5 minutes
    static unsigned long lastCleanup = 0;
    if (now - lastCleanup > 300000) {
        for (auto it = rateLimitData.begin(); it != rateLimitData.end();) {
            if (now - it->second.lastRequest > 300000) {
                it = rateLimitData.erase(it);
            } else {
                ++it;
            }
        }
        lastCleanup = now;
    }
}

bool isRateLimited(IPAddress ip) {
    if (isIPAllowed(ip)) {
        return false; // No rate limiting for whitelisted IPs
    }
    
    String ipStr = ip.toString();
    if (rateLimitData.find(ipStr) == rateLimitData.end()) {
        return false;
    }
    
    RateLimitData& data = rateLimitData[ipStr];
    unsigned long now = millis();
    
    // Check if blocked
    if (data.blockUntil > now) {
        return true;
    }
    
    // Clean old requests
    data.requestTimes.erase(
        std::remove_if(data.requestTimes.begin(), data.requestTimes.end(),
                      [now](unsigned long time) { return now - time > RATE_LIMIT_WINDOW_MS; }),
        data.requestTimes.end());
    
    return data.requestTimes.size() >= MAX_REQUESTS_PER_SECOND;
}

void recordRequest(IPAddress ip) {
    String ipStr = ip.toString();
    unsigned long now = millis();
    
    RateLimitData& data = rateLimitData[ipStr];
    data.requestTimes.push_back(now);
    data.lastRequest = now;
}

void recordFailedAttempt(IPAddress ip) {
    if (isIPAllowed(ip)) {
        return; // No blocking for whitelisted IPs
    }
    
    String ipStr = ip.toString();
    unsigned long now = millis();
    
    RateLimitData& data = rateLimitData[ipStr];
    data.failedAttempts++;
    
    if (data.failedAttempts >= MAX_FAILED_ATTEMPTS) {
        data.blockUntil = now + BLOCK_DURATION_MS;
        data.failedAttempts = 0;
        LOG_WARNING("IP %s blocked for failed attempts", ipStr.c_str());
    }
}

bool isIPBlocked(IPAddress ip) {
    if (isIPAllowed(ip)) {
        return false;
    }
    
    String ipStr = ip.toString();
    if (rateLimitData.find(ipStr) == rateLimitData.end()) {
        return false;
    }
    
    return rateLimitData[ipStr].blockUntil > millis();
}