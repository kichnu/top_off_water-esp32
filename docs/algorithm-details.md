# Algorithm Documentation

Detailed technical documentation of the intelligent water management algorithm implemented in the ESP32-C3 Water System.

## 🎯 Algorithm Overview

The water management system uses a sophisticated two-phase algorithm designed to minimize false triggers while ensuring reliable water level maintenance. The algorithm analyzes sensor timing patterns to make intelligent decisions about when and how much water to add.

### Core Principles

1. **Dual-Sensor Validation**: Uses two float sensors to confirm water level changes
2. **Timing Analysis**: Measures time gaps between sensor activations to assess water flow patterns
3. **Adaptive Response**: Adjusts pump operation based on historical performance data
4. **Error Recovery**: Multiple retry attempts with increasing intervention

## 📊 Algorithm States

### State Machine Overview

```
        IDLE
         │ 
    ┌────▼────┐
    │ TRIGGER │ ← One or both sensors activate
    └────┬────┘
         │
    ┌────▼────┐
    │TRYB_1   │ ← Wait for second sensor (TIME_GAP_1)
    │WAIT     │
    └────┬────┘
         │
    ┌────▼────┐
    │TRYB_1   │ ← Delay before pump start (TIME_TO_PUMP)
    │DELAY    │
    └────┬────┘
         │
    ┌────▼────┐
    │TRYB_2   │ ← Pump operating
    │PUMP     │
    └────┬────┘
         │
    ┌────▼────┐
    │TRYB_2   │ ← Verify sensor response
    │VERIFY   │
    └────┬────┘
         │
    ┌────▼────┐   Success     ┌─────────┐
    │TRYB_2   ├──────────────►│LOGGING  │
    │WAIT_GAP2│               └────┬────┘
    └────┬────┘                    │
         │ Timeout/Skip             │
         └─────────────────────────┘
                   │
              ┌────▼────┐
              │  IDLE   │ ← Return to monitoring
              └─────────┘
```

## ⏱️ Timing Parameters

### Core Timing Constants

```cpp
#define TIME_TO_PUMP            450    // Seconds from TRIGGER to pump start
#define TIME_GAP_1_MAX          300    // Max wait for second sensor (TRYB_1)
#define TIME_GAP_2_MAX          300    // Max wait for second sensor (TRYB_2)
#define THRESHOLD_1             180    // Threshold for TIME_GAP_1 evaluation
#define THRESHOLD_2             60     // Threshold for TIME_GAP_2 evaluation
#define WATER_TRIGGER_MAX_TIME  120    // Max time for sensor response after pump
#define THRESHOLD_WATER         30     // Threshold for WATER_TRIGGER evaluation
#define SENSOR_DEBOUNCE_TIME    1      // Sensor debouncing time
```

### Parameter Relationships

```
TIME_TO_PUMP > TIME_GAP_1_MAX + safety_margin
TIME_TO_PUMP > THRESHOLD_1 + startup_delay
WATER_TRIGGER_MAX_TIME > typical_pump_work_time
```

## 🔄 Phase 1: Water Level Detection (TRYB_1)

### Trigger Detection

The algorithm monitors both sensors continuously. A **TRIGGER** event occurs when:
- Either sensor activates (water level drops)
- System is in IDLE state
- No existing cycle is running

### TIME_GAP_1 Measurement

```
Timeline Example:
T=0s     Sensor1 activates → TRIGGER detected
T=45s    Sensor2 activates → TIME_GAP_1 = 45 seconds
T=450s   Pump starts (TIME_TO_PUMP elapsed)
```

#### Evaluation Logic

```cpp
bool isSlowDrain = (TIME_GAP_1 >= THRESHOLD_1);  // 180 seconds

if (isSlowDrain) {
    // Gradual water loss - likely normal evaporation/usage
    sensor_results |= RESULT_GAP1_FAIL;
    // Proceed to TRYB_2 normally
} else {
    // Rapid water loss - may indicate leak or high usage
    // Continue to TRYB_2, enable TIME_GAP_2 measurement
}
```

### Delay Phase (TIME_TO_PUMP)

**Purpose**: Allow water level to stabilize before pump activation
- Prevents oscillation from wave action
- Ensures consistent baseline for pump effectiveness measurement
- Provides time for any ongoing water usage to complete

## 🚰 Phase 2: Pump Operation (TRYB_2)

### Pump Activation

