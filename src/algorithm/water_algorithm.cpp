#include "water_algorithm.h"
#include "../core/logging.h"
#include "../hardware/pump_controller.h"
#include "../hardware/water_sensors.h"
// #include <algorithm>
#include "../config/config.h"  // dla currentPumpSettings
#include "../hardware/hardware_pins.h"
#include "algorithm_config.h" 
#include "../hardware/fram_controller.h"  
#include "../network/vps_logger.h"
#include "../hardware/rtc_controller.h"  // <-- DODAJ I TĘ LINIĘ

WaterAlgorithm waterAlgorithm;

WaterAlgorithm::WaterAlgorithm() {
    currentState = STATE_IDLE;
    resetCycle();
    dayStartTime = millis();
    dailyVolumeML = 0;
    lastError = ERROR_NONE;
    errorSignalActive = false;
    lastSensor1State = false;
    lastSensor2State = false;
    todayCycles.clear(); //nowa metoda

    framDataLoaded = false;
    
    lastFRAMCleanup = millis();
    framCycles.clear();

    loadCyclesFromStorage();

    ErrorStats stats;

    if (loadErrorStatsFromFRAM(stats)) {
        LOG_INFO("✅ Error statistics loaded from FRAM at startup");
    } else {
        LOG_WARNING("⚠️ Could not load error stats from FRAM at startup");
    }
    
    pinMode(ERROR_SIGNAL_PIN, OUTPUT);
    digitalWrite(ERROR_SIGNAL_PIN, HIGH);
    pinMode(RESET_PIN, INPUT_PULLUP);
}

void WaterAlgorithm::resetCycle() {

    currentCycle = {};
    currentCycle.timestamp = getCurrentTimeSeconds(); 
    triggerStartTime = 0;
    sensor1TriggerTime = 0;
    sensor2TriggerTime = 0;
    sensor1ReleaseTime = 0;
    sensor2ReleaseTime = 0;
    pumpStartTime = 0;
    waitingForSecondSensor = false;
    pumpAttempts = 0;
    cycleLogged = false;
    permission_log = true;
    waterFailDetected = false;
}

