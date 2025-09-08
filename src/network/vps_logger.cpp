// #include "vps_logger.h"
// #include "../config/config.h"
// #include "../config/credentials_manager.h" 
// #include "../hardware/water_sensors.h"
// #include "../network/wifi_manager.h"
// #include "../core/logging.h"
// #include "../hardware/rtc_controller.h"
// #include "../hardware/fram_controller.h" 
// #include <HTTPClient.h>
// #include <ArduinoJson.h>
// #include "../algorithm/algorithm_config.h"




// void initVPSLogger() {
//     LOG_INFO("VPS Logger initialized - endpoint: %s", VPS_URL);
// }

// bool logEventToVPS(const String& eventType, uint16_t volumeML, const String& timestamp) {

//     // Skip logging if pump globally disabled
//     if (!pumpGlobalEnabled) {
//         return false;
//     }

//     if (!isWiFiConnected()) {
//         LOG_WARNING("WiFi not connected, cannot log to VPS");
//         return false;
//     }
    
//     HTTPClient http;
//     http.begin(VPS_URL);
//     http.addHeader("Content-Type", "application/json");
//     http.addHeader("Authorization", "Bearer " + String(VPS_AUTH_TOKEN));
//     http.setTimeout(10000);
    
//     JsonDocument payload;
//     payload["device_id"] = DEVICE_ID;
//     payload["timestamp"] = timestamp;
//     payload["unix_time"] = getUnixTimestamp();
//     payload["event_type"] = eventType;
//     payload["volume_ml"] = volumeML;
//     payload["water_status"] = getWaterStatus();
//     payload["system_status"] = "OK";
    
//     String jsonString;
//     serializeJson(payload, jsonString);
    
//     LOG_INFO("Sending to VPS: %s", jsonString.c_str());
    
//     int httpCode = http.POST(jsonString);
    
//     if (httpCode == 200) {
//         LOG_INFO("VPS log successful: %s", eventType.c_str());
//         http.end();
//         return true;
//     } else {
//         LOG_ERROR("VPS log failed: HTTP %d", httpCode);
//         http.end();
//         return false;
//     }
// }

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
//     http.begin(VPS_URL);
//     http.addHeader("Content-Type", "application/json");
//     http.addHeader("Authorization", "Bearer " + String(VPS_AUTH_TOKEN));
//     http.setTimeout(10000);
    
//     // *** NOWE: Pobierz aktualne sumy błędów z FRAM ***
//     ErrorStats currentStats;
//     bool statsLoaded = loadErrorStatsFromFRAM(currentStats);
    
//     if (!statsLoaded) {
//         LOG_WARNING("Failed to load error stats from FRAM, using zeros");
//         currentStats.gap1_fail_sum = 0;
//         currentStats.gap2_fail_sum = 0;
//         currentStats.water_fail_sum = 0;
//         currentStats.last_reset_timestamp = millis() / 1000;
//     }
    
//     JsonDocument payload;
//     payload["device_id"] = DEVICE_ID;
//     payload["timestamp"] = timestamp;
//     payload["unix_time"] = getUnixTimestamp();
//     payload["event_type"] = "AUTO_CYCLE_COMPLETE";
//     payload["volume_ml"] = cycle.volume_dose;
//     payload["water_status"] = getWaterStatus();
//     payload["system_status"] = (cycle.error_code == 0) ? "OK" : "ERROR";
    
//     // *** ZACHOWANE: Szczegółowe dane algorytmiczne pojedynczego cyklu ***
//     payload["time_gap_1"] = cycle.time_gap_1;
//     payload["time_gap_2"] = cycle.time_gap_2;
//     payload["water_trigger_time"] = cycle.water_trigger_time;
//     payload["pump_duration"] = cycle.pump_duration;
//     payload["pump_attempts"] = cycle.pump_attempts;
    
//     // *** ZMIENIONE: Sumy błędów z ostatnich okresów zamiast pojedynczych 0/1 ***
//     payload["gap1_fail_sum"] = currentStats.gap1_fail_sum;
//     payload["gap2_fail_sum"] = currentStats.gap2_fail_sum;
//     payload["water_fail_sum"] = currentStats.water_fail_sum;
    
//     // Dodatkowe dane dla kompatybilności i debugowania
//     payload["last_reset_timestamp"] = currentStats.last_reset_timestamp;
    
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
//     LOG_INFO("JSON: %s", jsonString.c_str());
    
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


// #include "vps_logger.h"
// #include "../config/config.h"
// #include "../config/credentials_manager.h"  // <-- DODAJ TĘ LINIĘ
// #include "../hardware/water_sensors.h"
// #include "../network/wifi_manager.h"
// #include "../core/logging.h"
// #include "../hardware/rtc_controller.h"
// #include "../hardware/fram_controller.h" 
// #include <HTTPClient.h>
// #include <ArduinoJson.h>
// #include "../algorithm/algorithm_config.h"

// void initVPSLogger() {
//     // Use dynamic VPS URL - we'll keep this hardcoded for now
//     // but use dynamic auth token
//     LOG_INFO("VPS Logger initialized - endpoint: %s", VPS_URL);
//     LOG_INFO("Using %s auth token", areCredentialsLoaded() ? "FRAM" : "fallback");
// }

