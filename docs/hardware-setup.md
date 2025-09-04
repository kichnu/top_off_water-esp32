# Hardware Setup Guide

Complete guide for assembling the ESP32-C3 Water System hardware components.

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

### Optional Components

| Component | Quantity | Specification | Purpose |
|-----------|----------|---------------|---------|
| DS3231 RTC | 1 | I2C interface | Accurate timekeeping |
| FRAM Memory | 1 | I2C, 32KB (MB85RC256V) | Data persistence |
| Status LED | 1 | 5mm, any color | Visual feedback |
| Reset Button | 1 | Momentary push button | Error recovery |
| Enclosure | 1 | Weatherproof IP65+ | Protection |

### Additional Hardware

- **Resistors**: 10kΩ pull-up (2x for sensors)
- **Resistors**: 220Ω current limiting (1x for LED)
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
    RTC SDA →  D1  │5      17│ D8/A8    
    RTC SCL →  D2  │6      16│ D7/A7    
    Sensor1 →  D3  │7      15│ D6/A6    
    Sensor2 →  D4  │8      14│ D5/A5    ← Status LED
    Pump    →  D5  │9      13│ D4/A4    
               ... │10     12│ ...      
                   └─────────┘
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
            └─→ Sensors, RTC, LED (3.3V/5V)
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

### Step 3: Float Sensor Installation

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

### Step 4: Pump and Relay Installation

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

### Step 5: Optional Components

#### RTC Module (DS3231)
```
DS3231 VCC → ESP32 3.3V
DS3231 GND → ESP32 GND
DS3231 SDA → ESP32 Pin D6
DS3231 SCL → ESP32 Pin D7
```

#### FRAM Memory Module
```
FRAM VCC → ESP32 3.3V (same I2C bus as RTC)
FRAM GND → ESP32 GND
FRAM SDA → ESP32 Pin D6 (shared)
FRAM SCL → ESP32 Pin D7 (shared)
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

### Initial Hardware Test

1. **Power-up sequence**:
   - Connect 12V power supply
   - Verify 5V regulator output
   - Check ESP32 power LED

2. **Sensor verification**:
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

3. **Pump test**:
   ```cpp
   // Pump test code
   pinMode(2, OUTPUT);
   digitalWrite(2, HIGH);  // Activate pump
   delay(3000);           // Run for 3 seconds
   digitalWrite(2, LOW);   // Deactivate pump
   ```

### Sensor Calibration

1. **Position sensors** at appropriate water levels
2. **Test float activation** by manually moving floats
3. **Verify electrical continuity** with multimeter
4. **Confirm pull-up resistor values** (should read 3.3V when open)

### Flow Rate Calibration

1. **Measure pump output**:
   - Use graduated container
   - Run pump for exactly 30 seconds
   - Measure volume dispensed
   - Calculate ml/second rate

2. **Update firmware settings**:
   ```cpp
   // In web interface or config file
   volumePerSecond = measured_volume / 30.0;
   ```

## 🛡️ Safety Considerations

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
- **WiFi connection fails**: Verify credentials and signal strength
- **Serial monitor garbled**: Check baud rate (115200) and connections

## 📐 Mechanical Installation

### Mounting Considerations
- **Controller enclosure**: Accessible for maintenance, protected from weather
- **Sensor placement**: Stable mounting, free float movement
- **Pump location**: Submerged or self-priming, easy removal for service
- **Cable routing**: Protected from damage, allow for movement

### Environmental Protection
- **Temperature range**: Verify all components rated for ambient conditions
- **Humidity control**: Use desiccants in enclosures if necessary
- **UV protection**: Shield exposed components from direct sunlight
- **Vibration isolation**: Secure mounting to prevent connection loosening

---

**⚡ WARNING**: Always disconnect power when making wiring changes. Double-check all connections before applying power. Test system thoroughly before unattended operation.