void WaterAlgorithm::update() {
    // Check for reset button in error state
    if (currentState == STATE_ERROR) {
        if (digitalRead(RESET_PIN) == LOW) {
            LOG_INFO("Reset button pressed, clearing error");
            resetFromError();
        }
        updateErrorSignal();
        return;
    }
    
    // Daily volume reset (every 24h)
    if (millis() - dayStartTime > 86400000UL) { // 24 hours in ms
        dayStartTime = millis();
        dailyVolumeML = 0;
        todayCycles.clear();
        LOG_INFO("Daily volume counter reset");
    }
    
    uint32_t currentTime = getCurrentTimeSeconds(); // <-- ZMIANA: używaj sekund
    uint32_t stateElapsed = currentTime - stateStartTime;

    switch (currentState) {

        case STATE_IDLE:

            break;

        case STATE_TRYB_1_WAIT:

            if (stateElapsed >= TIME_GAP_1_MAX) { // <-- USUŃ * 1000
                // Timeout - proceed with max value
                currentCycle.time_gap_1 = TIME_GAP_1_MAX;
                LOG_INFO("TRYB_1: TIME_GAP_1 timeout, using max: %ds", TIME_GAP_1_MAX);
                
                // Evaluate result
                if (sensor_time_match_function(currentCycle.time_gap_1, THRESHOLD_1)) {
                    currentCycle.sensor_results |= PumpCycle::RESULT_GAP1_FAIL;
                }
                
                // Move to delay before pump
                currentState = STATE_TRYB_1_DELAY;
                stateStartTime = currentTime;
                LOG_INFO("TRYB_1: Starting TIME_TO_PUMP delay (%ds)", TIME_TO_PUMP);
            }
            break;
                    
        case STATE_TRYB_1_DELAY:

            // Wait TIME_TO_PUMP from original trigger
            if (currentTime - triggerStartTime >= TIME_TO_PUMP) { // <-- USUŃ * 1000
                // Calculate pump work time from volume setting
                uint16_t pumpWorkTime = calculatePumpWorkTime(currentPumpSettings.volumePerSecond);
                
                // Validate pump work time
                if (!validatePumpWorkTime(pumpWorkTime)) {
                    LOG_ERROR("PUMP_WORK_TIME (%ds) exceeds WATER_TRIGGER_MAX_TIME (%ds)", 
                            pumpWorkTime, WATER_TRIGGER_MAX_TIME);
                    // Fallback to maximum allowed time
                    pumpWorkTime = WATER_TRIGGER_MAX_TIME - 10; // Leave 10s margin
                }
                
                LOG_INFO("TRYB_2: Starting pump attempt %d/%d for %ds", 
                        pumpAttempts + 1, PUMP_MAX_ATTEMPTS, pumpWorkTime);
                
                pumpStartTime = currentTime;
                pumpAttempts++;
                
                // Start pump (duration in seconds, but triggerPump expects milliseconds)
                triggerPump(pumpWorkTime, "AUTO_PUMP");
                
                currentCycle.pump_duration = pumpWorkTime; // <-- USUŃ / 1000
                currentState = STATE_TRYB_2_PUMP;
                stateStartTime = currentTime;
            }
            break;
               
        case STATE_TRYB_2_PUMP:

            // Pump is running, wait for completion
            if (!isPumpActive()) {
                // Pump finished, check sensor response
                LOG_INFO("TRYB_2: Pump finished, checking sensors");
                currentState = STATE_TRYB_2_VERIFY;
                stateStartTime = currentTime;
            }
            break;

        case STATE_TRYB_2_VERIFY: {

            static uint32_t lastStatusLog = 0;
            if (currentTime - lastStatusLog >= 5) { // Co 5 sekund

                uint32_t timeSincePumpStart = currentTime - pumpStartTime;
                LOG_INFO("TRYB_2_VERIFY: Waiting for sensors... %ds/%ds (attempt %d/%d)", 
                        timeSincePumpStart, WATER_TRIGGER_MAX_TIME, pumpAttempts, PUMP_MAX_ATTEMPTS);
                lastStatusLog = currentTime;
            }

            // Check if sensors deactivated (water level rose)
            bool sensorsOK = !readWaterSensor1() && !readWaterSensor2();
            
            if (sensorsOK) {
                // Success! Calculate water trigger time
                calculateWaterTrigger();
                LOG_INFO("TRYB_2: Sensors deactivated, water_trigger_time: %ds", 
                        currentCycle.water_trigger_time);
                
                // Check if we need to measure TIME_GAP_2 - TYLKO gdy TRYB_1_result == 0
                uint8_t tryb1Result = sensor_time_match_function(currentCycle.time_gap_1, THRESHOLD_1);
                if (tryb1Result == 0) {
                    currentState = STATE_TRYB_2_WAIT_GAP2;
                    stateStartTime = currentTime;
                    waitingForSecondSensor = true;
                    LOG_INFO("TRYB_2: TRYB_1_result=0, waiting for TIME_GAP_2");
                } else {
                    LOG_INFO("TRYB_2: TRYB_1_result=1, skipping TIME_GAP_2");
                    currentState = STATE_LOGGING;
                    stateStartTime = currentTime;
                }
            } else {
                // POPRAWKA: liczy czas od PUMP_START, nie od state start
                uint32_t timeSincePumpStart = currentTime - pumpStartTime;
                
                if (timeSincePumpStart >= WATER_TRIGGER_MAX_TIME) {
                    // Timeout - sensors didn't respond
                    currentCycle.water_trigger_time = WATER_TRIGGER_MAX_TIME;

                        // 🆕 MARK FAIL on first timeout detection
                    if (WATER_TRIGGER_MAX_TIME >= THRESHOLD_WATER) {
                        waterFailDetected = true;  // Set flag, never reset
                        LOG_INFO("WATER fail detected in attempt %d/%d", pumpAttempts, PUMP_MAX_ATTEMPTS);
                    }
                    
                    LOG_WARNING("TRYB_2: Timeout after %ds (limit: %ds), attempt %d/%d", 
                            timeSincePumpStart, WATER_TRIGGER_MAX_TIME, 
                            pumpAttempts, PUMP_MAX_ATTEMPTS);
                    
                    if (pumpAttempts < PUMP_MAX_ATTEMPTS) {
                        // Try again - go back to pump
                        LOG_WARNING("TRYB_2: Retrying pump attempt %d/%d", 
                                pumpAttempts + 1, PUMP_MAX_ATTEMPTS);
                        
                        // Go directly to pump again (skip TIME_TO_PUMP delay)
                        currentState = STATE_TRYB_1_DELAY;
                        stateStartTime = currentTime - TIME_TO_PUMP; // Immediate retry
                    } else {
                            // All attempts failed - ERROR
                        LOG_ERROR("TRYB_2: All %d pump attempts failed!", PUMP_MAX_ATTEMPTS);
                        currentCycle.error_code = ERROR_PUMP_FAILURE;
                                        
                        // 🆕 DODAJ: Log failed cycle before ERROR state
                        LOG_INFO("Logging failed cycle before entering ERROR state");
                        logCycleComplete();
                                        
                        startErrorSignal(ERROR_PUMP_FAILURE);
                        currentState = STATE_ERROR;
                    }
                }
            }
            break;
        }

        case STATE_TRYB_2_WAIT_GAP2: 
            // DEBUGGING - sprawdź czy mamy release times
            static bool debugOnce = true;
            if (debugOnce) {
                LOG_INFO("DEBUG GAP2: s1Release=%ds, s2Release=%ds, waiting=%d", 
                        sensor1ReleaseTime, sensor2ReleaseTime, waitingForSecondSensor);
                debugOnce = false;
            }
            
            // Check if both sensors released
            if (sensor1ReleaseTime && sensor2ReleaseTime && waitingForSecondSensor) {
                // Calculate TIME_GAP_2
                calculateTimeGap2();
                waitingForSecondSensor = false;
                LOG_INFO("TRYB_2: TIME_GAP_2 calculated successfully");
                
                currentState = STATE_LOGGING;
                stateStartTime = currentTime;
            } else if (stateElapsed >= TIME_GAP_2_MAX) {
                currentCycle.time_gap_2 = TIME_GAP_2_MAX;

                uint8_t result = sensor_time_match_function(currentCycle.time_gap_2, THRESHOLD_2);
                if (result == 1) {
                    currentCycle.sensor_results |= PumpCycle::RESULT_GAP2_FAIL;
                }

                LOG_WARNING("TRYB_2: TIME_GAP_2 timeout - s1Release=%ds, s2Release=%ds", 
                        sensor1ReleaseTime, sensor2ReleaseTime);
              
                currentState = STATE_LOGGING;
                stateStartTime = currentTime;
            }
    break;
            
        case STATE_LOGGING:

            if(permission_log){
                LOG_INFO("==================case STATE_LOGGING");
                permission_log = false;      
            } 

        
            // Log cycle and check daily limit
            if (!cycleLogged) { // Wykonaj tylko raz na początku stanu
                logCycleComplete();
                cycleLogged = true;
                
                // Check daily limit
                if (dailyVolumeML > FILL_WATER_MAX) {
                    LOG_ERROR("Daily limit exceeded! %dml > %dml", dailyVolumeML, FILL_WATER_MAX);
                    currentCycle.error_code = ERROR_DAILY_LIMIT;
                    startErrorSignal(ERROR_DAILY_LIMIT);
                    currentState = STATE_ERROR;
                    break;
                }
            }
            
            // Return to idle after logging time
            if (stateElapsed >= LOGGING_TIME) { 
                LOG_INFO("Cycle complete, returning to IDLE######################################################");
                currentState = STATE_IDLE;
                resetCycle();
            }
            break;
    }
}

