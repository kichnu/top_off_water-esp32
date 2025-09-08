# Credential Management Guide

Complete guide for managing encrypted credentials in the ESP32-C3 Water System dual-mode architecture.

## 🎯 Overview

The ESP32-C3 Water System v2.0+ features a sophisticated dual-mode security architecture:

- **Programming Mode**: Secure credential programming via CLI
- **Production Mode**: Dynamic credential loading with encrypted storage
- **FRAM Storage**: AES-256 encrypted credential persistence
- **Fallback System**: Hardcoded credentials when FRAM unavailable

## 🔐 Security Architecture

### Encryption Method
- **Algorithm**: AES-256-CBC
- **Key Derivation**: Device-specific keys from device name + salt + seed
- **IV Generation**: Cryptographically secure random IV per device
- **Hash Function**: SHA-256 for password hashing

### Data Protection
```
Plaintext Credentials
        ↓
   SHA-256 Hashing (Admin Password)
        ↓
   AES-256-CBC Encryption
        ↓
   FRAM Storage (Non-volatile)
        ↓
   Production Mode Loading
        ↓
   Dynamic System Configuration
```

### Memory Layout
```
FRAM Memory (32KB):
├── 0x0000-0x0017: Common validation area (24 bytes)
├── 0x0018-0x0417: Encrypted credentials (1024 bytes)
│   ├── Magic: 0x57415452 ("WATR")
│   ├── Version: 0x0002
│   ├── Device Name: 32 bytes (plaintext)
│   ├── IV: 8 bytes
│   ├── Encrypted WiFi SSID: 64 bytes
│   ├── Encrypted WiFi Password: 128 bytes
│   ├── Encrypted Admin Hash: 96 bytes
│   ├── Encrypted VPS Token: 160 bytes
│   ├── Reserved: 528 bytes
│   └── Checksum: 2 bytes
└── 0x0500+: ESP32 water system data
```

## 🔧 Programming Mode

### Entering Programming Mode

1. **Connect ESP32 to computer**
2. **Upload Programming Mode firmware**:
   ```bash
   pio run -e programming -t upload
   ```
3. **Open CLI interface**:
   ```bash
   pio device monitor -e programming
   ```

### CLI Commands Reference

#### System Information
```bash
FRAM> help          # Show all available commands
FRAM> info          # Display system and FRAM information
FRAM> detect        # Verify FRAM hardware connectivity
```

#### Credential Management
```bash
FRAM> program       # Interactive credential programming
FRAM> config        # JSON-based credential configuration
FRAM> verify        # Verify stored credentials
FRAM> backup        # Backup FRAM contents (hex dump)
```

#### Testing & Diagnostics
```bash
FRAM> test          # Run comprehensive system tests
```

### Interactive Programming Workflow

```
FRAM> program

Device Name (1-31 chars, alphanumeric + _): WATER_PUMP_01
WiFi SSID (1-63 chars): MyNetworkSSID
WiFi Password (1-127 chars): MyNetworkPassword
Admin Password: admin123
VPS Token: abc123def456...

=== CREDENTIALS SUMMARY ===
Device Name: WATER_PUMP_01
WiFi SSID: MyNetworkSSID
WiFi Password: ******* (hidden)
Admin Password: ******* (will be hashed)
VPS Token: abc123def456...

Program these credentials to FRAM? (YES/no): YES

✅ Credentials programmed successfully!
```

### JSON Configuration Mode

```bash
FRAM> config
```

Paste JSON configuration:
```json
{
  "device_name": "WATER_PUMP_01",
  "wifi_ssid": "MyNetworkSSID", 
  "wifi_password": "MyNetworkPassword",
  "admin_password": "admin123",
  "vps_token": "abc123def456ghi789jkl012mno345pqr678stu901vwx234yz"
}
```

### Verification Process

```bash
FRAM> verify

[INFO] Verifying FRAM credentials...
✅ Credentials verification PASSED

=== DECRYPTED CREDENTIALS ===
Device Name: WATER_PUMP_01
WiFi SSID: MyNetworkSSID
WiFi Password: ******* (hidden)
Admin Hash: 5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8
VPS Token: abc123def456ghi789jkl012mno345pqr678stu901vwx234yz
```

## 🏭 Production Mode

### Entering Production Mode

1. **Upload Production Mode firmware**:
   ```bash
   pio run -e production -t upload
   ```
2. **Monitor startup logs**:
   ```bash
   pio device monitor -e production
   ```

