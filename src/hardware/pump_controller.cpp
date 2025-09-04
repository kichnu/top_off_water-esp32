#include "pump_controller.h"
#include "../config/hardware_pins.h"
#include "../config/config.h"
#include "../network/vps_logger.h"
#include "../hardware/rtc_controller.h"
#include "../core/logging.h"
#include <math.h>

#include "../algorithm/water_algorithm.h"  // <-- DODAJ



bool pumpRunning = false;
unsigned long pumpStartTime = 0;
unsigned long pumpDuration = 0;
String currentActionType = "";

static bool pumpActive = false;
static bool manualPumpActive = false;

void initPumpController() {
    pinMode(PUMP_RELAY_PIN, OUTPUT);
    digitalWrite(PUMP_RELAY_PIN, LOW);
    
    pumpRunning = false;
    LOG_INFO("Pump controller initialized");
}

void updatePumpController() {

        // Check global pump state - stop if disabled
    if (!pumpGlobalEnabled && pumpRunning) {
        digitalWrite(PUMP_RELAY_PIN, LOW);
        pumpRunning = false;
        LOG_INFO("Pump stopped - globally disabled");
        return;
    }

    if (pumpRunning && (millis() - pumpStartTime >= pumpDuration)) {
        // Stop pump and log event
        digitalWrite(PUMP_RELAY_PIN, LOW);
        pumpRunning = false;
        
        uint16_t actualDuration = (millis() - pumpStartTime) / 1000;
        // uint16_t volumeML = actualDuration * currentPumpSettings.volumePerSecond;
        uint16_t volumeML = (uint16_t)round(actualDuration * currentPumpSettings.volumePerSecond);
        
        LOG_INFO("Pump stopped after %d seconds, estimated volume: %d ml", 
                 actualDuration, volumeML);

        // Log to VPS (skip AUTO_PUMP - handled by algorithm)
        if (!currentActionType.startsWith("AUTO")) {
            logEventToVPS(currentActionType, volumeML, getCurrentTimestamp());
        }
        currentActionType = "";         
        
        // Log to VPS
        // logEventToVPS(currentActionType, volumeML, getCurrentTimestamp());
        // currentActionType = "";
    }

      static bool wasManualActive = false;
    if (wasManualActive && !manualPumpActive && !pumpRunning) {
        waterAlgorithm.onManualPumpComplete();
        wasManualActive = false;
    }
    if (manualPumpActive) {
        wasManualActive = true;
    }
}

bool triggerPump(uint16_t durationSeconds, const String& actionType) {
    LOG_INFO("triggerPump called: %s for %ds", actionType.c_str(), durationSeconds);
    
    if (pumpRunning) {
        LOG_WARNING("Pump already running, ignoring trigger");
        return false;
    }

    if (!pumpGlobalEnabled) {
        LOG_INFO("Pump trigger blocked - globally disabled");
        return false;
    }
    
    // TYLKO dla manual pump notify algorithm
    if (actionType.startsWith("MANUAL")) {
        if (!waterAlgorithm.requestManualPump(durationSeconds * 1000)) {
            LOG_WARNING("Algorithm rejected manual pump request");
            return false;
        }
    }
    // Dla AUTO_PUMP nie wywołuj requestManualPump!
    
    digitalWrite(PUMP_RELAY_PIN, HIGH);
    pumpRunning = true;
    pumpStartTime = millis();
    pumpDuration = durationSeconds * 1000UL;
    currentActionType = actionType;
    
    LOG_INFO("Pump started: %s for %d seconds", actionType.c_str(), durationSeconds);
    return true;
}

// bool triggerPump(uint16_t durationSeconds, const String& actionType) {

//     // Notify algorithm about manual pump
//     if (!waterAlgorithm.requestManualPump(pumpDuration)) {
//         LOG_WARNING("Algorithm rejected manual pump request");
//         return false;
//     }

//     LOG_INFO("%s", "TRigger pump");
//     if (pumpRunning) {
//         LOG_WARNING("Pump already running, ignoring trigger");
//         return false;
//     }

//         if (!pumpGlobalEnabled) {
//         LOG_INFO("Pump trigger blocked - globally disabled");
//         return false;
//     }
    
//     digitalWrite(PUMP_RELAY_PIN, HIGH);
//     pumpRunning = true;
//     pumpStartTime = millis();
//     pumpDuration = durationSeconds * 1000UL;
//     currentActionType = actionType;
    
//     LOG_INFO("Pump started: %s for %d seconds", actionType.c_str(), durationSeconds);
//     return true;
// }

bool isPumpActive() {
    return pumpRunning;
}

uint32_t getPumpRemainingTime() {
    if (!pumpRunning) return 0;
    
    unsigned long elapsed = millis() - pumpStartTime;
    if (elapsed >= pumpDuration) return 0;
    
    return (pumpDuration - elapsed) / 1000;
}

void stopPump() {
    if (pumpRunning) {
        digitalWrite(PUMP_RELAY_PIN, LOW);
        pumpRunning = false;
        LOG_INFO("Pump manually stopped");
    }
}