```cpp
uint16_t pumpWorkTime = SINGLE_DOSE_VOLUME / volumePerSecond;
// Example: 100ml / 2.5ml/s = 40 seconds pump time

// Safety check
if (pumpWorkTime > WATER_TRIGGER_MAX_TIME - 10) {
    pumpWorkTime = WATER_TRIGGER_MAX_TIME - 10;  // Leave 10s margin
}
```

### WATER_TRIGGER_TIME Measurement

Measures how quickly sensors respond to pump operation:

```
Pump Start (T=0)
    │
    ├─→ Sensor monitoring begins
    │
    └─→ T=8s: First sensor deactivates → WATER_TRIGGER_TIME = 8s
```

#### Evaluation Criteria

```cpp
bool isPumpEffective = (WATER_TRIGGER_TIME < THRESHOLD_WATER);  // 30 seconds

if (!isPumpEffective) {
    sensor_results |= RESULT_WATER_FAIL;
    // Pump may be clogged, low flow, or sensors malfunction
}
```

### Retry Logic

If sensors don't respond within `WATER_TRIGGER_MAX_TIME`:

```cpp
if (pumpAttempts < PUMP_MAX_ATTEMPTS) {
    // Retry pump operation
    currentState = STATE_TRYB_1_DELAY;
    stateStartTime = currentTime - TIME_TO_PUMP;  // Immediate retry
} else {
    // All attempts failed → ERROR_PUMP_FAILURE
    currentCycle.error_code = ERROR_PUMP_FAILURE;
    startErrorSignal(ERROR_PUMP_FAILURE);
}
```

## 📈 Phase 2.5: Recovery Analysis (TRYB_2_WAIT_GAP2)

### Conditions for TIME_GAP_2 Measurement

TIME_GAP_2 is measured **only when**:
- TRYB_1 result was successful (TIME_GAP_1 < THRESHOLD_1)
- Both sensors successfully deactivated after pump operation
- System needs to measure recovery timing

### TIME_GAP_2 Calculation

```
Sensor1 Release: T=125s
Sensor2 Release: T=140s
TIME_GAP_2 = |140 - 125| = 15 seconds
```

#### Evaluation Logic

```cpp
bool isSlowRecovery = (TIME_GAP_2 >= THRESHOLD_2);  // 60 seconds

if (isSlowRecovery) {
    sensor_results |= RESULT_GAP2_FAIL;
    // May indicate restricted flow or sensor issues
}
```

## 🧠 Decision Matrix

### Algorithm Decision Tree

```
TRIGGER Detected
    │
    ├─→ Wait TIME_GAP_1 (max 300s)
    │
    └─→ TIME_GAP_1 < 180s? 
            │
            ├─→ YES: Fast drain → Enable TIME_GAP_2 measurement
            │
            └─→ NO: Slow drain → Skip TIME_GAP_2 measurement
                    │
                    └─→ Wait TIME_TO_PUMP (450s total)
                            │
                            └─→ Start pump for calculated duration
                                    │
                                    ├─→ Sensors respond < 120s?
                                    │   │
                                    │   ├─→ YES: Success → Measure TIME_GAP_2 (if enabled)
                                    │   │
                                    │   └─→ NO: Retry (max 3 attempts) or FAIL
                                    │
                                    └─→ Log cycle data and return to IDLE
```

## 📊 Error Detection & Statistics

### Individual Cycle Results

Each cycle generates three binary results:
```cpp
struct CycleResults {
    bool gap1_fail;    // TIME_GAP_1 >= THRESHOLD_1 (180s)
    bool gap2_fail;    // TIME_GAP_2 >= THRESHOLD_2 (60s)  
    bool water_fail;   // WATER_TRIGGER_TIME >= THRESHOLD_WATER (30s)
};
```

### Cumulative Statistics

The system maintains running totals in FRAM:
```cpp
struct ErrorStats {
    uint16_t gap1_fail_sum;      // Total GAP1 failures
    uint16_t gap2_fail_sum;      // Total GAP2 failures  
    uint16_t water_fail_sum;     // Total WATER failures
    uint32_t last_reset_timestamp; // When stats were reset
};
```

### Error Interpretation

| Error Type | Possible Causes | Recommended Actions |
|------------|----------------|-------------------|
| **GAP1 High** | Slow water loss, normal evaporation | Monitor trends, may be normal |
| **GAP2 High** | Slow sensor recovery, restricted flow | Check sensor mounting, water circulation |
| **WATER High** | Pump issues, low flow, sensor problems | Inspect pump, clean filters, calibrate |

