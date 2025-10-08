

#include "vps_logger.h"
#include "../config/config.h"
#include "../hardware/water_sensors.h"
#include "../network/wifi_manager.h"
#include "../core/logging.h"
#include "../hardware/rtc_controller.h"
#include "../hardware/fram_controller.h" 
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../algorithm/algorithm_config.h"
#include "../algorithm/water_algorithm.h"
#include "../hardware/time_cache.h"

#if MODE_PRODUCTION
    #include "../config/credentials_manager.h"
#endif

extern WaterAlgorithm waterAlgorithm;

void initVPSLogger() {
#if MODE_PRODUCTION
    LOG_INFO("VPS Logger initialized - endpoint: %s", getVPSURL());  // 🆕 Dynamic URL
    LOG_INFO("Using %s credentials", areCredentialsLoaded() ? "FRAM" : "fallback");
#else
    LOG_INFO("VPS Logger initialized - endpoint: %s", VPS_URL);
    LOG_INFO("Using hardcoded credentials (programming mode)");
#endif
}

bool logEventToVPS(const String& eventType, uint16_t volumeML, const String& timestamp) {
    // Skip logging if pump globally disabled
    if (!pumpGlobalEnabled) {
        return false;
    }

    if (!isWiFiConnected()) {
        LOG_WARNING("WiFi not connected, cannot log to VPS");
        return false;
    }
    
    // ✅ DYNAMIC TIMEOUT based on event criticality
    uint16_t timeoutMs;
    bool isCritical;
    
    if (eventType == "STATISTICS_RESET") {
        timeoutMs = 3000;    // 3 seconds for non-critical reset
        isCritical = false;
    } else if (eventType == "AUTO_CYCLE_COMPLETE") {
        timeoutMs = 8000;    // 8 seconds for important cycle data
        isCritical = true;
    } else {
        timeoutMs = 5000;    // 5 seconds for other events
        isCritical = true;
    }
    
    HTTPClient http;
    
#if MODE_PRODUCTION
    // Production mode: use dynamic VPS URL and credentials
    const char* vpsUrl = getVPSURL();
    http.begin(vpsUrl);
    
    String authHeader = "Bearer " + String(getVPSAuthToken());
    http.addHeader("Authorization", authHeader);
    
    JsonDocument payload;
    payload["device_id"] = getDeviceID();
    
    LOG_INFO("Using dynamic VPS URL: %s (timeout: %dms)", vpsUrl, timeoutMs);
#else
    // Programming mode: use hardcoded credentials
    http.begin(VPS_URL);
    
    String authHeader = "Bearer " + String(VPS_AUTH_TOKEN);
    http.addHeader("Authorization", authHeader);
    
    JsonDocument payload;
    payload["device_id"] = DEVICE_ID;
#endif
    
    http.addHeader("Content-Type", "application/json");
    
    // ✅ SET DYNAMIC TIMEOUT
    http.setTimeout(timeoutMs);
    
    payload["timestamp"] = timestamp;
    payload["unix_time"] = getUnixTimestamp();
    payload["event_type"] = eventType;
    payload["volume_ml"] = volumeML;
    payload["water_status"] = getWaterStatus();
    payload["system_status"] = "OK";

    // 🆕 NEW: Add daily_volume_ml for MANUAL_NORMAL events
    if (eventType == "MANUAL_NORMAL") {
        payload["daily_volume_ml"] = waterAlgorithm.getDailyVolume();
        LOG_INFO("Including daily_volume_ml in VPS payload: %dml", 
                 waterAlgorithm.getDailyVolume());
    }
    
    String jsonString;
    serializeJson(payload, jsonString);
    
    LOG_INFO("Sending to VPS (%s priority): %s", 
             isCritical ? "HIGH" : "LOW", 
             jsonString.c_str());
    
    unsigned long startTime = millis();
    int httpCode = http.POST(jsonString);
    unsigned long duration = millis() - startTime;
    
    if (httpCode == 200) {
        LOG_INFO("✅ VPS log successful: %s (%lums)", eventType.c_str(), duration);
        http.end();
        return true;
    } else {
        // ✅ DIFFERENT LOG LEVELS based on criticality
        if (isCritical) {
            LOG_ERROR("❌ VPS log failed: %s HTTP %d (%lums)", eventType.c_str(), httpCode, duration);
        } else {
            LOG_WARNING("⚠️ VPS log failed (non-critical): %s HTTP %d (%lums)", eventType.c_str(), httpCode, duration);
        }
        
        http.end();
        return false;
    }
}

