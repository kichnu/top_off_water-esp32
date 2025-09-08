# Hardware Setup Guide

Complete guide for assembling the ESP32-C3 Water System hardware components with **secure credential storage**.

## 📋 Bill of Materials

### Essential Components

| Component | Quantity | Specification | Notes |
|-----------|----------|---------------|-------|
| ESP32-C3 Board | 1 | Seeed Xiao ESP32-C3 (recommended) | Main controller |
| Float Sensors | 2 | NC (Normally Closed) type | Water level detection |
| Water Pump | 1 | 12V DC, 1-3L/min flow rate | Submersible recommended |
| Relay Module | 1 | 5V/12V, 10A capacity | Pump control |
| Power Supply | 1 | 12V/2A DC adapter | Powers pump and system |
| Voltage Regulator | 1 | 12V to 5V, 2A capacity | ESP32 power |

### **🔐 Credential Storage (REQUIRED for v2.0+)**

| Component | Quantity | Specification | Purpose |
|-----------|----------|---------------|---------|
| **FRAM Memory** | **1** | **I2C, 32KB (MB85RC256V)** | **Encrypted credential storage** |
| DS3231 RTC | 1 | I2C interface | Accurate timekeeping |

### Optional Components

| Component | Quantity | Specification | Purpose |
|-----------|----------|---------------|---------|
| Status LED | 1 | 5mm, any color | Visual feedback |
| Reset Button | 1 | Momentary push button | Error recovery |
| Enclosure | 1 | Weatherproof IP65+ | Protection |

### Additional Hardware

- **Resistors**: 10kΩ pull-up (2x for sensors)
- **Resistors**: 220Ω current limiting (1x for LED)
- **Resistors**: 4.7kΩ pull-up (2x for I2C bus) **🆕**
- **Wires**: 22 AWG stranded, various colors
- **Connectors**: Terminal blocks, JST connectors
- **Mounting**: Screws, standoffs, cable management

## 🔌 Wiring Diagram

### ESP32-C3 Pin Assignments (Seeed Xiao ESP32-C3)

```
                    ESP32-C3
                   ┌─────────┐
               5V  │1      21│ 5V
               GND │2      20│ GND
               3V3 │3      19│ D10/A10  ← Reset Button
    LED →      D0  │4      18│ D9/A9    ← Error Signal
    FRAM SDA → D1  │5      17│ D8/A8    
    FRAM SCL → D2  │6      16│ D7/A7    
    Sensor1 →  D3  │7      15│ D6/A6    ← RTC SDA (shared)
    Sensor2 →  D4  │8      14│ D5/A5    ← RTC SCL (shared)  
    Pump    →  D5  │9      13│ D4/A4    ← Status LED
               ... │10     12│ ...      
                   └─────────┘
```

### **🔐 FRAM + RTC I2C Bus (CRITICAL)**

```
I2C Bus (Shared):
    ESP32 Pin D6 (SDA) ─┬─→ FRAM SDA (0x50)
                        └─→ RTC SDA (0x68)
                        
    ESP32 Pin D7 (SCL) ─┬─→ FRAM SCL (0x50)  
                        └─→ RTC SCL (0x68)

Pull-up Resistors:
    3.3V ─[4.7kΩ]─→ SDA line
    3.3V ─[4.7kΩ]─→ SCL line
```

### Power Distribution

```
12V DC Supply
    │
    ├─→ Pump Relay (12V coil)
    │   └─→ Water Pump (12V)
    │
    └─→ 12V to 5V Regulator
        └─→ ESP32-C3 (5V input)
            └─→ Sensors, RTC, FRAM, LED (3.3V/5V)
```

### Float Sensor Wiring

```
Float Sensor (NC Type)
    │
    ├─→ ESP32 GPIO Pin (D3 or D4)
    │
    └─→ 10kΩ Pull-up Resistor → 3.3V

Note: NC (Normally Closed) sensors are CLOSED when 
water level is LOW (triggering pump activation)
```

## 🔧 Assembly Instructions

### Step 1: Prepare the ESP32-C3 Board

1. **Solder headers** to the ESP32-C3 if not pre-installed
2. **Test basic functionality** by uploading a simple blink sketch
3. **Mount** in enclosure with appropriate standoffs

### Step 2: Install Power System

1. **Mount the 12V to 5V regulator** in enclosure
2. **Connect 12V input** from external power supply
3. **Wire 5V output** to ESP32-C3 VIN pin
4. **Add common ground** connections throughout system

### **Step 3: Install FRAM Memory Module (CRITICAL) 🔐**

#### FRAM Module Selection
**Recommended**: MB85RC256V (32KB FRAM)
- **Address**: 0x50 (default)
- **Interface**: I2C
- **Voltage**: 3.3V
- **Speed**: Up to 1MHz

#### FRAM Installation Steps
1. **Mount FRAM module** securely in enclosure
2. **Connect power**:
   ```
   FRAM VCC → ESP32 3.3V
   FRAM GND → ESP32 GND
   ```
3. **Connect I2C bus**:
   ```
   FRAM SDA → ESP32 Pin D6
   FRAM SCL → ESP32 Pin D7
   ```