## 🔧 Calibration Procedures

### Volume Per Second Calibration

1. **Run extended pump cycle** (30 seconds)
2. **Measure actual volume** dispensed
3. **Calculate rate**: `volumePerSecond = measured_ml / 30.0`
4. **Update system settings** via web interface

### Threshold Optimization

#### THRESHOLD_1 (Gap1 Threshold)
- **Monitor historical TIME_GAP_1 values**
- **Adjust based on typical drain patterns**
- **Lower values** = More sensitive to fast drains
- **Higher values** = More cycles include TIME_GAP_2 measurement

#### THRESHOLD_2 (Gap2 Threshold)  
- **Analyze TIME_GAP_2 distributions**
- **Set based on normal recovery patterns**
- **Consider sensor positioning and water flow dynamics**

#### THRESHOLD_WATER (Water Trigger Threshold)
- **Based on pump performance and sensor response time**
- **Should be > typical pump work time**
- **Allow margin for sensor switching delays**

## 🚨 Safety Mechanisms

### Daily Volume Limits

```cpp
#define FILL_WATER_MAX 2400  // ml per day

if (dailyVolumeML > FILL_WATER_MAX) {
    currentCycle.error_code = ERROR_DAILY_LIMIT;
    startErrorSignal(ERROR_DAILY_LIMIT);
    currentState = STATE_ERROR;
}
```

### Pump Protection

- **Maximum 3 retry attempts** per trigger event
- **WATER_TRIGGER_MAX_TIME** prevents infinite pump operation
- **Global pump disable** with 30-minute auto-recovery
- **Manual emergency stop** always available

### Error Signaling

Error codes transmitted via dedicated GPIO pin:

| Error | Pattern | Description |
|-------|---------|-------------|
| ERR1 | 1 pulse | Daily limit exceeded |
| ERR2 | 2 pulses | Pump failure (3 failed attempts) |
| ERR0 | 3 pulses | Combined errors |

## 📝 Data Logging

### Cycle Data Structure

```cpp
struct PumpCycle {
    uint32_t timestamp;           // Unix timestamp
    uint32_t trigger_time;        // When TRIGGER occurred
    uint32_t time_gap_1;         // Gap between sensor activations
    uint32_t time_gap_2;         // Gap between sensor deactivations  
    uint32_t water_trigger_time; // Pump response time
    uint16_t pump_duration;      // Actual pump run time
    uint8_t  pump_attempts;      // Number of retry attempts
    uint8_t  sensor_results;     // Binary result flags
    uint8_t  error_code;         // Error code (0 = success)
    uint8_t  volume_dose;        // Volume in 10ml units
};
```

### VPS Integration

Complete cycle data transmitted to remote server:
```json
{
  "device_id": "ESP32C3_WaterPump_001",
  "event_type": "AUTO_CYCLE_COMPLETE", 
  "time_gap_1": 45,
  "time_gap_2": 22,
  "water_trigger_time": 8,
  "gap1_fail_sum": 15,
  "gap2_fail_sum": 3,
  "water_fail_sum": 1,
  "algorithm_data": "THRESHOLDS(GAP1:180s,GAP2:60s,WATER:30s) CURRENT(0-0-0) SUMS(15-3-1)"
}
```

## 🎛️ Performance Tuning

### Optimizing for Different Applications

#### High-Turnover Systems (Aquaponics, Hydration)
```cpp
#define TIME_TO_PUMP     300    // Faster response
#define THRESHOLD_1      120    // More sensitive to drain patterns
#define FILL_WATER_MAX   5000   // Higher daily limit
```

#### Stable Systems (Decorative Ponds, Storage)
```cpp
#define TIME_TO_PUMP     600    // More conservative
#define THRESHOLD_1      300    // Less sensitive to variations
#define FILL_WATER_MAX   1000   // Lower daily limit
```

#### Precision Applications (Laboratory, Research)
```cpp
#define SENSOR_DEBOUNCE_TIME  0.1   // Higher sensor sensitivity
#define SINGLE_DOSE_VOLUME    50    // Smaller dose increments
#define PUMP_MAX_ATTEMPTS     5     // More retry attempts
```

---

**📊 The algorithm continuously learns from sensor patterns and pump performance to optimize water management while preventing system failures and ensuring reliable operation.**