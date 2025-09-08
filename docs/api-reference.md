# API Reference

Complete reference for the ESP32-C3 Water System REST API endpoints with **dynamic credential support**.

## 🔐 Authentication & Security

All API endpoints (except `/api/login`) require authentication via session token. **Admin password loaded dynamically from FRAM encrypted storage.**

### Login
```http
POST /api/login
Content-Type: application/x-www-form-urlencoded

password=your_admin_password_from_fram
```

**Response (Success):**
```json
{
  "success": true
}
```

**Response (Error):**
```json
{
  "success": false,
  "error": "Invalid password"
}
```

**Notes:**
- **Admin password verified against FRAM stored hash** (AES-256 encrypted)
- **Fallback to hardcoded hash** if FRAM credentials unavailable
- Sets HTTP-only session cookie
- Session expires after 5 minutes of inactivity
- Rate limited: max 5 attempts per IP per minute

### Logout
```http
POST /api/logout
```

**Response:**
```json
{
  "success": true
}
```

## 📊 System Status

### Get System Status
```http
GET /api/status
```

**Response:**
```json
{
  "water_status": "NORMAL",
  "pump_running": false,
  "pump_remaining": 0,
  "wifi_status": "Connected", 
  "wifi_connected": true,
  "rtc_time": "2024-01-15 14:30:25 (EXT)",
  "rtc_working": true,
  "rtc_info": "Using DS3231 external RTC",
  "free_heap": 184320,
  "uptime": 3600000,
  "device_id": "DOLEWKA",
  "credentials_source": "FRAM",
  "system_mode": "PRODUCTION"
}
```

**🆕 New Fields:**
- `device_id`: **Dynamic from FRAM credentials** (or fallback)
- `credentials_source`: `"FRAM"` or `"FALLBACK"`
- `system_mode`: `"PRODUCTION"` (API only available in Production Mode)

**Water Status Values:**
- `"NORMAL"` - Both sensors normal
- `"SENSOR1_LOW"` - Sensor 1 triggered
- `"SENSOR2_LOW"` - Sensor 2 triggered  
- `"BOTH_LOW"` - Both sensors triggered
- `"CHECKING"` - Sensors debouncing

**WiFi Status Values:**
- `"Connected"` - **Using FRAM or fallback WiFi credentials**
- `"Disconnected"`
- `"SSID not found"`
- `"Connection failed"`
- `"Connection lost"`

## 🚰 Pump Control

### Start Normal Pump Cycle
```http
POST /api/pump/normal
```

**Response:**
```json
{
  "success": true,
  "duration": 15,
  "volume_ml": 37.5
}
```

**Calculated Fields:**
- `duration`: From `currentPumpSettings.normalCycleSeconds`
- `volume_ml`: `duration * currentPumpSettings.volumePerSecond`

### Start Extended Pump Cycle (Calibration)
```http
POST /api/pump/extended
```

**Response:**
```json
{
  "success": true,
  "duration": 30,
  "type": "extended"
}
```

### Stop Pump (Emergency)
```http
POST /api/pump/stop
```

**Response:**
```json
{
  "success": true,
  "message": "Pump stopped"
}
```

## ⚙️ Settings Management

### Get Pump Settings
```http
GET /api/pump-settings
```

**Response:**
```json
{
  "success": true,
  "volume_per_second": 2.5,
  "normal_cycle": 15,
  "extended_cycle": 30,
  "auto_mode": true
}
```

### Update Pump Settings
```http
POST /api/pump-settings
Content-Type: application/x-www-form-urlencoded

volume_per_second=3.2
```

**Response:**
```json
{
  "success": true,
  "volume_per_second": 3.2,
  "message": "Volume per second updated successfully"
}
```

**Validation:**
- Range: 0.1 - 20.0 ml/s
- Automatically saves to FRAM
- Updates take effect immediately

## 🔧 Global Pump Control

### Get Pump Global State
```http
GET /api/pump-toggle
```

**Response (Enabled):**
```json
{
  "success": true,
  "enabled": true,
  "remaining_seconds": 0
}
```

**Response (Disabled):**
```json
{
  "success": true,
  "enabled": false,
  "remaining_seconds": 1654
}
```

### Toggle Pump Global State
```http
POST /api/pump-toggle
```

**Response:**
```json
{
  "success": true,
  "enabled": false,
  "message": "Pump disabled for 30 minutes",
  "remaining_seconds": 1800
}
```

**Behavior:**
- Disabling stops any running pump immediately
- Auto-enables after 30 minutes (1800 seconds)
- Manual re-enable cancels countdown

## 📈 Error Statistics

### Get Error Statistics
```http
GET /api/get-statistics
```

**Response:**
```json
{
  "success": true,
  "gap1_fail_sum": 15,
  "gap2_fail_sum": 3,
  "water_fail_sum": 1,
  "last_reset_timestamp": 1705329025,
  "last_reset_formatted": "15/01/2024 14:30"
}
```

