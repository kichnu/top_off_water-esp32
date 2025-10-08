#ifndef ALGORITHM_CONFIG_H
#define ALGORITHM_CONFIG_H

#include <Arduino.h>


// ============== PARAMETRY CZASOWE [sekundy] ==============
// #define TIME_TO_PUMP            450    // 450/7.5 min - czas od TRIGGER do startu pompy
// #define TIME_GAP_1_MAX          300    // 300/5 min - max oczekiwanie na drugi czujnik (TRYB_1)
// #define TIME_GAP_2_MAX          300    // 300/5 min - max oczekiwanie na drugi czujnik (TRYB_2)
// #define THRESHOLD_1             200    // 180/3 min - próg dla TIME_GAP_1
// #define THRESHOLD_2             200     // 60/1 min - próg dla TIME_GAP_2
// #define WATER_TRIGGER_MAX_TIME  120    // 120/2 min - max czas na reakcję czujników po starcie pompy
// #define THRESHOLD_WATER         30     // 30s - próg dla WATER_TRIGGER_TIME
// #define LOGGING_TIME            5      // 5s - czas na logowanie po cyklu
// #define SENSOR_DEBOUNCE_TIME    1      // 1s - debouncing czujników

#define TIME_TO_PUMP            20    // 450/7.5 min - czas od TRIGGER do startu pompy
#define TIME_GAP_1_MAX          15    // 300/5 min - max oczekiwanie na drugi czujnik (TRYB_1)
#define TIME_GAP_2_MAX          15    // 300/5 min - max oczekiwanie na drugi czujnik (TRYB_2)
#define THRESHOLD_1             10    // 180/3 min - próg dla TIME_GAP_1
#define THRESHOLD_2             10     // 60/1 min - próg dla TIME_GAP_2
#define WATER_TRIGGER_MAX_TIME  30    // 120/2 min - max czas na reakcję czujników po starcie pompy
#define THRESHOLD_WATER         10     // 30s - próg dla WATER_TRIGGER_TIME
#define LOGGING_TIME            5      // 5s - czas na logowanie po cyklu
#define SENSOR_DEBOUNCE_TIME    1      // 1s - debouncing czujników



// ============== PARAMETRY POMPY ==============
#define PUMP_MAX_ATTEMPTS       3      // Maksymalna liczba prób pompy w TRYB_2

#define SINGLE_DOSE_VOLUME      100    // ml - objętość jednej dolewki
#define FILL_WATER_MAX          2400   // ml - max dolewka na dobę

// ============== SYGNALIZACJA BŁĘDÓW ==============
#define ERROR_PULSE_HIGH        100    // ms - czas impulsu HIGH
#define ERROR_PULSE_LOW         100    // ms - czas przerwy między impulsami
#define ERROR_PAUSE             2000   // ms - pauza przed powtórzeniem sekwencji

// ============== SPRAWDZENIA INTEGRALNOŚCI ==============
// static_assert(TIME_TO_PUMP >= 300 && TIME_TO_PUMP <= 600, "TIME_TO_PUMP must be 300-600s");
// static_assert(TIME_TO_PUMP > (TIME_GAP_1_MAX + 10), "TIME_TO_PUMP must be > TIME_GAP_1_MAX + 10s");
// static_assert(TIME_TO_PUMP > (THRESHOLD_1 + 30), "TIME_TO_PUMP must be > THRESHOLD_1 + 30s");
// static_assert(WATER_TRIGGER_MAX_TIME > 30, "WATER_TRIGGER_MAX_TIME must be > typical pump work time");


// ============== STANY ALGORYTMU ==============
enum AlgorithmState {
    STATE_IDLE = 0,           // Oczekiwanie na TRIGGER
    STATE_TRYB_1_WAIT,        // Czekanie na TIME_GAP_1
    STATE_TRYB_1_DELAY,       // Odliczanie TIME_TO_PUMP
    STATE_TRYB_2_PUMP,        // Pompa pracuje
    STATE_TRYB_2_VERIFY,      // Weryfikacja reakcji czujników
    STATE_TRYB_2_WAIT_GAP2,   // Czekanie na TIME_GAP_2
    STATE_LOGGING,            // Logowanie wyników
    STATE_ERROR,              // Stan błędu
    STATE_MANUAL_OVERRIDE     // Manual pump przerwał cykl
};

// ============== KODY BŁĘDÓW ==============
enum ErrorCode {
    ERROR_NONE = 0,
    ERROR_DAILY_LIMIT = 1,    // ERR1: przekroczono FILL_WATER_MAX
    ERROR_PUMP_FAILURE = 2,   // ERR2: 3 nieudane próby pompy
    ERROR_BOTH = 3            // ERR0: oba błędy
};

// ============== STRUKTURA CYKLU ==============
struct PumpCycle {
    uint32_t timestamp;        // Unix timestamp rozpoczęcia
    uint32_t trigger_time;     // Czas aktywacji TRIGGER
    uint32_t time_gap_1;       // Czas między T1 i T2 (opadanie)
    uint32_t time_gap_2;       // Czas między T1 i T2 (podnoszenie)
    uint32_t water_trigger_time; // Czas reakcji czujników na pompę
    uint16_t pump_duration;    // Rzeczywisty czas pracy pompy
    uint8_t  pump_attempts;    // Liczba prób pompy
    uint8_t  sensor_results;   // Flagi wyników (bity 0-2)
    uint8_t  error_code;       // Kod błędu
    uint16_t  volume_dose;      // Objętość w ml
    
    // Sensor results bit flags
    static const uint8_t RESULT_GAP1_FAIL = 0x01;  // TIME_GAP_1 >= THRESHOLD_1
    static const uint8_t RESULT_GAP2_FAIL = 0x02;  // TIME_GAP_2 >= THRESHOLD_2
    static const uint8_t RESULT_WATER_FAIL = 0x04; // WATER_TRIGGER >= THRESHOLD_WATER
};

// ============== FUNKCJE POMOCNICZE ==============
inline uint8_t sensor_time_match_function(uint16_t time_ms, uint16_t threshold_ms) {
    return (time_ms >= threshold_ms) ? 1 : 0;
}

// ============== OBLICZANIE CZASU POMPY ==============
inline uint16_t calculatePumpWorkTime(float volumePerSecond) {
    // PUMP_WORK_TIME = SINGLE_DOSE_VOLUME / volumePerSecond
    return (uint16_t)(SINGLE_DOSE_VOLUME / volumePerSecond);
}

// Sprawdzenie zgodności z ograniczeniami
inline bool validatePumpWorkTime(uint16_t pumpWorkTime) {
    return pumpWorkTime <= WATER_TRIGGER_MAX_TIME;
}

#endif