void WaterAlgorithm::onSensorStateChange(uint8_t sensorNum, bool triggered) {
    uint32_t currentTime = getCurrentTimeSeconds(); // <-- ZMIANA: sekundy zamiast millis()
    
    // Update sensor states
    if (sensorNum == 1) {
        lastSensor1State = triggered;
        if (triggered) {
            sensor1TriggerTime = currentTime;
        } else {
            sensor1ReleaseTime = currentTime;
        }
    } else if (sensorNum == 2) {
        lastSensor2State = triggered;
        if (triggered) {
            sensor2TriggerTime = currentTime;
        } else {
            sensor2ReleaseTime = currentTime;
        }
    }
    
    // Handle state transitions based on sensor changes
    switch (currentState) {
        case STATE_IDLE:
            if (triggered && (lastSensor1State || lastSensor2State)) {
                // TRIGGER detected!
                LOG_INFO("TRIGGER detected! Starting TRYB_1");
                triggerStartTime = currentTime;
                currentCycle.trigger_time = currentTime;
                currentState = STATE_TRYB_1_WAIT;
                stateStartTime = currentTime;
                waitingForSecondSensor = true;
            }
            break;
            
        case STATE_TRYB_1_WAIT:
            if (waitingForSecondSensor && sensor1TriggerTime && sensor2TriggerTime) {
                // Both sensors triggered, calculate TIME_GAP_1
                calculateTimeGap1();
                waitingForSecondSensor = false;
                
                // Evaluate result
                if (sensor_time_match_function(currentCycle.time_gap_1, THRESHOLD_1)) {
                    currentCycle.sensor_results |= PumpCycle::RESULT_GAP1_FAIL;
                }
                
                // Continue waiting for TIME_TO_PUMP
                currentState = STATE_TRYB_1_DELAY;
                stateStartTime = currentTime;
                LOG_INFO("TRYB_1: Both sensors triggered, TIME_GAP_1=%ds", 
                        currentCycle.time_gap_1);
            }
            break;
            
        case STATE_TRYB_2_WAIT_GAP2:
            if (!triggered && waitingForSecondSensor && 
                sensor1ReleaseTime && sensor2ReleaseTime) {
                // Both sensors released, calculate TIME_GAP_2
                calculateTimeGap2();
                waitingForSecondSensor = false;
                LOG_INFO("TRYB_2: TIME_GAP_2=%dms", currentCycle.time_gap_2);
            }
            break;
            
        default:
            // Ignore sensor changes in other states
            break;
    }
}