**Field Descriptions:**
- `gap1_fail_sum`: Count of TIME_GAP_1 >= THRESHOLD_1 (slow drain detection)
- `gap2_fail_sum`: Count of TIME_GAP_2 >= THRESHOLD_2 (slow recovery)  
- `water_fail_sum`: Count of WATER_TRIGGER_TIME >= THRESHOLD_WATER (pump ineffective)
- `last_reset_timestamp`: Unix timestamp of last reset
- `last_reset_formatted`: Human-readable reset time

### Reset Error Statistics
```http
POST /api/reset-statistics
```

**Response:**
```json
{
  "success": true,
  "message": "Statistics reset successfully",
  "reset_timestamp": 1705332625
}
```

**Effects:**
- Sets all counters to zero
- Updates reset timestamp to current time
- **Logs reset event to VPS with FRAM device_id** (if configured)
- Cannot be undone

## 🏠 Web Pages

### Dashboard
```http
GET /
```
Returns HTML dashboard (requires authentication with **FRAM admin password**)

### Login Page  
```http
GET /login
```
Returns HTML login form

## 📡 Special Endpoints

### Algorithm Status (Aggregate)
```http
GET /status/aggregate
```

**Response:**
```json
{
  "aggregate": "15-03-01-0450",
  "state": "IDLE", 
  "daily_volume_ml": 450,
  "in_cycle": false,
  "device_id": "DOLEWKA",
  "credentials_loaded": true
}
```

**🆕 New Fields:**
- `device_id`: **Dynamic from FRAM** (or fallback device ID)
- `credentials_loaded`: Boolean indicating if FRAM credentials are in use

**Aggregate Format:** `XX-YY-ZZ-VVVV`
- `XX`: GAP1 failure count (last 14 days, max 99)
- `YY`: GAP2 failure count (last 14 days, max 99)
- `ZZ`: WATER failure count (last 14 days, max 99)  
- `VVVV`: Daily volume in ml (or ERR1/ERR2/ERR0/OVER)

**State Values:**
- `IDLE` - Monitoring sensors
- `TRYB_1_WAIT` - Waiting for second sensor
- `TRYB_1_DELAY` - Delay before pump start
- `TRYB_2_PUMP` - Pump running
- `TRYB_2_VERIFY` - Verifying sensor response
- `TRYB_2_WAIT_GAP2` - Measuring recovery time
- `LOGGING` - Recording cycle data
- `ERROR` - Error state (requires reset)
- `MANUAL_OVERRIDE` - Manual pump operation

## 🔒 **Security & Mode Information**

### System Mode Detection
**Note**: API endpoints are **ONLY available in Production Mode**. Programming Mode disables web server and API for security.

```http
GET /api/status
```
Response includes:
```json
{
  "system_mode": "PRODUCTION",
  "credentials_source": "FRAM"
}
```

**System Modes:**
- **`PRODUCTION`**: Full API access, web interface enabled, CLI disabled
- **`PROGRAMMING`**: API disabled, web interface disabled, CLI enabled for credential programming

### Credential Source Indicator

**FRAM Credentials Active:**
```json
{
  "credentials_source": "FRAM",
  "device_id": "DOLEWKA",
  "wifi_status": "Connected"
}
```

**Fallback Credentials Active:**
```json
{
  "credentials_source": "FALLBACK", 
  "device_id": "FALLBACK_DEVICE",
  "wifi_status": "Connected"
}
```

## 🚨 Error Codes

### HTTP Status Codes

| Code | Meaning | Description |
|------|---------|-------------|
| 200 | Success | Request completed successfully |
| 400 | Bad Request | Invalid parameters or malformed request |
| 401 | Unauthorized | Authentication required or session expired |
| 429 | Too Many Requests | Rate limit exceeded |
| 500 | Internal Server Error | Server-side error occurred |
| 503 | Service Unavailable | **Programming Mode active** - API disabled |

### Application Error Codes

```json
{
  "success": false,
  "error": "ERROR_CODE",
  "details": "Additional information"
}
```

**Common Error Codes:**
- `"Missing password"` - Login request without password
- `"Invalid password"` - **Authentication failed against FRAM stored hash**
- `"Session expired"` - Session timeout occurred
- `"Pump already running"` - Cannot start pump when active
- `"Value out of range"` - Parameter validation failed
- `"FRAM error"` - Storage operation failed
- `"Daily limit exceeded"` - Pump blocked due to volume limit
- **`"Credentials unavailable"`** - FRAM credential loading failed
- **`"Programming mode active"`** - API disabled in Programming Mode

## 🔄 Rate Limiting

### Limits Per IP Address

| Endpoint Category | Limit | Window | Block Duration |
|------------------|-------|--------|----------------|
| Login attempts | 10 failed | 1 minute | 1 minute |
| General requests | 5 requests | 1 second | None |
| Pump operations | 1 request | 2 seconds | None |

