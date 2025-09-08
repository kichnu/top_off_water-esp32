#include "config.h"
// #include <Preferences.h>
#include "../core/logging.h"
#include "../hardware/fram_controller.h"


const char* WIFI_SSID = "KiG";
const char* WIFI_PASSWORD = "3YqvXx5s3Z";
const IPAddress STATIC_IP(192, 168, 0, 164);
const IPAddress GATEWAY(192, 168, 0, 1);
const IPAddress SUBNET(255, 255, 255, 0);

bool pumpGlobalEnabled = true;  // Default ON
unsigned long pumpDisabledTime = 0;
const unsigned long PUMP_AUTO_ENABLE_MS = 3 * 60 * 1000; // 30 minutes

// const char* ADMIN_PASSWORD_HASH = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";
const char* ADMIN_PASSWORD_HASH = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";


const IPAddress ALLOWED_IPS[] = {
    IPAddress(192, 168, 0, 124),
    IPAddress(192, 168, 1, 102),
    IPAddress(192, 168, 1, 103),
    IPAddress(192, 168, 1, 1)
};
const int ALLOWED_IPS_COUNT = sizeof(ALLOWED_IPS) / sizeof(ALLOWED_IPS[0]);

const char* VPS_URL = "http://146.59.92.15:5000/api/water-events";
// const char* VPS_URL = "https://app.krzysztoforlinski.pl/api/water-events";

// const char* VPS_AUTH_TOKEN = "sha256:7b4f8a9c2e6d5a1b8f7e4c9a6d3b2f8e5c1a7b4f9e6d3c8a5b2f7e4c9a6d1b8f";;
const char* VPS_AUTH_TOKEN = "212d4a3d708f907c2c782937f72daf28aa66d8e342d4ede15381b6d8295344d6";;

// const char* VPS_AUTH_TOKEN = "WaterSystem2024_StaticToken_ESP32C3";

// const char* DEVICE_ID = "ESP32C3_WaterPump_001";
const char* DEVICE_ID = "DOLEWKA";


PumpSettings currentPumpSettings;

// ================= FRAM Storage Functions =================

void initNVS() {
    // Nazwa pozostaje dla kompatybilności, ale używamy FRAM
    LOG_INFO("Initializing storage (FRAM)...");
    if (initFRAM()) {
        LOG_INFO("Storage system ready (FRAM)");
    } else {
        LOG_ERROR("Storage initialization failed!");
    }
}

void loadVolumeFromNVS() {
    float savedVolume = currentPumpSettings.volumePerSecond; // default
    
    if (loadVolumeFromFRAM(savedVolume)) {
        currentPumpSettings.volumePerSecond = savedVolume;
    } else {
        LOG_INFO("Using default volumePerSecond: %.1f ml/s", 
                 currentPumpSettings.volumePerSecond);
    }
}

void saveVolumeToNVS() {
    if (!saveVolumeToFRAM(currentPumpSettings.volumePerSecond)) {
        LOG_ERROR("Failed to save volume to storage");
    }
}



// ================= NVS (Preferences) Functions =================
// Preferences preferences;
// bool preferencesInitialized = false;

// void initNVS() {
//     // Tylko informacja - nie inicjalizujemy tutaj
//     LOG_INFO("NVS (Preferences) ready");
// }

// void loadVolumeFromNVS() {
//     if (!preferences.begin("pump_settings", true)) { // true = readonly
//         LOG_ERROR("Failed to open preferences for reading");
//         LOG_INFO("Using default volumePerSecond: %.1f", currentPumpSettings.volumePerSecond);
//         return;
//     }
    
//     float savedVolume = preferences.getFloat("vol_per_sec", 0.0);
//     preferences.end(); // WAŻNE: zamknij po odczycie
    
//     if (savedVolume > 0.0 && savedVolume >= 0.1 && savedVolume <= 20.0) {
//         currentPumpSettings.volumePerSecond = savedVolume;
//         LOG_INFO("Loaded volumePerSecond from NVS: %.1f ml/s", savedVolume);
//     } else {
//         LOG_INFO("No valid volumePerSecond in NVS, using default: %.1f ml/s", currentPumpSettings.volumePerSecond);
//     }
// }

// void saveVolumeToNVS() {
//     // Delay żeby flash miał czas
//     delay(10);
    
//     if (!preferences.begin("pump_settings", false)) {
//         LOG_ERROR("Failed to open preferences for writing - retrying...");
//         delay(50);
//         if (!preferences.begin("pump_settings", false)) {
//             LOG_ERROR("Retry failed - skipping save");
//             return;
//         }
//     }
    
//     bool success = preferences.putFloat("vol_per_sec", currentPumpSettings.volumePerSecond);
//     preferences.end();
    
//     if (success) {
//         LOG_INFO("SUCCESS: Saved volumePerSecond to NVS: %.1f ml/s", currentPumpSettings.volumePerSecond);
//     } else {
//         LOG_ERROR("FAILED: Could not save volumePerSecond to NVS");
//     }
    
//     // Verify save by reading back
//     delay(10);
//     if (preferences.begin("pump_settings", true)) {
//         float readBack = preferences.getFloat("vol_per_sec", 0.0);
//         preferences.end();
//         LOG_INFO("Verification read: %.1f ml/s", readBack);
//     }
// }

// ================= Global Pump Control =================


void checkPumpAutoEnable() {
    if (!pumpGlobalEnabled && pumpDisabledTime > 0) {
        if (millis() - pumpDisabledTime >= PUMP_AUTO_ENABLE_MS) {
            pumpGlobalEnabled = true;
            pumpDisabledTime = 0;
            LOG_INFO("Pump auto-enabled after 30 minutes");
        }
    }
}

void setPumpGlobalState(bool enabled) {
    pumpGlobalEnabled = enabled;
    if (!enabled) {
        pumpDisabledTime = millis();
        LOG_INFO("Pump globally disabled for 30 minutes");
    } else {
        pumpDisabledTime = 0;
        LOG_INFO("Pump globally enabled");
    }
}