### Dynamic Credential Loading

#### Successful FRAM Loading
```
[INFO] Initializing credentials manager...
[INFO] Attempting to load credentials from FRAM...
[INFO] Credentials verification successful
[INFO] ✅ Credentials loaded from FRAM successfully
[INFO] 🔐 Dynamic credentials loaded:
[INFO]   Device ID: WATER_PUMP_01
[INFO]   WiFi SSID: MyNetworkSSID
[INFO]   WiFi Password: ******* (16 chars)
[INFO]   Admin Hash: 5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8
[INFO]   VPS Token: abc123def456ghi789...
Device ID: WATER_PUMP_01
```

#### Fallback to Hardcoded Credentials
```
[WARN] No valid credentials found in FRAM
[WARN] 🔄 Falling back to hardcoded credentials
[INFO] 📌 Hardcoded fallback credentials:
[INFO]   Device ID: FALLBACK_DEVICE
[INFO]   WiFi SSID: DefaultSSID
Device ID: FALLBACK_DEVICE
```

### System Integration

**WiFi Connection:**
```
[INFO] Connecting to WiFi: MyNetworkSSID
[INFO] Credentials source: FRAM
[INFO] WiFi connected - IP: 192.168.0.164
[INFO] Using FRAM credentials
```

**Web Authentication:**
```
[INFO] Password verification successful (FRAM credentials)
[INFO] Session created for IP: 192.168.0.124
```

**VPS Logging:**
```json
{
  "device_id": "WATER_PUMP_01",
  "event_type": "AUTO_CYCLE_COMPLETE",
  "volume_ml": 100,
  "credentials_source": "FRAM"
}
```

## 🔄 Credential Update Workflow

### Complete Update Process

1. **Backup Current Credentials** (optional):
   ```bash
   # Switch to Programming Mode
   pio run -e programming -t upload
   pio device monitor -e programming
   
   FRAM> backup
   # Save hex dump output to file
   ```

2. **Program New Credentials**:
   ```bash
   FRAM> program
   # Enter new credentials interactively
   
   # OR use JSON config
   FRAM> config
   # Paste new JSON configuration
   ```

3. **Verify Programming**:
   ```bash
   FRAM> verify
   # Confirm new credentials are correct
   ```

4. **Deploy Production Mode**:
   ```bash
   # Switch back to Production Mode
   pio run -e production -t upload
   pio device monitor -e production
   
   # Verify new credentials load correctly
   ```

5. **Test System Operation**:
   - Verify WiFi connection with new credentials
   - Test web interface login with new admin password
   - Confirm VPS logging with new token

### Partial Updates

**WiFi Credentials Only:**
```bash
FRAM> verify          # Check current credentials
FRAM> program         # Update all (including WiFi)
# Enter new WiFi SSID/password, keep other fields same
```

**Admin Password Only:**
```bash
FRAM> program
# Keep device name, WiFi, VPS token same
# Enter new admin password
```

**VPS Token Only:**
```bash
FRAM> config
# Use JSON with new VPS token, keep other fields same
```

## 🛡️ Security Best Practices

### Physical Security
- **Secure Physical Access**: Programming Mode requires physical ESP32 access
- **Tamper-Evident Enclosure**: Protect FRAM module from unauthorized access
- **Access Logging**: Monitor Programming Mode usage

### Credential Hygiene
- **Strong Passwords**: Use complex admin passwords (12+ characters)
- **Unique Device Names**: Each device should have unique identifier
- **Token Rotation**: Regularly rotate VPS authentication tokens
- **Network Isolation**: Use dedicated IoT network for water systems

### Backup Strategy
```bash
# Before any credential changes
FRAM> backup > backup_$(date +%Y%m%d_%H%M%S).hex

# Store backup securely offline
# Include device name and date in filename
```

### Access Control
- **Programming Mode**: Restrict to authorized personnel only
- **Production Deployment**: Use automated deployment scripts
- **Credential Storage**: Never store plaintext credentials in version control

## 🚨 Emergency Procedures

### Lost Admin Password

1. **Switch to Programming Mode**:
   ```bash
   pio run -e programming -t upload
   pio device monitor -e programming
   ```

2. **Verify Current Credentials**:
   ```bash
   FRAM> verify
   # Check if other credentials are intact
   ```

3. **Reset Admin Password**:
   ```bash
   FRAM> program
   # Keep device name, WiFi, VPS token same
   # Enter new admin password
   ```