void WaterAlgorithm::calculateTimeGap1() {
    if (sensor1TriggerTime && sensor2TriggerTime) {
        currentCycle.time_gap_1 = abs((int32_t)sensor2TriggerTime - 
                                      (int32_t)sensor1TriggerTime);
        
        // Wywołaj funkcję oceniającą zgodnie ze specyfikacją
        uint8_t result = sensor_time_match_function(currentCycle.time_gap_1, THRESHOLD_1);
        if (result == 1) {
            currentCycle.sensor_results |= PumpCycle::RESULT_GAP1_FAIL;
        }
        
        LOG_INFO("TIME_GAP_1: %ds, result: %d (threshold: %ds)", 
                currentCycle.time_gap_1, result, THRESHOLD_1);
    } else {
        LOG_WARNING("TIME_GAP_1 not calculated: s1Time=%ds, s2Time=%ds", 
                   sensor1TriggerTime, sensor2TriggerTime);
    }
}

void WaterAlgorithm::calculateTimeGap2() {
    if (sensor1ReleaseTime && sensor2ReleaseTime) {
        // Oblicz różnicę w sekundach (bez dzielenia przez 1000!)
        currentCycle.time_gap_2 = abs((int32_t)sensor2ReleaseTime - 
                                      (int32_t)sensor1ReleaseTime);
        
        // Wywołaj funkcję oceniającą zgodnie ze specyfikacją
        uint8_t result = sensor_time_match_function(currentCycle.time_gap_2, THRESHOLD_2);
        if (result == 1) {
            currentCycle.sensor_results |= PumpCycle::RESULT_GAP2_FAIL;
        }
        
        LOG_INFO("TIME_GAP_2: %ds, result: %d (threshold: %ds)", 
                currentCycle.time_gap_2, result, THRESHOLD_2);
    } else {
        LOG_WARNING("TIME_GAP_2 not calculated: s1Release=%ds, s2Release=%ds", 
                   sensor1ReleaseTime, sensor2ReleaseTime);
    }
}

