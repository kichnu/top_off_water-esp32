#include "water_algorithm.h"
#include "../core/logging.h"
#include "../hardware/pump_controller.h"
#include "../hardware/water_sensors.h"
#include "../config/config.h" 
#include "../hardware/hardware_pins.h"
#include "algorithm_config.h" 
#include "../hardware/fram_controller.h"  
#include "../network/vps_logger.h"
#include "../hardware/rtc_controller.h" 
#include "../hardware/time_cache.h" 

WaterAlgorithm waterAlgorithm;



WaterAlgorithm::WaterAlgorithm() {
    currentState = STATE_IDLE;
    resetCycle();
    dayStartTime = millis();
    
    // 🆕 MINIMAL INIT - bez odczytu RTC!
    dailyVolumeML = 0;
    strcpy(lastResetDate, "2025-01-01");  // Safe placeholder
    lastResetDate[11] = '\0';
    resetPending = false;
    
    lastError = ERROR_NONE;
    errorSignalActive = false;
    lastSensor1State = false;
    lastSensor2State = false;
    todayCycles.clear();

    framDataLoaded = false;
    lastFRAMCleanup = millis();
    framCycles.clear();

    // ❌ USUNIĘTO całą logikę inicjalizacji daily volume
    // Przeniesiona do initDailyVolume()
    
    loadCyclesFromStorage();

    ErrorStats stats;
    if (loadErrorStatsFromFRAM(stats)) {
        LOG_INFO("Error statistics loaded from FRAM");
    } else {
        LOG_WARNING("Could not load error stats from FRAM");
    }
    
    pinMode(ERROR_SIGNAL_PIN, OUTPUT);
    digitalWrite(ERROR_SIGNAL_PIN, LOW);
    pinMode(RESET_PIN, INPUT_PULLUP);
    
    LOG_INFO("WaterAlgorithm constructor completed (minimal init)");
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
    checkResetButton();
    updateErrorSignal();

       static uint32_t lastRecoveryAttempt = 0;
    static bool recoveryAttempted = false;
    
    if (!globalTimeCache.isValid && !recoveryAttempted && millis() > 10000) {
        // Cache invalid po 10 sekundach - spróbuj recovery
        if (millis() - lastRecoveryAttempt > 30000) {  // Max 1x per 30s
            LOG_WARNING("Time cache invalid - attempting recovery...");
            updateTimeCache();  // Spróbuj ponownie
            lastRecoveryAttempt = millis();
            
            if (globalTimeCache.isValid) {
                LOG_INFO("✅ Time cache recovered successfully!");
                // Ponownie zainicjalizuj daily volume z prawidłową datą
                LOG_INFO("Re-initializing daily volume with valid date...");
                initDailyVolume();
                recoveryAttempted = true;  // Nie próbuj więcej
            } else {
                LOG_ERROR("❌ Time cache recovery failed - will retry in 30s");
            }
        }
    }
    // 🆕 P3: THROTTLING - sprawdzaj datę max 1x/sekundę
    static uint32_t lastDateCheck = 0;
    uint32_t currentTime = getCurrentTimeSeconds();
    
    // Date check throttling
    if (millis() - lastDateCheck >= 1000) {  // Max 1x per second
        lastDateCheck = millis();
        
        // 🆕 P2: Użyj CACHE zamiast getCurrentTimestamp()
        const char* currentDate = getCachedDate();  // ← O(1), BEZ I2C!
        
        // Validate cache
        if (!globalTimeCache.isValid) {
            static uint32_t lastWarning = 0;
            if (millis() - lastWarning > 30000) {
                LOG_ERROR("Time cache invalid - skipping date check");
                LOG_ERROR("Recovery will be attempted automatically");
                lastWarning = millis();
            }
            goto skip_date_check;
        }
        
        // Validate date format
        if (strlen(currentDate) != 10 || currentDate[4] != '-' || currentDate[7] != '-') {
            static uint32_t lastFormatWarning = 0;
            if (millis() - lastFormatWarning > 30000) {
                LOG_ERROR("Invalid date format in cache: '%s'", currentDate);
                lastFormatWarning = millis();
            }
            goto skip_date_check;
        }
        
        // Validate year
        int year = String(currentDate).substring(0, 4).toInt();
        if (year < 2024 || year > 2030) {
            static uint32_t lastYearWarning = 0;
            if (millis() - lastYearWarning > 30000) {
                LOG_ERROR("Year out of range in cache: %d", year);
                lastYearWarning = millis();
            }
            goto skip_date_check;
        }
        
        // Validate lastResetDate
        if (strlen(lastResetDate) != 10 || lastResetDate[4] != '-' || lastResetDate[7] != '-') {
            LOG_ERROR("lastResetDate corrupted: '%s' (len=%d)", 
                      lastResetDate, strlen(lastResetDate));
            LOG_ERROR("Reinitializing with current date");
            
            strncpy(lastResetDate, currentDate, 11);
            lastResetDate[11] = '\0';
            saveDailyVolumeToFRAM(dailyVolumeML, lastResetDate);
            goto skip_date_check;
        }
        
        // Check if date changed
        if (strcmp(currentDate, lastResetDate) != 0) {
            // Parse dates
            int lastYear = String(lastResetDate).substring(0, 4).toInt();
            int lastMonth = String(lastResetDate).substring(5, 7).toInt();
            int lastDay = String(lastResetDate).substring(8, 10).toInt();
            
            int currYear = year;
            int currMonth = String(currentDate).substring(5, 7).toInt();
            int currDay = String(currentDate).substring(8, 10).toInt();
            
            long lastDate = lastYear * 10000 + lastMonth * 100 + lastDay;
            long currDate = currYear * 10000 + currMonth * 100 + currDay;
            
            if (currDate < lastDate) {
                // Date regression - RTC error
                LOG_ERROR("DATE REGRESSION DETECTED - IGNORING!");
                LOG_ERROR("Last: %s (%ld), Current: %s (%ld)", 
                          lastResetDate, lastDate, currentDate, currDate);
                goto skip_date_check;
            }
            
            // Legitimate date change
            LOG_WARNING("===========================================");
            LOG_WARNING("DATE CHANGE DETECTED - RESET TRIGGERED!");
            LOG_WARNING("===========================================");
            LOG_WARNING("Previous: '%s'", lastResetDate);
            LOG_WARNING("Current:  '%s'", currentDate);
            LOG_WARNING("Daily volume BEFORE: %dml", dailyVolumeML);
            LOG_WARNING("===========================================");
            
            if (isPumpActive()) {
                if (!resetPending) {
                    LOG_INFO("Reset delayed - pump active");
                    resetPending = true;
                }
            } else {
                todayCycles.clear();
                strncpy(lastResetDate, currentDate, 11);
                lastResetDate[11] = '\0';
                saveDailyVolumeToFRAM(dailyVolumeML, lastResetDate);
                resetPending = false;
                
                LOG_WARNING("RESET EXECUTED: new date = %s", lastResetDate);
                LOG_WARNING("===========================================");
            }
        }
    }
    
    skip_date_check:
    
    // Execute delayed reset when pump finishes
    if (resetPending && !isPumpActive() && currentState == STATE_IDLE) {
        LOG_INFO("Executing delayed reset (pump finished)");
        
        const char* currentDate = getCachedDate();
        dailyVolumeML = 0;
        todayCycles.clear();
        strncpy(lastResetDate, currentDate, 11);
        lastResetDate[11] = '\0';
        saveDailyVolumeToFRAM(dailyVolumeML, lastResetDate);
        resetPending = false;
        
        LOG_INFO("Delayed reset complete: 0ml (date: %s)", lastResetDate);
    }
    
    uint32_t stateElapsed = currentTime - stateStartTime;

    switch (currentState) {
        case STATE_IDLE:
            break;

        case STATE_TRYB_1_WAIT:
            if (stateElapsed >= TIME_GAP_1_MAX) {
                currentCycle.time_gap_1 = TIME_GAP_1_MAX;
                LOG_INFO("TRYB_1: TIME_GAP_1 timeout, using max: %ds", TIME_GAP_1_MAX);
                
                if (sensor_time_match_function(currentCycle.time_gap_1, THRESHOLD_1)) {
                    currentCycle.sensor_results |= PumpCycle::RESULT_GAP1_FAIL;
                }
                
                currentState = STATE_TRYB_1_DELAY;
                stateStartTime = currentTime;
                LOG_INFO("TRYB_1: Starting TIME_TO_PUMP delay (%ds)", TIME_TO_PUMP);
            }
            break;
                    
        case STATE_TRYB_1_DELAY:
            if (currentTime - triggerStartTime >= TIME_TO_PUMP) {
                uint16_t pumpWorkTime = calculatePumpWorkTime(currentPumpSettings.volumePerSecond);
                
                if (!validatePumpWorkTime(pumpWorkTime)) {
                    LOG_ERROR("PUMP_WORK_TIME (%ds) exceeds WATER_TRIGGER_MAX_TIME (%ds)", 
                            pumpWorkTime, WATER_TRIGGER_MAX_TIME);
                    pumpWorkTime = WATER_TRIGGER_MAX_TIME - 10;
                }
                
                LOG_INFO("TRYB_2: Starting pump attempt %d/%d for %ds", 
                        pumpAttempts + 1, PUMP_MAX_ATTEMPTS, pumpWorkTime);
                
                pumpStartTime = currentTime;
                pumpAttempts++;
                
                triggerPump(pumpWorkTime, "AUTO_PUMP");
                
                currentCycle.pump_duration = pumpWorkTime;
                currentState = STATE_TRYB_2_PUMP;
                stateStartTime = currentTime;
            }
            break;
               
        case STATE_TRYB_2_PUMP:
            if (!isPumpActive()) {
                LOG_INFO("TRYB_2: Pump finished, checking sensors");
                currentState = STATE_TRYB_2_VERIFY;
                stateStartTime = currentTime;
            }
            break;

        case STATE_TRYB_2_VERIFY: {
            static uint32_t lastStatusLog = 0;
            if (currentTime - lastStatusLog >= 5) {
                uint32_t timeSincePumpStart = currentTime - pumpStartTime;
                // LOG_INFO("TRYB_2_VERIFY: Waiting for sensors... %ds/%ds (attempt %d/%d)", 
                //         timeSincePumpStart, WATER_TRIGGER_MAX_TIME, pumpAttempts, PUMP_MAX_ATTEMPTS);###################################################################
                lastStatusLog = currentTime;
            }

            bool sensorsOK = !readWaterSensor1() && !readWaterSensor2();
            
            if (sensorsOK) {
                calculateWaterTrigger();
                LOG_INFO("TRYB_2: Sensors deactivated, water_trigger_time: %ds", 
                        currentCycle.water_trigger_time);
                
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
                uint32_t timeSincePumpStart = currentTime - pumpStartTime;
                
                if (timeSincePumpStart >= WATER_TRIGGER_MAX_TIME) {
                    currentCycle.water_trigger_time = WATER_TRIGGER_MAX_TIME;

                    if (WATER_TRIGGER_MAX_TIME >= THRESHOLD_WATER) {
                        waterFailDetected = true;
                        LOG_INFO("WATER fail detected in attempt %d/%d", pumpAttempts, PUMP_MAX_ATTEMPTS);
                    }
                    
                    LOG_WARNING("TRYB_2: Timeout after %ds (limit: %ds), attempt %d/%d", 
                            timeSincePumpStart, WATER_TRIGGER_MAX_TIME, 
                            pumpAttempts, PUMP_MAX_ATTEMPTS);
                    
                    if (pumpAttempts < PUMP_MAX_ATTEMPTS) {
                        LOG_WARNING("TRYB_2: Retrying pump attempt %d/%d", 
                                pumpAttempts + 1, PUMP_MAX_ATTEMPTS);
                        
                        currentState = STATE_TRYB_1_DELAY;
                        stateStartTime = currentTime - TIME_TO_PUMP;
                    } else {
                        LOG_ERROR("TRYB_2: All %d pump attempts failed!", PUMP_MAX_ATTEMPTS);
                        currentCycle.error_code = ERROR_PUMP_FAILURE;
                                        
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
            static bool debugOnce = true;
            if (debugOnce) {
                LOG_INFO("DEBUG GAP2: s1Release=%ds, s2Release=%ds, waiting=%d", 
                        sensor1ReleaseTime, sensor2ReleaseTime, waitingForSecondSensor);
                debugOnce = false;
            }
            
            if (sensor1ReleaseTime && sensor2ReleaseTime && waitingForSecondSensor) {
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
        
            if (!cycleLogged) {
                logCycleComplete();
                cycleLogged = true;
                
                if (dailyVolumeML > FILL_WATER_MAX) {
                    LOG_ERROR("Daily limit exceeded! %dml > %dml", dailyVolumeML, FILL_WATER_MAX);
                    currentCycle.error_code = ERROR_DAILY_LIMIT;
                    startErrorSignal(ERROR_DAILY_LIMIT);
                    currentState = STATE_ERROR;
                    break;
                }
            }
            
            if (stateElapsed >= LOGGING_TIME) { 
                LOG_INFO("Cycle complete, returning to IDLE######################################################");
                currentState = STATE_IDLE;
                resetCycle();
            }
            break;
    }
}

void WaterAlgorithm::initDailyVolume() {
    LOG_INFO("====================================");
    LOG_INFO("INITIALIZING DAILY VOLUME");
    LOG_INFO("====================================");
    
    // ✅ Teraz RTC i cache są już zainicjalizowane!
    const char* currentDate = getCachedDate();  // ← O(1), bez I2C!
    
    LOG_INFO("Current date from cache: %s", currentDate);
    LOG_INFO("Cache valid: %s", globalTimeCache.isValid ? "YES" : "NO");
    LOG_INFO("RTC info: %s", getRTCInfo().c_str());
    
    // 🆕 CRITICAL: Jeśli cache invalid, NIE zapisuj do FRAM!
    if (!globalTimeCache.isValid) {
        LOG_ERROR("====================================");
        LOG_ERROR("⚠️ Time cache INVALID at initDailyVolume!");
        LOG_ERROR("====================================");
        LOG_ERROR("Cannot initialize daily volume without valid RTC");
        LOG_ERROR("Using temporary fallback - will retry automatically");
        LOG_ERROR("Daily volume tracking SUSPENDED until RTC ready");
        LOG_ERROR("====================================");
        
        // Użyj fallback ALE NIE ZAPISUJ DO FRAM
        dailyVolumeML = 0;
        strcpy(lastResetDate, "2025-01-01");
        
        // ❌ NIE ZAPISUJ: saveDailyVolumeToFRAM(dailyVolumeML, lastResetDate);
        
        LOG_WARNING("Fallback date: %s (NOT saved to FRAM)", lastResetDate);
        LOG_WARNING("System will auto-recover when RTC becomes available");
        return;
    }
    
    // Cache is VALID - proceed normally
    LOG_INFO("✅ Cache is valid, proceeding with initialization");
    
    // Validate date format
    if (strlen(currentDate) != 10 || currentDate[4] != '-' || currentDate[7] != '-') {
        LOG_ERROR("Invalid date format in cache: '%s'", currentDate);
        dailyVolumeML = 0;
        strcpy(lastResetDate, currentDate);
        // Zapisz tylko jeśli format jest zły ale cache valid
        saveDailyVolumeToFRAM(dailyVolumeML, lastResetDate);
        return;
    }
    
    // Validate year
    int year = String(currentDate).substring(0, 4).toInt();
    if (year < 2024 || year > 2030) {
        LOG_ERROR("Invalid year from cache: %d", year);
        dailyVolumeML = 0;
        strcpy(lastResetDate, currentDate);
        saveDailyVolumeToFRAM(dailyVolumeML, lastResetDate);
        return;
    }
    
    // Load from FRAM
    char loadedDate[12];
    uint16_t loadedVolume = 0;
    
    if (loadDailyVolumeFromFRAM(loadedVolume, loadedDate)) {
        LOG_INFO("FRAM data: %dml, date='%s'", loadedVolume, loadedDate);
        LOG_INFO("Current date: '%s'", currentDate);
        
        // Validate FRAM year
        int framYear = String(loadedDate).substring(0, 4).toInt();
        if (framYear < 2024 || framYear > 2030) {
            LOG_WARNING("Invalid FRAM year: %d - resetting", framYear);
            dailyVolumeML = 0;
            strncpy(lastResetDate, currentDate, 11);
            lastResetDate[11] = '\0';
            saveDailyVolumeToFRAM(dailyVolumeML, lastResetDate);
        } else if (strcmp(currentDate, loadedDate) == 0) {
            // Same day - restore volume
            dailyVolumeML = loadedVolume;
            strncpy(lastResetDate, loadedDate, 11);
            lastResetDate[11] = '\0';
            LOG_INFO("✅ Same day - restored: %dml", dailyVolumeML);
        } else {
            // Different day - reset
            LOG_INFO("🔄 Date changed from '%s' to '%s'", loadedDate, currentDate);
            dailyVolumeML = 0;
            strncpy(lastResetDate, currentDate, 11);
            lastResetDate[11] = '\0';
            saveDailyVolumeToFRAM(dailyVolumeML, lastResetDate);
            LOG_INFO("✅ New day - reset to 0ml");
        }
    } else {
        // No valid data in FRAM
        LOG_INFO("No valid FRAM data - initializing");
        dailyVolumeML = 0;
        strncpy(lastResetDate, currentDate, 11);
        lastResetDate[11] = '\0';
        saveDailyVolumeToFRAM(dailyVolumeML, lastResetDate);
        LOG_INFO("✅ Initialized to 0ml");
    }
    
    LOG_INFO("INIT COMPLETE:");
    LOG_INFO("  dailyVolumeML: %dml", dailyVolumeML);
    LOG_INFO("  lastResetDate: '%s'", lastResetDate);
    LOG_INFO("====================================");
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
        
        if (triggerStartTime > 0) {
            LOG_INFO("Attempting to reconstruct TIME_GAP_1 from available data");
        }
    }

    // Calculate volume based on actual pump duration
    uint16_t actualVolumeML = (uint16_t)(currentCycle.pump_duration * currentPumpSettings.volumePerSecond);
    currentCycle.volume_dose = actualVolumeML;
    currentCycle.pump_attempts = pumpAttempts;

    // SET final fail flag based on any failure
    if (waterFailDetected) {
        currentCycle.sensor_results |= PumpCycle::RESULT_WATER_FAIL;
        LOG_INFO("Final WATER fail flag set due to timeout in any attempt");
    }
    
    // Add to daily volume (use actual volume, not fixed SINGLE_DOSE_VOLUME)
    dailyVolumeML += actualVolumeML;

    if (!saveDailyVolumeToFRAM(dailyVolumeML, lastResetDate)) {
        LOG_WARNING("⚠️ Failed to save daily volume to FRAM");
    }
    
    // Store in today's cycles (RAM)
    todayCycles.push_back(currentCycle);
    
    // Keep only last 50 cycles in RAM (FRAM will store more)
    if (todayCycles.size() > 50) {
        todayCycles.erase(todayCycles.begin());
    }
    
    // Save cycle to FRAM (for debugging and history)
    saveCycleToStorage(currentCycle);
    
    // *** Update error statistics in FRAM ***
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
    
    // ✅ FIX KOMPILACJI: Użyj getCachedUnixTime() zamiast getCurrentTimestamp()
    uint32_t unixTime = getCachedUnixTime();  // ← JEDEN odczyt z cache, BEZ I2C!
    
    if (logCycleToVPS(currentCycle, unixTime)) {  // ← Przekaż uint32_t
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

    if (dailyVolumeML >= FILL_WATER_MAX) {
        LOG_ERROR("❌ Manual pump blocked: Daily limit reached (%dml / %dml)", 
                  dailyVolumeML, FILL_WATER_MAX);
        return false;  // Block manual pump when limit reached
    }

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
        digitalWrite(ERROR_SIGNAL_PIN, errorPulseState ? HIGH : LOW);
    }
}

void WaterAlgorithm::resetFromError() {
    lastError = ERROR_NONE;
    errorSignalActive = false;
    pinMode(ERROR_SIGNAL_PIN, OUTPUT);
    digitalWrite(ERROR_SIGNAL_PIN, LOW);
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


        LOG_INFO("Daily volume already loaded from FRAM: %dml", dailyVolumeML);

                // Just load today's cycles for display
        uint32_t todayStart = (millis() / 1000) - (millis() / 1000) % 86400;
        
        for (const auto& cycle : framCycles) {
            if (cycle.timestamp >= todayStart) {
                todayCycles.push_back(cycle);
            }
        }
        
        LOG_INFO("Loaded %d cycles from today", todayCycles.size());
        
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
        
        // ✅ PRZYWRÓĆ VPS logging z short timeout (3 seconds max)
        String timestamp = getCurrentTimestamp();
        bool vpsSuccess = logEventToVPS("STATISTICS_RESET", 0, timestamp);
        
        if (vpsSuccess) {
            LOG_INFO("✅ Statistics reset + VPS logging: SUCCESS");
        } else {
            LOG_WARNING("⚠️ Statistics reset: SUCCESS, VPS logging: FAILED (non-critical)");
        }
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

void WaterAlgorithm::addManualVolume(uint16_t volumeML) {
    // 🆕 NEW: Add manual pump volume to daily total
    dailyVolumeML += volumeML;
    
    // Save to FRAM
    if (!saveDailyVolumeToFRAM(dailyVolumeML, lastResetDate)) {
        LOG_WARNING("⚠️ Failed to save daily volume to FRAM after manual pump");
    }
    
    LOG_INFO("✅ Manual volume added: +%dml → Total: %dml / %dml", 
             volumeML, dailyVolumeML, FILL_WATER_MAX);
    
    // 🆕 NEW: Check if limit exceeded after manual pump
    if (dailyVolumeML >= FILL_WATER_MAX) {
        LOG_ERROR("❌ Daily limit reached after manual pump: %dml / %dml", 
                  dailyVolumeML, FILL_WATER_MAX);
        
        // Trigger error state
        currentCycle.error_code = ERROR_DAILY_LIMIT;
        startErrorSignal(ERROR_DAILY_LIMIT);
        currentState = STATE_ERROR;
        
        LOG_ERROR("System entering ERROR state - press reset button to clear");
    }
}


// ============================================
// 🆕 CHECK RESET BUTTON (Pin 8 - Active LOW)
// Funkcja: TYLKO reset z błędu (resetFromError)
// ============================================

void WaterAlgorithm::checkResetButton() {
    static bool lastButtonState = HIGH;           // Poprzedni stan (HIGH = not pressed)
    static uint32_t lastDebounceTime = 0;         // Czas ostatniej zmiany
    static bool buttonPressed = false;            // Czy przycisk jest wciśnięty
    
    const uint32_t DEBOUNCE_DELAY = 50;           // 50ms debouncing
    
    // Read current button state (INPUT_PULLUP, więc LOW = pressed)
    bool currentButtonState = digitalRead(RESET_PIN);
    
    // Sprawdź czy stan się zmienił
    if (currentButtonState != lastButtonState) {
        lastDebounceTime = millis();
    }
    
    // Debouncing - sprawdź czy stan jest stabilny przez DEBOUNCE_DELAY
    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
        
        // Przycisk został naciśnięty (HIGH → LOW)
        if (currentButtonState == LOW && !buttonPressed) {
            buttonPressed = true;
            LOG_INFO("🔘 Reset button pressed");
        }
        
        // Przycisk został zwolniony (LOW → HIGH)
        else if (currentButtonState == HIGH && buttonPressed) {
            buttonPressed = false;
            LOG_INFO("🔘 Reset button released");
            
            // Sprawdź czy system jest w stanie błędu
            if (currentState == STATE_ERROR) {
                LOG_INFO("====================================");
                LOG_INFO("✅ RESET FROM ERROR STATE");
                LOG_INFO("====================================");
                LOG_INFO("Previous error: %s", 
                         lastError == ERROR_DAILY_LIMIT ? "ERR1 (Daily Limit)" :
                         lastError == ERROR_PUMP_FAILURE ? "ERR2 (Pump Failure)" : 
                         "ERR0 (Both)");
                
                // Wywołaj reset z błędu
                resetFromError();
                
                LOG_INFO("System state: %s", getStateString());
                LOG_INFO("Error signal: CLEARED");
                LOG_INFO("====================================");
                
                // Visual feedback - krótkie mignięcie LED (potwierdzenie)
                digitalWrite(ERROR_SIGNAL_PIN, HIGH);
                delay(100);
                digitalWrite(ERROR_SIGNAL_PIN, LOW);
                delay(100);
                digitalWrite(ERROR_SIGNAL_PIN, HIGH);
                delay(100);
                digitalWrite(ERROR_SIGNAL_PIN, LOW);
                
            } else {
                LOG_INFO("ℹ️ System not in error state");
                LOG_INFO("Current state: %s", getStateString());
                LOG_INFO("Reset button ignored (no error to clear)");
            }
        }
    }
    
    lastButtonState = currentButtonState;
}

