**Whitelisted IPs** (no rate limiting):
- Configured in `ALLOWED_IPS` array
- Typically local network admin devices

### Rate Limit Headers

Response headers when limits are approached:
```http
X-RateLimit-Limit: 5
X-RateLimit-Remaining: 2  
X-RateLimit-Reset: 1705329085
```

## 📝 Request/Response Examples

### **Dynamic Credential Workflow 🆕**

1. **Check credential source:**
```bash
curl -X GET http://192.168.0.164/api/status \
  --cookie "session_token=abc123..."
  
# Response shows credentials_source: "FRAM" or "FALLBACK"
```

2. **Login with FRAM admin password:**
```bash
curl -X POST http://192.168.0.164/api/login \
  -d "password=admin_password_from_fram"
  
# Note: Password verified against FRAM stored hash
```

### Complete Pump Cycle Workflow

1. **Check system status:**
```bash
curl -X GET http://192.168.0.164/api/status \
  --cookie "session_token=abc123..."
```

2. **Start pump if not running:**
```bash
curl -X POST http://192.168.0.164/api/pump/normal \
  --cookie "session_token=abc123..."
```

3. **Monitor progress:**
```bash
# Poll status until pump_remaining reaches 0
curl -X GET http://192.168.0.164/api/status \
  --cookie "session_token=abc123..."
```

### Statistics Management

1. **View current statistics:**
```bash
curl -X GET http://192.168.0.164/api/get-statistics \
  --cookie "session_token=abc123..."
```

2. **Reset if needed:**
```bash
curl -X POST http://192.168.0.164/api/reset-statistics \
  --cookie "session_token=abc123..."
```

## 🌐 VPS Integration

### **Dynamic Device Identification 🆕**

VPS logs now include **dynamic device ID from FRAM credentials**:

```json
{
  "device_id": "DOLEWKA",
  "timestamp": "2024-01-15 14:30:00",
  "event_type": "AUTO_CYCLE_COMPLETE",
  "volume_ml": 150,
  "credentials_source": "FRAM"
}
```

**VPS Authentication:**
- Uses **VPS token from FRAM credentials**
- Fallback to hardcoded token if FRAM unavailable
- Device identification via encrypted FRAM device name

### CORS & Headers

**Response Headers:**
```http
Content-Type: application/json
Cache-Control: no-cache, no-store, must-revalidate
Access-Control-Allow-Origin: *
X-Credentials-Source: FRAM
X-System-Mode: PRODUCTION
```

**Required Request Headers:**
```http
Content-Type: application/json  (for JSON requests)
Content-Type: application/x-www-form-urlencoded  (for form data)
Cookie: session_token=...  (for authenticated requests)
```

## 🔍 Debugging API Calls

### Enable Detailed Logging
Set in firmware configuration:
```cpp
#define ENABLE_FULL_LOGGING true
#define ENABLE_SERIAL_DEBUG true
```

### Monitor Serial Output (Production Mode Only)
```bash
pio device monitor -e production --baud 115200
```

### **Credential Debugging 🆕**
```bash
# Check credential loading at startup
pio device monitor -e production | grep -E "(FRAM|credentials|Device ID)"

# Expected startup messages:
# ✅ Credentials loaded from FRAM successfully  
# Device ID: DOLEWKA
# WiFi connected - Using FRAM credentials
```

### Common Debug Information
- Request IP address and endpoint
- **Authentication against FRAM stored hash**
- **Credential source (FRAM vs Fallback)**
- Parameter validation results
- Internal state changes
- Error conditions and recovery

### **Mode-Specific Debugging 🆕**

**Programming Mode** (No API):
```bash
pio device monitor -e programming
# CLI interface available
# Water system disabled
# API endpoints not available
```

**Production Mode** (Full API):
```bash
pio device monitor -e production  
# CLI interface disabled
# Water system enabled
# API endpoints available
# Dynamic credential loading
```

## 🛡️ **Security Notes**

### API Access Control
- **Programming Mode**: API completely disabled for security
- **Production Mode**: Full API access with FRAM authentication
- **Credential Updates**: Require switch to Programming Mode

### Authentication Security
- **Password hashing**: SHA-256 with dynamic salt
- **FRAM storage**: AES-256 encrypted credential storage
- **Session management**: Secure token-based authentication
- **Rate limiting**: Brute force protection

### Network Security
- **Dynamic WiFi**: Connects using FRAM stored credentials
- **VPS logging**: Uses FRAM stored authentication token  
- **Device identification**: Dynamic device ID from FRAM

---

**🔧 For Programming Mode CLI commands and credential management, see the [Hardware Setup Guide](hardware-setup.md) and [README](../README.md).**

**🔐 API is only available in Production Mode. Use Programming Mode for credential management via CLI interface.**