4. **Install I2C pull-ups**:
   ```
   4.7kΩ resistor: 3.3V to SDA line
   4.7kΩ resistor: 3.3V to SCL line
   ```

#### **FRAM Verification**
```cpp
// Test code for FRAM detection
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(6, 7);  // SDA=6, SCL=7
  
  Wire.beginTransmission(0x50);
  if (Wire.endTransmission() == 0) {
    Serial.println("✅ FRAM detected at 0x50");
  } else {
    Serial.println("❌ FRAM not found!");
  }
}
```

### Step 4: Float Sensor Installation

#### Sensor Preparation
```
Float Sensor Wiring:
- Red/Brown: Not used (NO contact)
- Black: Common
- Blue/White: NC contact (use this)
```

#### Installation Steps
1. **Mount sensors** at desired water levels
   - Sensor 1: Lower level (primary trigger)
   - Sensor 2: Upper level (secondary confirmation)
2. **Run cables** to control enclosure
3. **Connect to ESP32**:
   ```
   Sensor 1: Pin D3 + 10kΩ pull-up to 3.3V
   Sensor 2: Pin D4 + 10kΩ pull-up to 3.3V
   Both grounds to ESP32 GND
   ```

### Step 5: Pump and Relay Installation

1. **Mount relay module** in enclosure
2. **Connect relay control**:
   ```
   Relay VCC → ESP32 5V
   Relay GND → ESP32 GND  
   Relay IN1 → ESP32 Pin D2
   ```
3. **Connect pump circuit**:
   ```
   12V Supply (+) → Relay COM
   Relay NO → Pump (+)
   Pump (-) → 12V Supply GND
   ```

### Step 6: Optional Components

#### RTC Module (DS3231) - Shared I2C Bus
```
DS3231 VCC → ESP32 3.3V
DS3231 GND → ESP32 GND
DS3231 SDA → ESP32 Pin D6 (shared with FRAM)
DS3231 SCL → ESP32 Pin D7 (shared with FRAM)
```

#### Status LED
```
LED Anode → 220Ω Resistor → ESP32 Pin D5
LED Cathode → ESP32 GND
```

#### Reset Button
```
Button Pin 1 → ESP32 Pin D10
Button Pin 2 → ESP32 GND
(Internal pull-up enabled in software)
```

## 🧪 Testing & Calibration

### **Step 1: FRAM Credential System Test (CRITICAL) 🔐**

#### Programming Mode Test
1. **Upload Programming Mode**:
   ```bash
   pio run -e programming -t upload
   pio device monitor -e programming
   ```

2. **Verify FRAM Detection**:
   ```
   FRAM> detect
   [SUCCESS] FRAM detected at address 0x50
   ```

3. **Program Test Credentials**:
   ```
   FRAM> program
   Device Name: TEST_DEVICE
   WiFi SSID: TestNetwork
   WiFi Password: TestPassword123
   Admin Password: testadmin
   VPS Token: test_token_123
   ```

4. **Verify Programming**:
   ```
   FRAM> verify
   [SUCCESS] Credentials verification PASSED
   ```

#### Production Mode Test
1. **Upload Production Mode**:
   ```bash
   pio run -e production -t upload
   pio device monitor -e production
   ```

2. **Verify Dynamic Loading**:
   ```
   ✅ Credentials loaded from FRAM successfully
   Device ID: TEST_DEVICE
   WiFi SSID: TestNetwork
   ```

### Step 2: Hardware Verification

1. **Power-up sequence**:
   - Connect 12V power supply
   - Verify 5V regulator output
   - Check ESP32 power LED

2. **I2C Bus Test**:
   ```cpp
   // I2C scanner code
   for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
     Wire.beginTransmission(addr);
     if (Wire.endTransmission() == 0) {
       Serial.printf("Device found: 0x%02X\n", addr);
     }
   }
   // Expected: 0x50 (FRAM), 0x68 (RTC)
   ```

3. **Sensor verification**:
   ```cpp
   // Test code snippet
   void setup() {
     Serial.begin(115200);
     pinMode(3, INPUT_PULLUP);  // Sensor 1
     pinMode(4, INPUT_PULLUP);  // Sensor 2
   }
   
   void loop() {
     Serial.printf("Sensor1: %d, Sensor2: %d\n", 
                   digitalRead(3), digitalRead(4));
     delay(1000);
   }
   ```

4. **Pump test**:
   ```cpp
   // Pump test code
   pinMode(2, OUTPUT);
   digitalWrite(2, HIGH);  // Activate pump
   delay(3000);           // Run for 3 seconds
   digitalWrite(2, LOW);   // Deactivate pump
   ```

### Step 3: System Integration Test

1. **Complete Workflow Test**:
   - Program credentials via Programming Mode
   - Switch to Production Mode
   - Verify credential loading
   - Test web interface login
   - Verify VPS logging (if configured)

### Flow Rate Calibration

1. **Measure pump output**:
   - Use graduated container
   - Run pump for exactly 30 seconds
   - Measure volume dispensed
   - Calculate ml/second rate

2. **Update firmware settings**:
   ```cpp
   // Via web interface
   volumePerSecond = measured_volume / 30.0;
   ```

