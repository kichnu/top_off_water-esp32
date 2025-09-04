#include <Arduino.h>
#include "config/config.h"
#include "core/logging.h"
#include "hardware/rtc_controller.h"
#include "hardware/water_sensors.h"
#include "hardware/pump_controller.h"
#include "network/wifi_manager.h"
#include "network/vps_logger.h"
#include "security/auth_manager.h"
#include "security/session_manager.h"
#include "security/rate_limiter.h"
#include "web/web_server.h"
#include "hardware/hardware_pins.h"
#include "hardware/fram_controller.h" 
#include "algorithm/water_algorithm.h"  // <-- DODAJ
#include "hardware/water_sensors.h"



void setup() {
    // Initialize core systems
    delay(6000);
    
    initLogging();
    
    LOG_INFO("=== ESP32-C3 Water System Starting ===");
    LOG_INFO("Device ID: %s", DEVICE_ID);

    // Check NVS partition
    LOG_INFO("ESP32 Flash size: %d bytes", ESP.getFlashChipSize());
    LOG_INFO("Free heap: %d bytes", ESP.getFreeHeap());


    
    // Initialize hardware
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH); 
    initWaterSensors();
    initPumpController();

    // Initialize and load settings from NVS
    // initNVS();
    // loadVolumeFromNVS();

    // This now initializes FRAM

    initNVS(); 
    
    // DODAJ TEST FRAM (opcjonalnie, usuń po weryfikacji):
    #ifdef DEBUG_BUILD
        testFRAM();  // Run FRAM test in debug mode
    #endif
    
    loadVolumeFromNVS(); 
    
    // Initialize RTC (now with proper error handling)
    initializeRTC();
    LOG_INFO("RTC Status: %s", getRTCInfo().c_str());
    
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
    
    LOG_INFO("=== System initialization complete ===");
    if (isWiFiConnected()) {
        LOG_INFO("Dashboard: http://%s", getLocalIP().toString().c_str());
    }
    LOG_INFO("Current time: %s", getCurrentTimestamp().c_str());
}

void loop() {
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();
    
    // DAILY RESTART CHECK - 24 godziny
    if (now > 86400000UL) { // 24h w ms
        LOG_INFO("=== DAILY RESTART: 24h uptime reached ===");
        
        // Opcjonalnie: zatrzymaj pompę przed restartem
        if (isPumpActive()) {
            stopPump();
            LOG_INFO("Pump stopped before restart");
            delay(1000); // Daj czas na zatrzymanie
        }
        
        LOG_INFO("System restarting in 3 seconds...");
        delay(3000);
        ESP.restart();
    }
    
    // Update water sensors every loop
    updateWaterSensors();

    waterAlgorithm.update();  // <-- DODAJ
    
    // Check water sensors  
    checkWaterSensors();

      // Check pump auto-enable timer
    checkPumpAutoEnable();
    
    // Update other systems every 100ms
    if (now - lastUpdate >= 100) {
        updatePumpController();
        updateSessionManager();
        updateRateLimiter();
        updateWiFi();
        
        // Check for auto pump trigger
        if (currentPumpSettings.autoModeEnabled && shouldActivatePump() && !isPumpActive()) {
            LOG_INFO("Auto pump triggered - water level low");
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
    
    
    delay(100); // Small delay to prevent watchdog issues
}