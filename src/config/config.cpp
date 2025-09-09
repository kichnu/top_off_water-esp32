// #include "config.h"
// // #include <Preferences.h>
// #include "../core/logging.h"
// #include "../hardware/fram_controller.h"


// const char* WIFI_SSID = "KiG";
// const char* WIFI_PASSWORD = "3YqvXx5s3Z";
// const IPAddress STATIC_IP(192, 168, 0, 164);
// const IPAddress GATEWAY(192, 168, 0, 1);
// const IPAddress SUBNET(255, 255, 255, 0);

// bool pumpGlobalEnabled = true;  // Default ON
// unsigned long pumpDisabledTime = 0;
// const unsigned long PUMP_AUTO_ENABLE_MS = 3 * 60 * 1000; // 30 minutes

// const char* ADMIN_PASSWORD_HASH = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";


// const IPAddress ALLOWED_IPS[] = {
//     IPAddress(192, 168, 0, 124),
//     IPAddress(192, 168, 1, 102),
//     IPAddress(192, 168, 1, 103),
//     IPAddress(192, 168, 1, 1)
// };
// const int ALLOWED_IPS_COUNT = sizeof(ALLOWED_IPS) / sizeof(ALLOWED_IPS[0]);

// const char* VPS_URL = "http://146.59.92.15:5000/api/water-events";

// // const char* VPS_AUTH_TOKEN = "212d4a3d708f907c2c782937f72daf28aa66d8e342d4ede15381b6d8295344d6";


// const char* DEVICE_ID = "DOLEWKA";


// PumpSettings currentPumpSettings;

// // ================= FRAM Storage Functions =================

// void initNVS() {
//     // Nazwa pozostaje dla kompatybilności, ale używamy FRAM
//     LOG_INFO("Initializing storage (FRAM)...");
//     if (initFRAM()) {
//         LOG_INFO("Storage system ready (FRAM)");
//     } else {
//         LOG_ERROR("Storage initialization failed!");
//     }
// }

// void loadVolumeFromNVS() {
//     float savedVolume = currentPumpSettings.volumePerSecond; // default
    
//     if (loadVolumeFromFRAM(savedVolume)) {
//         currentPumpSettings.volumePerSecond = savedVolume;
//     } else {
//         LOG_INFO("Using default volumePerSecond: %.1f ml/s", 
//                  currentPumpSettings.volumePerSecond);
//     }
// }

// void saveVolumeToNVS() {
//     if (!saveVolumeToFRAM(currentPumpSettings.volumePerSecond)) {
//         LOG_ERROR("Failed to save volume to storage");
//     }
// }

// // ================= Global Pump Control =================


// void checkPumpAutoEnable() {
//     if (!pumpGlobalEnabled && pumpDisabledTime > 0) {
//         if (millis() - pumpDisabledTime >= PUMP_AUTO_ENABLE_MS) {
//             pumpGlobalEnabled = true;
//             pumpDisabledTime = 0;
//             LOG_INFO("Pump auto-enabled after 30 minutes");
//         }
//     }
// }

// void setPumpGlobalState(bool enabled) {
//     pumpGlobalEnabled = enabled;
//     if (!enabled) {
//         pumpDisabledTime = millis();
//         LOG_INFO("Pump globally disabled for 30 minutes");
//     } else {
//         pumpDisabledTime = 0;
//         LOG_INFO("Pump globally enabled");
//     }
// }



#include "config.h"
#include "../core/logging.h"
#include "../hardware/fram_controller.h"

// ===============================
// 🔒 SECURE PLACEHOLDER VALUES 
// ===============================
// These are FALLBACK values used when FRAM credentials are not available
// NEVER put real credentials here - use Programming Mode to set them in FRAM

const char* WIFI_SSID = "SETUP_REQUIRED";
const char* WIFI_PASSWORD = "SETUP_REQUIRED";
const IPAddress STATIC_IP(192, 168, 0, 164);
const IPAddress GATEWAY(192, 168, 0, 1);
const IPAddress SUBNET(255, 255, 255, 0);

// Dummy admin password hash (for "password")
// const char* ADMIN_PASSWORD_HASH = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";
// const char* ADMIN_PASSWORD_HASH = "Dummy admin password";
const char* ADMIN_PASSWORD_HASH = nullptr;  // ✅ Force FRAM setup!


const IPAddress ALLOWED_IPS[] = {
    IPAddress(192, 168, 0, 124),
    IPAddress(192, 168, 1, 102),
    IPAddress(192, 168, 1, 103),
    IPAddress(192, 168, 1, 1)
};
const int ALLOWED_IPS_COUNT = sizeof(ALLOWED_IPS) / sizeof(ALLOWED_IPS[0]);

// 🔒 SECURE PLACEHOLDER VALUES - NO REAL CREDENTIALS!
const char* VPS_URL = "http://localhost:5000/api/setup-required";     // what?????
const char* VPS_AUTH_TOKEN = "SETUP_REQUIRED_USE_FRAM_PROGRAMMER";
const char* DEVICE_ID = "UNCONFIGURED_DEVICE";

// Global pump control
bool pumpGlobalEnabled = true;  // Default ON
unsigned long pumpDisabledTime = 0;
const unsigned long PUMP_AUTO_ENABLE_MS = 3 * 60 * 1000; // 30 minutes

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