// bool logEventToVPS(const String& eventType, uint16_t volumeML, const String& timestamp) {
//     // Skip logging if pump globally disabled
//     if (!pumpGlobalEnabled) {
//         return false;
//     }

//     if (!isWiFiConnected()) {
//         LOG_WARNING("WiFi not connected, cannot log to VPS");
//         return false;
//     }
    
//     HTTPClient http;
//     http.begin(VPS_URL);
//     http.addHeader("Content-Type", "application/json");
    
//     // Use dynamic auth token
//     String authHeader = "Bearer " + String(getVPSAuthToken());
//     http.addHeader("Authorization", authHeader);
//     http.setTimeout(10000);
    
//     JsonDocument payload;
//     payload["device_id"] = getDeviceID();  // <-- DYNAMIC DEVICE ID
//     payload["timestamp"] = timestamp;
//     payload["unix_time"] = getUnixTimestamp();
//     payload["event_type"] = eventType;
//     payload["volume_ml"] = volumeML;
//     payload["water_status"] = getWaterStatus();
//     payload["system_status"] = "OK";
    
//     String jsonString;
//     serializeJson(payload, jsonString);
    
//     LOG_INFO("Sending to VPS: %s", jsonString.c_str());
    
//     int httpCode = http.POST(jsonString);
    
//     if (httpCode == 200) {
//         LOG_INFO("VPS log successful: %s", eventType.c_str());
//         http.end();
//         return true;
//     } else {
//         LOG_ERROR("VPS log failed: HTTP %d", httpCode);
//         http.end();
//         return false;
//     }
// }

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
//     http.begin(VPS_URL);
//     http.addHeader("Content-Type", "application/json");
    
//     // Use dynamic auth token
//     String authHeader = "Bearer " + String(getVPSAuthToken());
//     http.addHeader("Authorization", authHeader);
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
    
//     JsonDocument payload;
//     payload["device_id"] = getDeviceID();  // <-- DYNAMIC DEVICE ID
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

#if MODE_PRODUCTION
    #include "../config/credentials_manager.h"
#endif

void initVPSLogger() {
    LOG_INFO("VPS Logger initialized - endpoint: %s", VPS_URL);
#if MODE_PRODUCTION
    LOG_INFO("Using %s auth token", areCredentialsLoaded() ? "FRAM" : "fallback");
#else
    LOG_INFO("Using hardcoded auth token (programming mode)");
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
    
    HTTPClient http;
    http.begin(VPS_URL);
    http.addHeader("Content-Type", "application/json");
    
#if MODE_PRODUCTION
    // Production mode: use dynamic auth token and device ID
    String authHeader = "Bearer " + String(getVPSAuthToken());
    http.addHeader("Authorization", authHeader);
    
    JsonDocument payload;
    payload["device_id"] = getDeviceID();
#else
    // Programming mode: use hardcoded credentials
    String authHeader = "Bearer " + String(VPS_AUTH_TOKEN);
    http.addHeader("Authorization", authHeader);
    
    JsonDocument payload;
    payload["device_id"] = DEVICE_ID;
#endif
    
    http.setTimeout(10000);
    
    payload["timestamp"] = timestamp;
    payload["unix_time"] = getUnixTimestamp();
    payload["event_type"] = eventType;
    payload["volume_ml"] = volumeML;
    payload["water_status"] = getWaterStatus();
    payload["system_status"] = "OK";
    
    String jsonString;
    serializeJson(payload, jsonString);
    
    LOG_INFO("Sending to VPS: %s", jsonString.c_str());
    
    int httpCode = http.POST(jsonString);
    
    if (httpCode == 200) {
        LOG_INFO("VPS log successful: %s", eventType.c_str());
        http.end();
        return true;
    } else {
        LOG_ERROR("VPS log failed: HTTP %d", httpCode);
        http.end();
        return false;
    }
}

bool logCycleToVPS(const PumpCycle& cycle, const String& timestamp) {
    // Skip logging if pump globally disabled
    if (!pumpGlobalEnabled) {
        return false;
    }

    if (!isWiFiConnected()) {
        LOG_WARNING("WiFi not connected, cannot log cycle to VPS");
        return false;
    }
    
    HTTPClient http;
    http.begin(VPS_URL);
    http.addHeader("Content-Type", "application/json");
    
#if MODE_PRODUCTION
    // Production mode: use dynamic auth token and device ID
    String authHeader = "Bearer " + String(getVPSAuthToken());
    http.addHeader("Authorization", authHeader);
    
    JsonDocument payload;
    payload["device_id"] = getDeviceID();
#else
    // Programming mode: use hardcoded credentials
    String authHeader = "Bearer " + String(VPS_AUTH_TOKEN);
    http.addHeader("Authorization", authHeader);
    
    JsonDocument payload;
    payload["device_id"] = DEVICE_ID;
#endif
    
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
    
    payload["timestamp"] = timestamp;
    payload["unix_time"] = getUnixTimestamp();
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
    LOG_INFO("Sending cycle to VPS with sums: GAP1=%d, GAP2=%d, WATER=%d", 
             currentStats.gap1_fail_sum, currentStats.gap2_fail_sum, currentStats.water_fail_sum);
    
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