void WaterAlgorithm::calculateWaterTrigger() {
    uint32_t earliestRelease = 0;
    
    // Znajdź najwcześniejszą deaktywację po starcie pompy
    if (sensor1ReleaseTime > pumpStartTime) {
        earliestRelease = sensor1ReleaseTime;
    }
    if (sensor2ReleaseTime > pumpStartTime && 
        (earliestRelease == 0 || sensor2ReleaseTime < earliestRelease)) {
        earliestRelease = sensor2ReleaseTime;
    }
    
    if (earliestRelease > 0) {
        // Różnica już w sekundach - bez dzielenia przez 1000!
        currentCycle.water_trigger_time = earliestRelease - pumpStartTime;
        
        // Sanity check
        if (currentCycle.water_trigger_time > WATER_TRIGGER_MAX_TIME) {
            currentCycle.water_trigger_time = WATER_TRIGGER_MAX_TIME;
        }
        
        LOG_INFO("WATER_TRIGGER_TIME: %ds", currentCycle.water_trigger_time);
        
        // Evaluate result
        if (sensor_time_match_function(currentCycle.water_trigger_time, THRESHOLD_WATER)) {
            currentCycle.sensor_results |= PumpCycle::RESULT_WATER_FAIL;
        }
    } else {
        // No valid release detected
        currentCycle.water_trigger_time = WATER_TRIGGER_MAX_TIME;
        currentCycle.sensor_results |= PumpCycle::RESULT_WATER_FAIL;
        LOG_WARNING("No sensor release detected after pump start");
    }

        if (currentCycle.water_trigger_time >= THRESHOLD_WATER) {
        waterFailDetected = true;
        LOG_INFO("WATER fail detected in successful attempt");
    }
}

void WaterAlgorithm::logCycleComplete() {
    // SPRAWDZENIE: czy currentCycle zostało gdzieś wyzerowane
    if (currentCycle.time_gap_1 == 0 && sensor1TriggerTime == 0 && sensor2TriggerTime == 0) {
        LOG_ERROR("CRITICAL: currentCycle.time_gap_1 was RESET! Reconstructing...");
        
        // Spróbuj odtworzyć TIME_GAP_1 z logów timing
        if (triggerStartTime > 0) {
            LOG_INFO("Attempting to reconstruct TIME_GAP_1 from available data");
            // W tym przypadku nie możemy odtworzyć, ale przynajmniej wiemy że był problem
        }
    }

    // Calculate volume based on actual pump duration
    uint16_t actualVolumeML = (uint16_t)(currentCycle.pump_duration * currentPumpSettings.volumePerSecond);
    currentCycle.volume_dose = actualVolumeML;
    currentCycle.pump_attempts = pumpAttempts;

        // 🆕 DODAJ TUTAJ - SET final fail flag based on any failure
    if (waterFailDetected) {
        currentCycle.sensor_results |= PumpCycle::RESULT_WATER_FAIL;
        LOG_INFO("Final WATER fail flag set due to timeout in any attempt");
    }
    
    // Add to daily volume (use actual volume, not fixed SINGLE_DOSE_VOLUME)
    dailyVolumeML += actualVolumeML;
    
    // Store in today's cycles (RAM)
    todayCycles.push_back(currentCycle);
    
    // Keep only last 50 cycles in RAM (FRAM will store more)
    if (todayCycles.size() > 50) {
        todayCycles.erase(todayCycles.begin());
    }
    
    // Save cycle to FRAM (for debugging and history)
    saveCycleToStorage(currentCycle);
    
    // *** NOWE: Aktualizuj sumy błędów w FRAM ***
    uint8_t gap1_increment = (currentCycle.sensor_results & PumpCycle::RESULT_GAP1_FAIL) ? 1 : 0;
    uint8_t gap2_increment = (currentCycle.sensor_results & PumpCycle::RESULT_GAP2_FAIL) ? 1 : 0;
    uint8_t water_increment = (currentCycle.sensor_results & PumpCycle::RESULT_WATER_FAIL) ? 1 : 0;
    
    if (gap1_increment || gap2_increment || water_increment) {
        if (incrementErrorStats(gap1_increment, gap2_increment, water_increment)) {
            LOG_INFO("Error stats updated: GAP1+%d, GAP2+%d, WATER+%d", 
                    gap1_increment, gap2_increment, water_increment);
        } else {
            LOG_WARNING("Failed to update error stats in FRAM");
        }
    }
    
    // *** ZMIENIONE: Log cycle data to VPS z sumami błędów ***
    String timestamp = getCurrentTimestamp();
    if (logCycleToVPS(currentCycle, timestamp)) {
        LOG_INFO("Cycle data sent to VPS successfully");
    } else {
        LOG_WARNING("Failed to send cycle data to VPS");
    }
    
    LOG_INFO("=== CYCLE COMPLETE ===");
    LOG_INFO("Actual volume: %dml (pump_duration: %ds)", actualVolumeML, currentCycle.pump_duration);
    LOG_INFO("TIME_GAP_1: %ds (fail=%d)", currentCycle.time_gap_1, gap1_increment);
    LOG_INFO("TIME_GAP_2: %ds (fail=%d)", currentCycle.time_gap_2, gap2_increment);
    LOG_INFO("WATER_TRIGGER_TIME: %ds (fail=%d)", currentCycle.water_trigger_time, water_increment);
    LOG_INFO("Daily volume: %dml / %dml", dailyVolumeML, FILL_WATER_MAX);

    
}