## 🛡️ Security Considerations

### **Credential Protection 🔐**
- **FRAM Physical Security**: Mount in tamper-evident enclosure
- **I2C Bus Protection**: Shield SDA/SCL lines from interference
- **Access Control**: Programming Mode requires physical access
- **Backup Strategy**: Use `FRAM> backup` command before changes

### Electrical Safety
- **Use appropriate fuse** on 12V supply (3A recommended)
- **Ensure proper grounding** of all components
- **Use weatherproof connectors** for outdoor installations
- **Test GFCI functionality** if using mains power

### Water Protection
- **Seal all electrical connections** with appropriate IP rating
- **Position controllers** above potential flood levels
- **Use marine-grade wire** in wet environments
- **Test enclosure sealing** before installation

### System Safety
- **Install manual shutoff valve** for emergency pump stop
- **Add overflow protection** to prevent system flooding
- **Include water level indicators** for visual confirmation
- **Document all connections** for future maintenance

## 🔍 Troubleshooting

### **FRAM/Credential Issues 🔐**

**FRAM Not Detected**
- **Check I2C wiring**: SDA=Pin6, SCL=Pin7
- **Verify pull-up resistors**: 4.7kΩ on both lines
- **Test I2C addresses**: Should find 0x50 (FRAM), 0x68 (RTC)
- **Check power**: FRAM VCC should be 3.3V

**Credential Programming Failed**
- **Verify FRAM detection** first: `FRAM> detect`
- **Check memory integrity**: `FRAM> test`
- **Try different I2C speed**: Lower frequency if unstable
- **Verify pull-up resistors**: May need stronger (2.2kΩ) pull-ups

**Production Mode Not Loading Credentials**
- **Check Programming Mode first**: `FRAM> verify`
- **Monitor startup logs**: Look for credential loading messages
- **Verify FRAM version**: Should show "unified layout v2"
- **Test fallback mode**: Should use hardcoded credentials if FRAM fails

**I2C Bus Conflicts**
- **Multiple devices**: FRAM (0x50) and RTC (0x68) should not conflict
- **Check address jumpers**: Some FRAM modules have address selection
- **Verify shared bus**: Both devices on same SDA/SCL lines

### Power Issues
- **No power to ESP32**: Check 12V supply and regulator output
- **Brown-out resets**: Ensure adequate current capacity (2A minimum)
- **Voltage drops**: Check wire gauge and connection quality

### Sensor Problems  
- **Sensors always active**: Check for stuck floats or wiring shorts
- **Sensors never trigger**: Verify pull-up resistors and float movement
- **Intermittent readings**: Clean float mechanism, check connections

### Pump Issues
- **Pump won't start**: Verify relay operation and 12V supply
- **Pump runs continuously**: Check software logic and sensor states
- **Low flow rate**: Clean pump impeller, check for blockages

### Communication Problems
- **I2C devices not found**: Check SDA/SCL connections and pull-ups
- **WiFi connection fails**: Verify credentials via Programming Mode
- **Serial monitor garbled**: Check baud rate (115200) and connections

## 📐 Mechanical Installation

### Mounting Considerations
- **Controller enclosure**: Accessible for maintenance, protected from weather
- **FRAM security**: Mount in tamper-evident location
- **Sensor placement**: Stable mounting, free float movement
- **Pump location**: Submerged or self-priming, easy removal for service
- **Cable routing**: Protected from damage, allow for movement

### Environmental Protection
- **Temperature range**: Verify all components rated for ambient conditions
- **Humidity control**: Use desiccants in enclosures if necessary
- **UV protection**: Shield exposed components from direct sunlight
- **Vibration isolation**: Secure mounting to prevent connection loosening

## 📋 **Component Checklist (v2.0+ Requirements)**

### **Before Assembly**
- [ ] ESP32-C3 board with headers
- [ ] **FRAM module (MB85RC256V or compatible)**
- [ ] DS3231 RTC module (recommended)
- [ ] Float sensors (2x NC type)
- [ ] Pump and relay module
- [ ] Power supply and voltage regulator
- [ ] Pull-up resistors: 10kΩ (2x), **4.7kΩ (2x for I2C)**

### **After Assembly**
- [ ] **FRAM detected at 0x50**
- [ ] RTC detected at 0x68 (if installed)
- [ ] Programming Mode CLI functional
- [ ] **Credentials programmed and verified**
- [ ] Production Mode loads credentials automatically
- [ ] Web interface accessible with FRAM admin password
- [ ] Sensors respond correctly
- [ ] Pump operates as expected

### **Security Verification**
- [ ] **Programming Mode**: CLI accessible, water system disabled
- [ ] **Production Mode**: CLI disabled, water system enabled
- [ ] **Credential encryption**: Verify data not readable in raw FRAM
- [ ] **Fallback operation**: System works with hardcoded credentials when FRAM empty
- [ ] **Access control**: Physical access required for credential programming

---

**⚡ WARNING**: Always disconnect power when making wiring changes. **FRAM memory contains encrypted credentials - handle securely!** Double-check all connections before applying power. Test credential system thoroughly before deployment.