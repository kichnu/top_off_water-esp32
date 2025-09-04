#ifndef CONFIG_H
#define CONFIG_H

#include <IPAddress.h>

// ================= DEBUG & LOGGING CONFIG =================
// Ustaw na false jeśli masz problemy z stabilnością
#define ENABLE_FULL_LOGGING true
#define ENABLE_SERIAL_DEBUG true

// TYLKO DEKLARACJE (extern) - NIE DEFINICJE!
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;
extern const IPAddress STATIC_IP;
extern const IPAddress GATEWAY;
extern const IPAddress SUBNET;
extern const char* ADMIN_PASSWORD_HASH;
extern const IPAddress ALLOWED_IPS[];
extern const int ALLOWED_IPS_COUNT;
extern const char* VPS_URL;
extern const char* VPS_AUTH_TOKEN;
extern const char* DEVICE_ID;

// Global pump control
extern bool pumpGlobalEnabled;
extern unsigned long pumpDisabledTime;
extern const unsigned long PUMP_AUTO_ENABLE_MS;


// Stałe mogą być tutaj
const unsigned long SESSION_TIMEOUT_MS = 300000;
const unsigned long RATE_LIMIT_WINDOW_MS = 1000;
const int MAX_REQUESTS_PER_SECOND = 5;
const int MAX_FAILED_ATTEMPTS = 10;
const unsigned long BLOCK_DURATION_MS = 60000;

struct PumpSettings {
    uint16_t normalCycleSeconds = 15;
    uint16_t extendedCycleSeconds = 15;
    float volumePerSecond = 1.0;
    bool autoModeEnabled = true;
};

extern PumpSettings currentPumpSettings;

// Functions
void checkPumpAutoEnable();
void setPumpGlobalState(bool enabled);

void loadVolumeFromNVS();
void saveVolumeToNVS();
void initNVS();



#endif