bool WaterAlgorithm::requestManualPump(uint16_t duration_ms) {
    if (currentState == STATE_ERROR) {
        LOG_WARNING("Cannot start manual pump in error state");
        return false;
    }
    
    // SPRAWDŹ czy to AUTO_PUMP podczas automatycznego cyklu
    if (currentState != STATE_IDLE) {
        // Jeśli jesteśmy w automatycznym cyklu, nie resetuj danych!
        if (currentState == STATE_TRYB_1_DELAY || 
            currentState == STATE_TRYB_2_PUMP ||
            currentState == STATE_TRYB_2_VERIFY) {
            
            LOG_INFO("AUTO_PUMP during automatic cycle - preserving cycle data");
            // NIE wywołuj resetCycle() - zachowaj zebrane dane!
            return true; // Pozwól na pompę, ale nie resetuj
        } else {
            // Manual interrupt w innych stanach
            LOG_INFO("Manual pump interrupting current cycle");
            currentState = STATE_MANUAL_OVERRIDE;
            resetCycle();
        }
    }
    
    return true;
}

void WaterAlgorithm::onManualPumpComplete() {
    if (currentState == STATE_MANUAL_OVERRIDE) {
        LOG_INFO("Manual pump complete, returning to IDLE");
        currentState = STATE_IDLE;
        resetCycle();
    }
}

const char* WaterAlgorithm::getStateString() const {
    switch (currentState) {
        case STATE_IDLE: return "IDLE";
        case STATE_TRYB_1_WAIT: return "TRYB_1_WAIT";
        case STATE_TRYB_1_DELAY: return "TRYB_1_DELAY";
        case STATE_TRYB_2_PUMP: return "TRYB_2_PUMP";
        case STATE_TRYB_2_VERIFY: return "TRYB_2_VERIFY";
        case STATE_TRYB_2_WAIT_GAP2: return "TRYB_2_WAIT_GAP2";
        case STATE_LOGGING: return "LOGGING";
        case STATE_ERROR: return "ERROR";
        case STATE_MANUAL_OVERRIDE: return "MANUAL_OVERRIDE";
        default: return "UNKNOWN";
    }
}

bool WaterAlgorithm::isInCycle() const {
    return currentState != STATE_IDLE && currentState != STATE_ERROR;
}

std::vector<PumpCycle> WaterAlgorithm::getRecentCycles(size_t count) {
    size_t start = todayCycles.size() > count ? todayCycles.size() - count : 0;
    return std::vector<PumpCycle>(todayCycles.begin() + start, todayCycles.end());
}

void WaterAlgorithm::startErrorSignal(ErrorCode error) {
    lastError = error;
    errorSignalActive = true;
    errorSignalStart = millis();
    errorPulseCount = 0;
    errorPulseState = false;
    pinMode(ERROR_SIGNAL_PIN, OUTPUT);
    digitalWrite(ERROR_SIGNAL_PIN, LOW);
    
    LOG_ERROR("Starting error signal: %s", 
             error == ERROR_DAILY_LIMIT ? "ERR1" :
             error == ERROR_PUMP_FAILURE ? "ERR2" : "ERR0");
}

