
#include "config.h"
#include "../core/logging.h"
#include "../hardware/fram_controller.h"

// ===============================
// 🔒 SECURE PLACEHOLDER VALUES 
// ===============================
const char* WIFI_SSID = "SETUP_REQUIRED";
const char* WIFI_PASSWORD = "SETUP_REQUIRED";
const IPAddress STATIC_IP(192, 168, 0, 164);
const IPAddress GATEWAY(192, 168, 0, 1);
const IPAddress SUBNET(255, 255, 255, 0);

const char* ADMIN_PASSWORD_HASH = nullptr;  // ✅ Force FRAM setup!

const IPAddress ALLOWED_IPS[] = {
    IPAddress(192, 168, 0, 124),
    IPAddress(192, 168, 1, 102),
    IPAddress(192, 168, 1, 103),
    IPAddress(192, 168, 1, 1)
};
const int ALLOWED_IPS_COUNT = sizeof(ALLOWED_IPS) / sizeof(ALLOWED_IPS[0]);

// 🔒 SECURE PLACEHOLDER VALUES - NO REAL CREDENTIALS!
// const char* VPS_URL = "http://localhost:5000/api/setup-required"; 
const char* VPS_URL = "SETUP_REQUIRED_USE_FRAM_PROGRAMMER"; 
const char* VPS_AUTH_TOKEN = "SETUP_REQUIRED_USE_FRAM_PROGRAMMER";
const char* DEVICE_ID = "UNCONFIGURED_DEVICE";

// Global pump control
bool pumpGlobalEnabled = true;  // Default ON
unsigned long pumpDisabledTime = 0;
const unsigned long PUMP_AUTO_ENABLE_MS = 30 * 60 * 1000; // 30 minutes

bool systemDisableRequested = false;
unsigned long systemDisabledTime = 0;
const unsigned long SYSTEM_AUTO_ENABLE_MS = 30 * 60 * 1000; // 30 minutes

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

// ================= 🆕 NEW: System State Control =================

void setSystemState(bool enabled) {
    if (!enabled) {
        systemDisableRequested = true;
        systemDisabledTime = millis();
        LOG_INFO("🛑 System disable requested - will pause at safe point");
        LOG_INFO("System will auto-enable in 30 minutes");
    } else {
        systemDisableRequested = false;
        systemDisabledTime = 0;
        LOG_INFO("✅ System manually enabled");
    }
}

void checkSystemAutoEnable() {
    if (systemDisableRequested && systemDisabledTime > 0) {
        unsigned long elapsed = millis() - systemDisabledTime;
        
        if (elapsed >= SYSTEM_AUTO_ENABLE_MS) {
            systemDisableRequested = false;
            systemDisabledTime = 0;
            LOG_INFO("✅ System auto-enabled after 30 minutes");
        }
    }
}

bool isSystemDisabled() {
    return systemDisableRequested;
}