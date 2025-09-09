

#include <Arduino.h>
#include "mode_config.h"

// ===============================
// CONDITIONAL INCLUDES
// ===============================

// Zawsze dostępne
#include "core/logging.h"
#include "hardware/rtc_controller.h"
#include "hardware/fram_controller.h"
#include "hardware/hardware_pins.h"
#include "cli/cli_handler.h" 

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
    
    // Basic hardware initialization
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH);
    
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
    
    digitalWrite(STATUS_LED_PIN, LOW);
    
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
    Serial.print("Device ID: ");
    
    // TEMPORARY: Still using hardcoded credentials
    // Will be replaced with FRAM loading later
    Serial.println("TEMPORARY_DEVICE_ID");
    
    // Initialize hardware
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH); 
    initWaterSensors();
    initPumpController();

    // Initialize storage and load settings
    initNVS(); 
    loadVolumeFromNVS();

    bool credentials_loaded = initCredentialsManager();
    
    // *** Print Device ID AFTER credentials loading ***
    Serial.print("Device ID: ");
    if (credentials_loaded) {
        Serial.println(getDeviceID());
    } else {
        Serial.println("FALLBACK_MODE");
    }
    
    //  // *** NOWE: Initialize credentials manager ***
    // if (initCredentialsManager()) {
    //     Serial.print("Device ID: ");
    //     Serial.println(getDeviceID());
    // } else {
    //     Serial.println("⚠️ Using fallback credentials");
    // }
    
    // Initialize RTC
    initializeRTC();
    Serial.print("RTC Status: ");
    Serial.println(getRTCInfo());
    
    // Initialize security
    initAuthManager();
    initSessionManager();
    initRateLimiter();
    
    // Initialize network
    initWiFi();
    initVPSLogger();
    
    // Initialize web server
    initWebServer();
    
    digitalWrite(STATUS_LED_PIN, LOW);
    
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

void setup() {
    // Initialize core systems
    delay(3000); // Wait for serial monitor
    initLogging();
    
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

void loop() {
#if MODE_PROGRAMMING
    // Programming mode loop - CLI interface
    static unsigned long lastBlink = 0;
    unsigned long now = millis();

    // Handle CLI commands
    handleCLI();
    
    // Status LED heartbeat (slow blink in programming mode)
    if (now - lastBlink >= 1000) {
        digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
        lastBlink = now;
    }
    
    // CLI handling will be added here
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
            triggerPump(currentPumpSettings.normalCycleSeconds, "AUTO_PUMP");
        }
        
        lastUpdate = now;
    }
    
    // Status LED heartbeat (slow blink when OK, fast when issues)
    static unsigned long lastBlink = 0;
    unsigned long blinkInterval = (isWiFiConnected() && isRTCWorking()) ? 2000 : 500;
    
    if (now - lastBlink >= blinkInterval) {
        digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
        lastBlink = now;
    }
    
    delay(100);
#endif
}