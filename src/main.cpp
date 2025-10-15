#include <Arduino.h>
#include "mode_config.h"
#include "core/logging.h"
#include "hardware/rtc_controller.h"
#include "hardware/fram_controller.h"
#include "hardware/hardware_pins.h"
#include "cli/cli_handler.h" 

#if MODE_PROGRAMMING
void setupProgrammingMode();
#else  
void setupProductionMode();
#endif

#if MODE_PROGRAMMING
    // Programming mode includes - tylko podstawowe
    // CLI będzie dodany później
#else
    // Production mode includes - pełny water system
    #include "config/config.h"
    #include "config/credentials_manager.h" 
    #include "hardware/water_sensors.h"
    #include "hardware/pump_controller.h"
    #include "network/wifi_manager.h"
    #include "network/vps_logger.h"
    #include "security/auth_manager.h"
    #include "security/session_manager.h"
    #include "security/rate_limiter.h"
    #include "web/web_server.h"
    #include "algorithm/water_algorithm.h"
#endif

void setup() {
    // Initialize core systems
    initLogging();
    delay(5000); // Wait for serial monitor
    
    Serial.print("=== MODE: ");
    Serial.print(MODE_NAME);
    Serial.println(" ===");
    
    // Check ESP32 resources
    Serial.print("ESP32 Flash size: ");
    Serial.print(ESP.getFlashChipSize());
    Serial.println(" bytes");
    Serial.print("Free heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    
#if MODE_PROGRAMMING
    setupProgrammingMode();
#else
    setupProductionMode();
#endif
}
// ===============================
// MODE-SPECIFIC SETUP FUNCTIONS
// ===============================

#if MODE_PROGRAMMING
void setupProgrammingMode() {
    Serial.println();
    Serial.println("========================================");
    Serial.println("    ESP32-C3 FRAM PROGRAMMER v1.0");
    Serial.println("    Programming Mode - CLI Interface");
    Serial.println("========================================");
    Serial.println();

    // Initialize RTC and FRAM
    initializeRTC();
    Serial.print("RTC Status: ");
    Serial.println(getRTCInfo());
    
    if (initFRAM()) {
        Serial.println("✅ FRAM initialized successfully");
    } else {
        Serial.println("❌ FRAM initialization failed!");
    }

    initCLI();
    
    Serial.println();
    Serial.println("=== PROGRAMMING MODE READY ===");
    Serial.println("CLI interface will be available here");
    Serial.println("Type 'help' for available commands");
    Serial.println();
}
#else

void setupProductionMode() {
    Serial.println();
    Serial.println("=== ESP32-C3 Water System Starting ===");
    Serial.println("Production Mode - Full Water System");

    initWaterSensors();
    initPumpController();

    initNVS();
    loadVolumeFromNVS();

    bool credentials_loaded = initCredentialsManager();
    
    Serial.print("Device ID: ");
    if (credentials_loaded) {
        Serial.println(getDeviceID());
    } else {
        Serial.println("FALLBACK_MODE");
    }
    
    Serial.println("[INIT] Initializing network...");
    initWiFi();

    LOG_INFO("[INIT] Initializing RTC...");
    initializeRTC();
    LOG_INFO("RTC Status: %s", getRTCInfo().c_str());
    
    LOG_INFO("[INIT] Waiting for RTC to stabilize...");
    delay(2000);
    
    if (!isRTCWorking()) {
        LOG_WARNING("⚠️ WARNING: RTC not working properly");
        LOG_WARNING("⚠️ Daily volume tracking may be affected");
    }
    
    LOG_INFO("[INIT] Initializing daily volume tracking...");
    waterAlgorithm.initDailyVolume();

    // Initialize security
    initAuthManager();
    initSessionManager();
    initRateLimiter();
    
    // Initialize VPS logger
    initVPSLogger();
    
    // Initialize web server
    initWebServer();
    
    // Post-init diagnostics
    Serial.println();
    LOG_INFO("====================================");
    LOG_INFO("SYSTEM POST-INIT STATUS");
    LOG_INFO("====================================");
    LOG_INFO("RTC Working: %s", isRTCWorking() ? "YES" : "NO");
    LOG_INFO("RTC Info: %s", getRTCInfo().c_str());
    LOG_INFO("Current Time: %s", getCurrentTimestamp().c_str());
    LOG_INFO("Water Algorithm:");
    LOG_INFO("  State: %s", waterAlgorithm.getStateString());
    LOG_INFO("  Daily Volume: %d / %d ml", 
             waterAlgorithm.getDailyVolume(), FILL_WATER_MAX);
    LOG_INFO("  UTC Day: %lu", waterAlgorithm.getLastResetUTCDay());
    LOG_INFO("====================================");
    Serial.println();
    
    Serial.println("=== System initialization complete ===");
    if (isWiFiConnected()) {
        Serial.print("Dashboard: http://");
        Serial.println(getLocalIP().toString());
    }
    Serial.print("Current time: ");
    Serial.println(getCurrentTimestamp());
}

#endif

// ===============================
// ARDUINO SETUP/LOOP
// ===============================

void loop() {
#if MODE_PROGRAMMING
    static unsigned long lastBlink = 0;
    unsigned long now = millis();
    handleCLI();
    delay(10);
    
#else
    // Production mode loop - full water system (unchanged)
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();
    
    // DAILY RESTART CHECK - 24 godziny
    if (now > 86400000UL) { // 24h w ms
        Serial.println("=== DAILY RESTART: 24h uptime reached ===");
        
        if (isPumpActive()) {
            stopPump();
            Serial.println("Pump stopped before restart");
            delay(1000);
        }
        
        Serial.println("System restarting in 3 seconds...");
        delay(3000);
        ESP.restart();
    }
    
    // Update water sensors every loop
    updateWaterSensors();
    waterAlgorithm.update();
    checkWaterSensors();
    checkPumpAutoEnable();

    // Update other systems every 100ms
    if (now - lastUpdate >= 100) {
        updatePumpController();
        updateSessionManager();
        updateRateLimiter();
        updateWiFi();
        
        // Check for auto pump trigger
        if (currentPumpSettings.autoModeEnabled && shouldActivatePump() && !isPumpActive()) {
            Serial.println("Auto pump triggered - water level low");
            triggerPump(currentPumpSettings.manualCycleSeconds, "AUTO_PUMP");
        }
        
        lastUpdate = now;
    }

    static unsigned long lastBlink = 0;
    unsigned long blinkInterval = (isWiFiConnected() && isRTCWorking()) ? 2000 : 500;
    
    delay(100);
#endif
}