void WaterAlgorithm::updateErrorSignal() {
    if (!errorSignalActive) return;
    
    uint32_t elapsed = millis() - errorSignalStart;
    uint8_t pulsesNeeded = (lastError == ERROR_DAILY_LIMIT) ? 1 :
                           (lastError == ERROR_PUMP_FAILURE) ? 2 : 3;
    
    // Calculate current position in signal pattern
    uint32_t cycleTime = 0;
    for (uint8_t i = 0; i < pulsesNeeded; i++) {
        cycleTime += ERROR_PULSE_HIGH + ERROR_PULSE_LOW;
    }
    cycleTime += ERROR_PAUSE - ERROR_PULSE_LOW; // Remove last LOW, add PAUSE
    
    uint32_t posInCycle = elapsed % cycleTime;
    
    // Determine if we should be HIGH or LOW
    bool shouldBeHigh = false;
    uint32_t currentPos = 0;
    
    for (uint8_t i = 0; i < pulsesNeeded; i++) {
        if (posInCycle >= currentPos && posInCycle < currentPos + ERROR_PULSE_HIGH) {
            shouldBeHigh = true;
            break;
        }
        currentPos += ERROR_PULSE_HIGH + ERROR_PULSE_LOW;
    }
    
    // Update pin state
    if (shouldBeHigh != errorPulseState) {
        errorPulseState = shouldBeHigh;
        pinMode(ERROR_SIGNAL_PIN, OUTPUT);
        digitalWrite(ERROR_SIGNAL_PIN, errorPulseState ? LOW : HIGH);
    }
}

void WaterAlgorithm::resetFromError() {
    lastError = ERROR_NONE;
    errorSignalActive = false;
    pinMode(ERROR_SIGNAL_PIN, OUTPUT);
    digitalWrite(ERROR_SIGNAL_PIN, HIGH);
    currentState = STATE_IDLE;
    resetCycle();
    LOG_INFO("System reset from error state");
}

void WaterAlgorithm::loadCyclesFromStorage() {
    LOG_INFO("Loading cycles from FRAM...");
    
    // Load recent cycles from FRAM
    if (loadCyclesFromFRAM(framCycles, 200)) { // Load max 200 cycles
        framDataLoaded = true;
        LOG_INFO("Loaded %d cycles from FRAM", framCycles.size());
        
        // Calculate daily volume from today's cycles
        uint32_t todayStart = (millis() / 1000) - (millis() / 1000) % 86400; // Start of today
        dailyVolumeML = 0;
        
        for (const auto& cycle : framCycles) {
            if (cycle.timestamp >= todayStart) {
                // Today's cycle
                uint16_t cycleVolume = cycle.volume_dose * 10; // Convert back from 10ml units
                dailyVolumeML += cycleVolume;
                
                // Also add to RAM cycles for immediate access
                todayCycles.push_back(cycle);
            }
        }
        
        LOG_INFO("Calculated daily volume from FRAM: %dml", dailyVolumeML);
    } else {
        LOG_WARNING("Failed to load cycles from FRAM, starting fresh");
        framDataLoaded = false;
    }
}

void WaterAlgorithm::saveCycleToStorage(const PumpCycle& cycle) {
    if (saveCycleToFRAM(cycle)) {
        LOG_INFO("Cycle saved to FRAM successfully");
        
        // Add to framCycles for immediate access
        framCycles.push_back(cycle);
        
        // Keep framCycles size reasonable
        if (framCycles.size() > 200) {
            framCycles.erase(framCycles.begin());
        }
        
        // Periodic cleanup of old data (once per day)
        if (millis() - lastFRAMCleanup > 86400000UL) { // 24 hours
            clearOldCyclesFromFRAM(14); // Keep 14 days
            lastFRAMCleanup = millis();
            loadCyclesFromStorage(); // Reload after cleanup
            LOG_INFO("FRAM cleanup completed");
        }
    } else {
        LOG_ERROR("Failed to save cycle to FRAM");
    }
}

bool WaterAlgorithm::resetErrorStatistics() {
    bool success = resetErrorStatsInFRAM();
    if (success) {
        LOG_INFO("Error statistics reset requested via web interface");
        
        // Log reset event to VPS
        String timestamp = getCurrentTimestamp();
        logEventToVPS("STATISTICS_RESET", 0, timestamp);
    }
    return success;
}

bool WaterAlgorithm::getErrorStatistics(uint16_t& gap1_sum, uint16_t& gap2_sum, uint16_t& water_sum, uint32_t& last_reset) {
    ErrorStats stats;
    bool success = loadErrorStatsFromFRAM(stats);
    
    if (success) {
        gap1_sum = stats.gap1_fail_sum;
        gap2_sum = stats.gap2_fail_sum;
        water_sum = stats.water_fail_sum;
        last_reset = stats.last_reset_timestamp;
    } else {
        // Return defaults on failure
        gap1_sum = gap2_sum = water_sum = 0;
        last_reset = millis() / 1000;
    }
    
    return success;
}
