### FRAM Corruption

1. **Check FRAM Status**:
   ```bash
   FRAM> detect
   [ERROR] FRAM not found
   # or
   [WARNING] Invalid credentials magic number
   ```

2. **Hardware Verification**:
   - Check I2C wiring (SDA=Pin6, SCL=Pin7)
   - Verify FRAM power (3.3V)
   - Test I2C pull-up resistors (4.7kΩ)

3. **Reprogram Credentials**:
   ```bash
   FRAM> test         # Run hardware tests
   FRAM> program      # Reprogram from scratch
   ```

### Network Connectivity Issues

1. **Check WiFi Credentials**:
   ```bash
   # Programming Mode
   FRAM> verify
   # Verify WiFi SSID and password
   ```

2. **Test Connection**:
   ```bash
   # Production Mode startup logs
   [INFO] Connecting to WiFi: MyNetworkSSID
   [ERROR] WiFi connection failed after 20 attempts
   ```

3. **Update WiFi Credentials**:
   ```bash
   # Programming Mode
   FRAM> program
   # Update WiFi SSID/password
   ```

### Fallback Mode Recovery

When system runs in fallback mode:

1. **Identify Issue**:
   ```
   Device ID: FALLBACK_DEVICE  # Indicates fallback mode
   ```

2. **Check FRAM Hardware**:
   ```bash
   # Programming Mode
   FRAM> detect
   FRAM> test
   ```

3. **Reprogram Credentials**:
   ```bash
   FRAM> program      # Fresh credential programming
   ```

## 📊 Monitoring & Diagnostics

### Health Indicators

**Production Mode Startup:**
```bash
# Healthy FRAM credential loading
grep -E "(Device ID|Credentials loaded|WiFi connected)" startup.log

# Expected output:
# ✅ Credentials loaded from FRAM successfully
# Device ID: WATER_PUMP_01
# WiFi connected - Using FRAM credentials
```

**API Status Check:**
```bash
curl -s http://192.168.0.164/api/status | jq '.credentials_source'
# Expected: "FRAM" (not "FALLBACK")
```

### Performance Metrics

**Credential Loading Time:**
- Typical: 500-1000ms
- Warning: >2000ms (possible I2C issues)
- Error: >5000ms (likely FRAM failure)

**Memory Usage:**
- FRAM utilization: 1024 bytes for credentials
- Encryption overhead: ~30% (due to padding)
- Available space: 31KB for other data

## 🔧 Advanced Configuration

### Custom Encryption Parameters

Located in `src/crypto/fram_encryption.h`:
```cpp
#define ENCRYPTION_SALT "ESP32_FRAM_SALT_2024"
#define ENCRYPTION_SEED "WATER_SYSTEM_SEED_V1"
```

**Note**: Changing these invalidates existing credentials!

### Memory Layout Customization

Located in `src/hardware/fram_controller.h`:
```cpp
#define FRAM_CREDENTIALS_ADDR  0x0018
#define FRAM_CREDENTIALS_SIZE  1024
#define FRAM_ESP32_BASE        0x0500
```

### I2C Configuration

Default I2C settings:
```cpp
Wire.begin(6, 7);          // SDA=Pin6, SCL=Pin7
Wire.setClock(100000);     // 100kHz for FRAM compatibility
```

## 📋 Troubleshooting Checklist

### Pre-Deployment Verification
- [ ] FRAM detected at 0x50
- [ ] Programming Mode CLI accessible
- [ ] Credentials programmed successfully
- [ ] Verification passes
- [ ] Production Mode loads credentials
- [ ] WiFi connects with FRAM credentials
- [ ] Web login works with FRAM admin password
- [ ] VPS logging includes FRAM device ID

### Post-Deployment Monitoring
- [ ] System starts with FRAM credentials (not fallback)
- [ ] Web interface accessible
- [ ] VPS logging operational
- [ ] No credential-related errors in logs
- [ ] Backup credentials stored securely

### Security Audit
- [ ] Programming Mode requires physical access
- [ ] Production Mode has CLI disabled
- [ ] FRAM contains encrypted data (not plaintext)
- [ ] Fallback credentials are functional
- [ ] Access logs reviewed regularly

---

**🔐 Remember: Programming Mode requires physical access to ESP32. This is a security feature, not a limitation!**

**⚠️ Always backup credentials before making changes. FRAM corruption can result in permanent credential loss.**