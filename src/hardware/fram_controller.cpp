#include "fram_controller.h"
#include "../core/logging.h"
#include "../hardware/hardware_pins.h"
#include <Wire.h>
#include <Adafruit_FRAM_I2C.h>
#include "../algorithm/algorithm_config.h"
#include "rtc_controller.h"

#include "../crypto/fram_encryption.h"





Adafruit_FRAM_I2C fram = Adafruit_FRAM_I2C();
bool framInitialized = false;

// Calculate simple checksum
uint16_t calculateChecksum(uint8_t* data, size_t len) {
    uint16_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

bool initFRAM() {
    LOG_INFO("Initializing FRAM at address 0x50...");
    
    // FRAM uses same I2C as RTC - already initialized in rtc_controller
    // Just begin FRAM communication
    if (!fram.begin(0x50)) {
        LOG_ERROR("FRAM not found at address 0x50!");
        framInitialized = false;
        return false;
    }
    
    framInitialized = true;
    LOG_INFO("FRAM initialized successfully (256Kbit = 32KB)");
    
    // Verify FRAM integrity
    if (!verifyFRAM()) {
        LOG_WARNING("FRAM not initialized or corrupted, initializing...");
        
        // Write magic number
        uint32_t magic = FRAM_MAGIC_NUMBER;
        fram.write(FRAM_ADDR_MAGIC, (uint8_t*)&magic, 4);
        
        // Write version
        uint16_t version = FRAM_DATA_VERSION;
        fram.write(FRAM_ADDR_VERSION, (uint8_t*)&version, 2);
        
        // Write default volume
        float defaultVolume = 1.0;
        fram.write(FRAM_ADDR_VOLUME_ML, (uint8_t*)&defaultVolume, 4);
        
        // Calculate and write checksum
        uint8_t buffer[4];
        fram.read(FRAM_ADDR_VOLUME_ML, buffer, 4);
        uint16_t checksum = calculateChecksum(buffer, 4);
        fram.write(FRAM_ADDR_CHECKSUM, (uint8_t*)&checksum, 2);
        
        LOG_INFO("FRAM initialized with defaults");
    }
    
    return true;
}
bool verifyFRAM() {
    if (!framInitialized) return false;
    
    // Check magic number
    uint32_t magic = 0;
    fram.read(FRAM_ADDR_MAGIC, (uint8_t*)&magic, 4);
    
    if (magic != FRAM_MAGIC_NUMBER) {
        LOG_WARNING("FRAM magic number mismatch: 0x%08X", magic);
        return false;
    }
    
    // Check version
    uint16_t version = 0;
    fram.read(FRAM_ADDR_VERSION, (uint8_t*)&version, 2);
    
    if (version != FRAM_DATA_VERSION) {
        // Auto-upgrade from version 1 to 2
        if (version == 0x0001) {
            LOG_INFO("Auto-upgrading FRAM from v1 to v2 (unified layout)");
                
            // Initialize error stats with defaults
            ErrorStats defaultStats;
            defaultStats.gap1_fail_sum = 0;
            defaultStats.gap2_fail_sum = 0;
            defaultStats.water_fail_sum = 0;
            defaultStats.last_reset_timestamp = millis() / 1000;
                
            saveErrorStatsToFRAM(defaultStats);
                
            // Update version
            uint16_t newVersion = FRAM_DATA_VERSION;
            fram.write(FRAM_ADDR_VERSION, (uint8_t*)&newVersion, 2);
                
            LOG_INFO("FRAM upgraded to version %d", FRAM_DATA_VERSION);
            return true;
        }

        return false;
    }
    
    // Verify ESP32 data checksum
    uint8_t buffer[4];
    fram.read(FRAM_ADDR_VOLUME_ML, buffer, 4);
    uint16_t calculatedChecksum = calculateChecksum(buffer, 4);
    
    uint16_t storedChecksum = 0;
    fram.read(FRAM_ADDR_CHECKSUM, (uint8_t*)&storedChecksum, 2);
    
    if (calculatedChecksum != storedChecksum) {
        LOG_WARNING("ESP32 data checksum mismatch: stored=%d, calculated=%d", 
                    storedChecksum, calculatedChecksum);
        return false;
    }
    
    LOG_INFO("FRAM verification successful (unified layout v%d)", FRAM_DATA_VERSION);
    return true;
}

bool loadVolumeFromFRAM(float& volume) {
    if (!framInitialized) {
        LOG_ERROR("FRAM not initialized, cannot load volume");
        return false;
    }
    
    // Read volume value
    fram.read(FRAM_ADDR_VOLUME_ML, (uint8_t*)&volume, 4);
    
    // Verify checksum
    uint8_t buffer[4];
    memcpy(buffer, &volume, 4);
    uint16_t calculatedChecksum = calculateChecksum(buffer, 4);
    
    uint16_t storedChecksum = 0;
    fram.read(FRAM_ADDR_CHECKSUM, (uint8_t*)&storedChecksum, 2);
    
    if (calculatedChecksum != storedChecksum) {
        LOG_ERROR("FRAM checksum error when loading volume!");
        return false;
    }
    
    // Sanity check the value
    if (volume < 0.1 || volume > 20.0) {
        LOG_WARNING("FRAM volume out of range: %.2f, using default", volume);
        volume = 1.0;
        return false;
    }
    
    LOG_INFO("Loaded volume from FRAM: %.1f ml/s", volume);
    return true;
}

bool saveVolumeToFRAM(float volume) {
    if (!framInitialized) {
        LOG_ERROR("FRAM not initialized, cannot save volume");
        return false;
    }
    
    // Sanity check
    if (volume < 0.1 || volume > 20.0) {
        LOG_ERROR("Invalid volume value: %.2f", volume);
        return false;
    }
    
    // Write volume
    fram.write(FRAM_ADDR_VOLUME_ML, (uint8_t*)&volume, 4);
    
    // Calculate and write checksum
    uint8_t buffer[4];
    memcpy(buffer, &volume, 4);
    uint16_t checksum = calculateChecksum(buffer, 4);
    fram.write(FRAM_ADDR_CHECKSUM, (uint8_t*)&checksum, 2);
    
    // Verify write by reading back
    float readBack = 0;
    fram.read(FRAM_ADDR_VOLUME_ML, (uint8_t*)&readBack, 4);
    
    if (abs(readBack - volume) > 0.01) {
        LOG_ERROR("FRAM write verification failed! Wrote: %.2f, Read: %.2f", 
                  volume, readBack);
        return false;
    }
    
    LOG_INFO("SUCCESS: Saved volume to FRAM: %.1f ml/s", volume);
    return true;
}

void testFRAM() {
    if (!framInitialized) {
        LOG_ERROR("FRAM not initialized for testing");
        return;
    }
    
    LOG_INFO("=== FRAM Test Start ===");
    
    // Test 1: Write/Read test pattern
    uint8_t testData[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                            0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t readData[16];
    
    fram.write(0x1000, testData, 16);  // Test address at 4KB offset
    fram.read(0x1000, readData, 16);
    
    bool testPassed = true;
    for (int i = 0; i < 16; i++) {
        if (testData[i] != readData[i]) {
            LOG_ERROR("FRAM test failed at byte %d: wrote 0x%02X, read 0x%02X", 
                     i, testData[i], readData[i]);
            testPassed = false;
        }
    }
    
    if (testPassed) {
        LOG_INFO("FRAM read/write test PASSED");
    }
    
    // Test 2: Volume persistence
    float testVolume = 3.14;
    if (saveVolumeToFRAM(testVolume)) {
        float loadedVolume = 0;
        if (loadVolumeFromFRAM(loadedVolume)) {
            if (abs(loadedVolume - testVolume) < 0.01) {
                LOG_INFO("FRAM volume persistence test PASSED");
            } else {
                LOG_ERROR("FRAM volume mismatch: saved %.2f, loaded %.2f", 
                         testVolume, loadedVolume);
            }
        }
    }
    
    // Restore default volume
    saveVolumeToFRAM(1.0);
    
    LOG_INFO("=== FRAM Test Complete ===");
}

bool saveCycleToFRAM(const PumpCycle& cycle) {
    if (!framInitialized) {
        LOG_ERROR("FRAM not initialized for cycle save");
        return false;
    }
    
    // Read current cycle count and index
    uint16_t cycleCount = 0;
    uint16_t writeIndex = 0;
    
    fram.read(FRAM_ADDR_CYCLE_COUNT, (uint8_t*)&cycleCount, 2);
    fram.read(FRAM_ADDR_CYCLE_INDEX, (uint8_t*)&writeIndex, 2);
    
    // Calculate write address
    uint16_t writeAddr = FRAM_ADDR_CYCLE_DATA + (writeIndex * FRAM_CYCLE_SIZE);
    
    // Write cycle data
    fram.write(writeAddr, (uint8_t*)&cycle, sizeof(PumpCycle));
    
    // Update circular buffer index
    writeIndex = (writeIndex + 1) % FRAM_MAX_CYCLES;
    fram.write(FRAM_ADDR_CYCLE_INDEX, (uint8_t*)&writeIndex, 2);
    
    // Update count (max FRAM_MAX_CYCLES)
    if (cycleCount < FRAM_MAX_CYCLES) {
        cycleCount++;
        fram.write(FRAM_ADDR_CYCLE_COUNT, (uint8_t*)&cycleCount, 2);
    }
    
    LOG_INFO("Cycle saved to FRAM at index %d (total: %d)", 
             (writeIndex - 1 + FRAM_MAX_CYCLES) % FRAM_MAX_CYCLES, cycleCount);
    
    return true;
}

bool loadCyclesFromFRAM(std::vector<PumpCycle>& cycles, uint16_t maxCount) {
    if (!framInitialized) {
        LOG_ERROR("FRAM not initialized for cycle load");
        return false;
    }
    
    cycles.clear();
    
    // Read cycle count and current index
    uint16_t cycleCount = 0;
    uint16_t writeIndex = 0;
    
    fram.read(FRAM_ADDR_CYCLE_COUNT, (uint8_t*)&cycleCount, 2);
    fram.read(FRAM_ADDR_CYCLE_INDEX, (uint8_t*)&writeIndex, 2);
    
    if (cycleCount == 0) {
        LOG_INFO("No cycles found in FRAM");
        return true;
    }
    
    // Limit count to requested max
    uint16_t loadCount = (cycleCount > maxCount) ? maxCount : cycleCount;
    
    // Calculate start index (load most recent cycles)
    uint16_t startIndex;
    if (cycleCount < FRAM_MAX_CYCLES) {
        // Buffer not full, start from beginning
        startIndex = (cycleCount >= loadCount) ? (cycleCount - loadCount) : 0;
    } else {
        // Buffer full, start from writeIndex - loadCount
        startIndex = (writeIndex + FRAM_MAX_CYCLES - loadCount) % FRAM_MAX_CYCLES;
    }
    
    // Load cycles
    for (uint16_t i = 0; i < loadCount; i++) {
        uint16_t readIndex = (startIndex + i) % FRAM_MAX_CYCLES;
        uint16_t readAddr = FRAM_ADDR_CYCLE_DATA + (readIndex * FRAM_CYCLE_SIZE);
        
        PumpCycle cycle;
        fram.read(readAddr, (uint8_t*)&cycle, sizeof(PumpCycle));
        
        // Basic validation
        if (cycle.timestamp > 0 && cycle.timestamp < 0xFFFFFFFF) {
            cycles.push_back(cycle);
        }
    }
    
    LOG_INFO("Loaded %d cycles from FRAM (requested: %d, available: %d)", 
             cycles.size(), loadCount, cycleCount);
    
    return true;
}

uint16_t getCycleCountFromFRAM() {
    if (!framInitialized) return 0;
    
    uint16_t cycleCount = 0;
    fram.read(FRAM_ADDR_CYCLE_COUNT, (uint8_t*)&cycleCount, 2);
    return cycleCount;
}

bool clearOldCyclesFromFRAM(uint32_t olderThanDays) {
    if (!framInitialized) return false;
    
    uint32_t cutoffTime = (millis() / 1000) - (olderThanDays * 24 * 3600);
    
    // For now, simple implementation - clear all if any is too old
    // More sophisticated implementation would compact the data
    
    std::vector<PumpCycle> allCycles;
    if (!loadCyclesFromFRAM(allCycles)) return false;
    
    // Filter recent cycles
    std::vector<PumpCycle> recentCycles;
    for (const auto& cycle : allCycles) {
        if (cycle.timestamp >= cutoffTime) {
            recentCycles.push_back(cycle);
        }
    }
    
    if (recentCycles.size() == allCycles.size()) {
        LOG_INFO("No old cycles to clear");
        return true;
    }
    
    // Reset FRAM cycle data
    uint16_t zero = 0;
    fram.write(FRAM_ADDR_CYCLE_COUNT, (uint8_t*)&zero, 2);
    fram.write(FRAM_ADDR_CYCLE_INDEX, (uint8_t*)&zero, 2);
    
    // Save recent cycles back
    for (const auto& cycle : recentCycles) {
        saveCycleToFRAM(cycle);
    }
    
    LOG_INFO("Cleared old cycles, kept %d recent cycles", recentCycles.size());
    return true;
}

// ===============================
// ERROR STATISTICS MANAGEMENT
// ===============================

uint16_t calculateStatsChecksum(const ErrorStats& stats) {
    uint16_t sum = 0;
    sum += stats.gap1_fail_sum;
    sum += stats.gap2_fail_sum;
    sum += stats.water_fail_sum;
    sum += (stats.last_reset_timestamp >> 16) & 0xFFFF;
    sum += stats.last_reset_timestamp & 0xFFFF;
    return sum;
}

bool loadErrorStatsFromFRAM(ErrorStats& stats) {
    if (!framInitialized) {
        LOG_ERROR("FRAM not initialized for stats load");
        return false;
    }
    
    // Read stats data
    fram.read(FRAM_ADDR_GAP1_SUM, (uint8_t*)&stats.gap1_fail_sum, 2);
    fram.read(FRAM_ADDR_GAP2_SUM, (uint8_t*)&stats.gap2_fail_sum, 2);
    fram.read(FRAM_ADDR_WATER_SUM, (uint8_t*)&stats.water_fail_sum, 2);
    fram.read(FRAM_ADDR_LAST_RESET, (uint8_t*)&stats.last_reset_timestamp, 4);
    
    // Verify checksum
    uint16_t calculatedChecksum = calculateStatsChecksum(stats);
    uint16_t storedChecksum = 0;
    fram.read(FRAM_ADDR_STATS_CHKSUM, (uint8_t*)&storedChecksum, 2);
    
    if (calculatedChecksum != storedChecksum) {
        LOG_WARNING("Stats checksum mismatch: stored=%d, calculated=%d", 
                    storedChecksum, calculatedChecksum);
        
        // Initialize with defaults on checksum error
        stats.gap1_fail_sum = 0;
        stats.gap2_fail_sum = 0;
        stats.water_fail_sum = 0;
        stats.last_reset_timestamp = getUnixTimestamp(); // Current time as reset time
        
        saveErrorStatsToFRAM(stats); // Save defaults
        return false;
    }
    
    LOG_INFO("Loaded error stats: GAP1=%d, GAP2=%d, WATER=%d", 
             stats.gap1_fail_sum, stats.gap2_fail_sum, stats.water_fail_sum);
    
    return true;
}

bool saveErrorStatsToFRAM(const ErrorStats& stats) {
    if (!framInitialized) {
        LOG_ERROR("FRAM not initialized for stats save");
        return false;
    }
    
    // Write stats data
    fram.write(FRAM_ADDR_GAP1_SUM, (uint8_t*)&stats.gap1_fail_sum, 2);
    fram.write(FRAM_ADDR_GAP2_SUM, (uint8_t*)&stats.gap2_fail_sum, 2);
    fram.write(FRAM_ADDR_WATER_SUM, (uint8_t*)&stats.water_fail_sum, 2);
    fram.write(FRAM_ADDR_LAST_RESET, (uint8_t*)&stats.last_reset_timestamp, 4);
    
    // Calculate and write checksum
    uint16_t checksum = calculateStatsChecksum(stats);
    fram.write(FRAM_ADDR_STATS_CHKSUM, (uint8_t*)&checksum, 2);
    
    // Verify write by reading back
    ErrorStats readBack;
    fram.read(FRAM_ADDR_GAP1_SUM, (uint8_t*)&readBack.gap1_fail_sum, 2);
    fram.read(FRAM_ADDR_GAP2_SUM, (uint8_t*)&readBack.gap2_fail_sum, 2);
    fram.read(FRAM_ADDR_WATER_SUM, (uint8_t*)&readBack.water_fail_sum, 2);
    
    if (readBack.gap1_fail_sum != stats.gap1_fail_sum ||
        readBack.gap2_fail_sum != stats.gap2_fail_sum ||
        readBack.water_fail_sum != stats.water_fail_sum) {
        
        LOG_ERROR("FRAM stats write verification failed!");
        return false;
    }
    
    LOG_INFO("Saved error stats to FRAM: GAP1=%d, GAP2=%d, WATER=%d", 
             stats.gap1_fail_sum, stats.gap2_fail_sum, stats.water_fail_sum);
    
    return true;
}

bool resetErrorStatsInFRAM() {
    if (!framInitialized) {
        LOG_ERROR("FRAM not initialized for stats reset");
        return false;
    }
    
    ErrorStats stats;
    stats.gap1_fail_sum = 0;
    stats.gap2_fail_sum = 0;
    stats.water_fail_sum = 0;
    stats.last_reset_timestamp = getUnixTimestamp(); // Current time
    
    bool success = saveErrorStatsToFRAM(stats);
    if (success) {
        LOG_INFO("Error statistics reset to zero");
    }
    
    return success;
}

bool incrementErrorStats(uint8_t gap1_increment, uint8_t gap2_increment, uint8_t water_increment) {
    if (!framInitialized) {
        LOG_ERROR("FRAM not initialized for stats increment");
        return false;
    }
    
    // Load current stats
    ErrorStats stats;
    if (!loadErrorStatsFromFRAM(stats)) {
        // If load fails, start with defaults
        stats.gap1_fail_sum = 0;
        stats.gap2_fail_sum = 0;
        stats.water_fail_sum = 0;
        stats.last_reset_timestamp = millis() / 1000;
    }
    
    // Increment with overflow protection
    if (stats.gap1_fail_sum + gap1_increment <= 65535) {
        stats.gap1_fail_sum += gap1_increment;
    } else {
        stats.gap1_fail_sum = 65535; // Cap at max uint16_t
        LOG_WARNING("GAP1 sum capped at 65535");
    }
    
    if (stats.gap2_fail_sum + gap2_increment <= 65535) {
        stats.gap2_fail_sum += gap2_increment;
    } else {
        stats.gap2_fail_sum = 65535;
        LOG_WARNING("GAP2 sum capped at 65535");
    }
    
    if (stats.water_fail_sum + water_increment <= 65535) {
        stats.water_fail_sum += water_increment;
    } else {
        stats.water_fail_sum = 65535;
        LOG_WARNING("WATER sum capped at 65535");
    }
    
    // Save updated stats
    bool success = saveErrorStatsToFRAM(stats);
    
    if (success && (gap1_increment || gap2_increment || water_increment)) {
        LOG_INFO("Incremented stats: GAP1+%d=%d, GAP2+%d=%d, WATER+%d=%d", 
                gap1_increment, stats.gap1_fail_sum,
                gap2_increment, stats.gap2_fail_sum, 
                water_increment, stats.water_fail_sum);
    }
    
    return success;
}

// ===============================
// FRAM CREDENTIALS SECTION  
// (Shared with FRAM Programmer)
// ===============================
bool readCredentialsFromFRAM(FRAMCredentials& creds) {
    if (!framInitialized) {
        LOG_ERROR("FRAM not initialized for credentials read");
        return false;
    }
    
    // Read credentials structure from FRAM
    fram.read(FRAM_CREDENTIALS_ADDR, (uint8_t*)&creds, sizeof(FRAMCredentials));
    
    LOG_INFO("Read credentials from FRAM at address 0x%04X", FRAM_CREDENTIALS_ADDR);
    return true;
}

bool writeCredentialsToFRAM(const FRAMCredentials& creds) {
    if (!framInitialized) {
        LOG_ERROR("FRAM not initialized for credentials write");
        return false;
    }
    
    // Write credentials structure to FRAM
    fram.write(FRAM_CREDENTIALS_ADDR, (uint8_t*)&creds, sizeof(FRAMCredentials));
    
    // Verify write by reading back
    FRAMCredentials verify_creds;
    fram.read(FRAM_CREDENTIALS_ADDR, (uint8_t*)&verify_creds, sizeof(FRAMCredentials));
    
    // Compare written data
    if (memcmp(&creds, &verify_creds, sizeof(FRAMCredentials)) != 0) {
        LOG_ERROR("FRAM credentials write verification failed!");
        return false;
    }
    
    LOG_INFO("Credentials written to FRAM at address 0x%04X", FRAM_CREDENTIALS_ADDR);
    return true;
}

bool verifyCredentialsInFRAM() {
    if (!framInitialized) {
        LOG_ERROR("FRAM not initialized for credentials verify");
        return false;
    }
    
    FRAMCredentials creds;
    if (!readCredentialsFromFRAM(creds)) {
        return false;
    }
    
    // Check magic number
    if (creds.magic != FRAM_MAGIC_NUMBER) {
        LOG_WARNING("Invalid credentials magic number: 0x%08X", creds.magic);
        return false;
    }
    
    // 🔄 UPDATED: Check version (accept v1, v2, and v3)
    if (creds.version != 0x0001 && creds.version != 0x0002 && creds.version != 0x0003) {
        LOG_WARNING("Invalid credentials version: %d", creds.version);
        return false;
    }
    
    // Verify checksum
    size_t checksum_offset = offsetof(FRAMCredentials, checksum);
    uint16_t calculated_checksum = calculateChecksum((uint8_t*)&creds, checksum_offset);
    
    if (creds.checksum != calculated_checksum) {
        LOG_WARNING("Credentials checksum mismatch: stored=%d, calculated=%d", 
                    creds.checksum, calculated_checksum);
        return false;
    }
    
    LOG_INFO("Credentials verification successful (version %d)", creds.version);
    return true;
}


// ===============================
// DAILY VOLUME MANAGEMENT
// ===============================

uint16_t calculateDailyVolumeChecksum(const DailyVolumeData& data) {
    uint16_t sum = 0;
    sum += data.volume_ml;
    
    // Add date string to checksum
    for (int i = 0; i < 11 && data.last_reset_date[i] != '\0'; i++) {
        sum += (uint8_t)data.last_reset_date[i];
    }
    
    return sum;
}

bool saveDailyVolumeToFRAM(uint16_t dailyVolume, const char* resetDate) {
    if (!framInitialized) {
        LOG_ERROR("FRAM not initialized for daily volume save");
        return false;
    }
    
    // Sanity check
    if (dailyVolume > 10000) {
        LOG_ERROR("Invalid daily volume: %d (max 10000ml)", dailyVolume);
        return false;
    }
    
    if (!resetDate || strlen(resetDate) != 10) {
        LOG_ERROR("Invalid reset date format (expected YYYY-MM-DD)");
        return false;
    }
    
    // Prepare data structure
    DailyVolumeData data;
    data.volume_ml = dailyVolume;
    strncpy(data.last_reset_date, resetDate, 11);
    data.last_reset_date[11] = '\0';
    
    // Write volume
    fram.write(FRAM_ADDR_DAILY_VOLUME, (uint8_t*)&data.volume_ml, 2);
    
    // Write date string
    fram.write(FRAM_ADDR_LAST_RESET_DATE, (uint8_t*)data.last_reset_date, 12);
    
    // Calculate and write checksum
    uint16_t checksum = calculateDailyVolumeChecksum(data);
    fram.write(FRAM_ADDR_DAILY_CHECKSUM, (uint8_t*)&checksum, 2);
    
    // Verify write by reading back
    DailyVolumeData verify;
    fram.read(FRAM_ADDR_DAILY_VOLUME, (uint8_t*)&verify.volume_ml, 2);
    fram.read(FRAM_ADDR_LAST_RESET_DATE, (uint8_t*)verify.last_reset_date, 12);
    
    if (verify.volume_ml != dailyVolume || 
        strcmp(verify.last_reset_date, resetDate) != 0) {
        LOG_ERROR("FRAM daily volume write verification failed!");
        return false;
    }
    
    LOG_INFO("✅ Daily volume saved to FRAM: %dml (date: %s)", dailyVolume, resetDate);
    return true;
}

bool loadDailyVolumeFromFRAM(uint16_t& dailyVolume, char* resetDate) {
    if (!framInitialized) {
        LOG_ERROR("FRAM not initialized for daily volume load");
        return false;
    }
    
    // Read data
    DailyVolumeData data;
    fram.read(FRAM_ADDR_DAILY_VOLUME, (uint8_t*)&data.volume_ml, 2);
    fram.read(FRAM_ADDR_LAST_RESET_DATE, (uint8_t*)data.last_reset_date, 12);
    
    // Verify checksum
    uint16_t calculatedChecksum = calculateDailyVolumeChecksum(data);
    uint16_t storedChecksum = 0;
    fram.read(FRAM_ADDR_DAILY_CHECKSUM, (uint8_t*)&storedChecksum, 2);
    
    if (calculatedChecksum != storedChecksum) {
        LOG_WARNING("Daily volume checksum mismatch: stored=%d, calculated=%d", 
                    storedChecksum, calculatedChecksum);
        return false;
    }
    
    // Sanity check
    if (data.volume_ml > 10000) {
        LOG_WARNING("FRAM daily volume out of range: %d", data.volume_ml);
        return false;
    }
    
    // Validate date format (basic check)
    if (strlen(data.last_reset_date) != 10 || 
        data.last_reset_date[4] != '-' || 
        data.last_reset_date[7] != '-') {
        LOG_WARNING("Invalid date format in FRAM: %s", data.last_reset_date);
        return false;
    }
    
    // Return values
    dailyVolume = data.volume_ml;
    if (resetDate) {
        strcpy(resetDate, data.last_reset_date);
    }
    
    LOG_INFO("✅ Daily volume loaded from FRAM: %dml (date: %s)", 
             dailyVolume, data.last_reset_date);
    
    return true;
}