bool logCycleToVPS(const PumpCycle& cycle, uint32_t unixTime) {
    if (!pumpGlobalEnabled) {
        return false;
    }

    if (!isWiFiConnected()) {
        LOG_WARNING("WiFi not connected, cannot log cycle to VPS");
        return false;
    }
    
    HTTPClient http;
    
#if MODE_PRODUCTION
    const char* vpsUrl = getVPSURL();
    http.begin(vpsUrl);
    
    String authHeader = "Bearer " + String(getVPSAuthToken());
    http.addHeader("Authorization", authHeader);
    
    JsonDocument payload;
    payload["device_id"] = getDeviceID();
#else
    http.begin(VPS_URL);
    
    String authHeader = "Bearer " + String(VPS_AUTH_TOKEN);
    http.addHeader("Authorization", authHeader);
    
    JsonDocument payload;
    payload["device_id"] = DEVICE_ID;
#endif
    
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);
    
    // Load current error stats from FRAM
    ErrorStats currentStats;
    bool statsLoaded = loadErrorStatsFromFRAM(currentStats);
    
    if (!statsLoaded) {
        LOG_WARNING("Failed to load error stats from FRAM, using zeros");
        currentStats.gap1_fail_sum = 0;
        currentStats.gap2_fail_sum = 0;
        currentStats.water_fail_sum = 0;
        currentStats.last_reset_timestamp = millis() / 1000;
    }
    
    // 🆕 P1: FIXED - konwertuj unix timestamp na string LOKALNIE
    time_t t = (time_t)unixTime;
    struct tm timeinfo;
    localtime_r(&t, &timeinfo);
    
    char timestampStr[32];
    strftime(timestampStr, sizeof(timestampStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    // ✅ Używamy PRZEKAZANEGO timestamp zamiast czytać ponownie
    payload["timestamp"] = String(timestampStr);
    payload["unix_time"] = unixTime;  // ← JEDEN odczyt, zero I2C!
    
    payload["event_type"] = "AUTO_CYCLE_COMPLETE";
    payload["volume_ml"] = cycle.volume_dose;
    payload["water_status"] = getWaterStatus();
    payload["system_status"] = (cycle.error_code == 0) ? "OK" : "ERROR";
    
    // Algorithm data
    payload["time_gap_1"] = cycle.time_gap_1;
    payload["time_gap_2"] = cycle.time_gap_2;
    payload["water_trigger_time"] = cycle.water_trigger_time;
    payload["pump_duration"] = cycle.pump_duration;
    payload["pump_attempts"] = cycle.pump_attempts;
    
    // Error sums from FRAM
    payload["gap1_fail_sum"] = currentStats.gap1_fail_sum;
    payload["gap2_fail_sum"] = currentStats.gap2_fail_sum;
    payload["water_fail_sum"] = currentStats.water_fail_sum;
    payload["last_reset_timestamp"] = currentStats.last_reset_timestamp;

    payload["daily_volume_ml"] = waterAlgorithm.getDailyVolume();
    
#if MODE_PRODUCTION
    payload["vps_endpoint"] = getVPSURL();
    payload["credentials_source"] = areCredentialsLoaded() ? "FRAM" : "FALLBACK";
#else
    payload["vps_endpoint"] = VPS_URL;
    payload["credentials_source"] = "HARDCODED";
#endif
    
    // Algorithm summary
    String algorithmSummary = "";
    algorithmSummary += "THRESHOLDS(GAP1:" + String(THRESHOLD_1) + "s,";
    algorithmSummary += "GAP2:" + String(THRESHOLD_2) + "s,";
    algorithmSummary += "WATER:" + String(THRESHOLD_WATER) + "s) ";
    algorithmSummary += "CURRENT(" + String((cycle.sensor_results & PumpCycle::RESULT_GAP1_FAIL) ? 1 : 0) + "-";
    algorithmSummary += String((cycle.sensor_results & PumpCycle::RESULT_GAP2_FAIL) ? 1 : 0) + "-";
    algorithmSummary += String((cycle.sensor_results & PumpCycle::RESULT_WATER_FAIL) ? 1 : 0) + ") ";
    algorithmSummary += "SUMS(" + String(currentStats.gap1_fail_sum) + "-";
    algorithmSummary += String(currentStats.gap2_fail_sum) + "-";
    algorithmSummary += String(currentStats.water_fail_sum) + ")";
    payload["algorithm_data"] = algorithmSummary;
    
    String jsonString;
    serializeJson(payload, jsonString);
    
    LOG_INFO("JSON size: %d bytes", jsonString.length());
    LOG_INFO("Sending cycle to VPS (unix time: %lu)", (unsigned long)unixTime);
    
    int httpCode = http.POST(jsonString);
    
    if (httpCode == 200) {
        LOG_INFO("VPS cycle log successful");
        http.end();
        return true;
    } else {
        LOG_ERROR("VPS cycle log failed: HTTP %d", httpCode);
        http.end();
        return false;
    }
}

// bool logCycleToVPS(const PumpCycle& cycle, const String& timestamp) {
//     // Skip logging if pump globally disabled
//     if (!pumpGlobalEnabled) {
//         return false;
//     }

//     if (!isWiFiConnected()) {
//         LOG_WARNING("WiFi not connected, cannot log cycle to VPS");
//         return false;
//     }
    
//     HTTPClient http;
    
// #if MODE_PRODUCTION
//     // Production mode: use dynamic VPS URL and credentials
//     const char* vpsUrl = getVPSURL();
//     http.begin(vpsUrl);
    
//     String authHeader = "Bearer " + String(getVPSAuthToken());
//     http.addHeader("Authorization", authHeader);
    
//     JsonDocument payload;
//     payload["device_id"] = getDeviceID();
// #else
//     // Programming mode: use hardcoded credentials
//     http.begin(VPS_URL);
    
//     String authHeader = "Bearer " + String(VPS_AUTH_TOKEN);
//     http.addHeader("Authorization", authHeader);
    
//     JsonDocument payload;
//     payload["device_id"] = DEVICE_ID;
// #endif
    
//     http.addHeader("Content-Type", "application/json");
//     http.setTimeout(10000);
    
//     // Load current error stats from FRAM
//     ErrorStats currentStats;
//     bool statsLoaded = loadErrorStatsFromFRAM(currentStats);
    
//     if (!statsLoaded) {
//         LOG_WARNING("Failed to load error stats from FRAM, using zeros");
//         currentStats.gap1_fail_sum = 0;
//         currentStats.gap2_fail_sum = 0;
//         currentStats.water_fail_sum = 0;
//         currentStats.last_reset_timestamp = millis() / 1000;
//     }
    
//     payload["timestamp"] = timestamp;
//     payload["unix_time"] = getUnixTimestamp();
//     payload["event_type"] = "AUTO_CYCLE_COMPLETE";
//     payload["volume_ml"] = cycle.volume_dose;
//     payload["water_status"] = getWaterStatus();
//     payload["system_status"] = (cycle.error_code == 0) ? "OK" : "ERROR";
    
//     // Algorithm data
//     payload["time_gap_1"] = cycle.time_gap_1;
//     payload["time_gap_2"] = cycle.time_gap_2;
//     payload["water_trigger_time"] = cycle.water_trigger_time;
//     payload["pump_duration"] = cycle.pump_duration;
//     payload["pump_attempts"] = cycle.pump_attempts;
    
//     // Error sums from FRAM
//     payload["gap1_fail_sum"] = currentStats.gap1_fail_sum;
//     payload["gap2_fail_sum"] = currentStats.gap2_fail_sum;
//     payload["water_fail_sum"] = currentStats.water_fail_sum;
//     payload["last_reset_timestamp"] = currentStats.last_reset_timestamp;

//     payload["daily_volume_ml"] = waterAlgorithm.getDailyVolume();
    
//     // 🆕 NEW: Add VPS URL info for debugging
// #if MODE_PRODUCTION
//     payload["vps_endpoint"] = getVPSURL();
//     payload["credentials_source"] = areCredentialsLoaded() ? "FRAM" : "FALLBACK";
// #else
//     payload["vps_endpoint"] = VPS_URL;
//     payload["credentials_source"] = "HARDCODED";
// #endif
    
//     // Algorithm summary
//     String algorithmSummary = "";
//     algorithmSummary += "THRESHOLDS(GAP1:" + String(THRESHOLD_1) + "s,";
//     algorithmSummary += "GAP2:" + String(THRESHOLD_2) + "s,";
//     algorithmSummary += "WATER:" + String(THRESHOLD_WATER) + "s) ";
//     algorithmSummary += "CURRENT(" + String((cycle.sensor_results & PumpCycle::RESULT_GAP1_FAIL) ? 1 : 0) + "-";
//     algorithmSummary += String((cycle.sensor_results & PumpCycle::RESULT_GAP2_FAIL) ? 1 : 0) + "-";
//     algorithmSummary += String((cycle.sensor_results & PumpCycle::RESULT_WATER_FAIL) ? 1 : 0) + ") ";
//     algorithmSummary += "SUMS(" + String(currentStats.gap1_fail_sum) + "-";
//     algorithmSummary += String(currentStats.gap2_fail_sum) + "-";
//     algorithmSummary += String(currentStats.water_fail_sum) + ")";
//     payload["algorithm_data"] = algorithmSummary;
    
//     String jsonString;
//     serializeJson(payload, jsonString);
    
//     LOG_INFO("JSON size: %d bytes", jsonString.length());
//     LOG_INFO("Sending cycle to VPS with sums: GAP1=%d, GAP2=%d, WATER=%d", 
//              currentStats.gap1_fail_sum, currentStats.gap2_fail_sum, currentStats.water_fail_sum);
    
//     int httpCode = http.POST(jsonString);
    
//     if (httpCode == 200) {
//         LOG_INFO("VPS cycle log successful");
//         http.end();
//         return true;
//     } else {
//         LOG_ERROR("VPS cycle log failed: HTTP %d", httpCode);
//         http.end();
//         return false;
//     }
// }