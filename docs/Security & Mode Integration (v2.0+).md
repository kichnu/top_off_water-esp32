---

## 🔐 Security & Mode Integration (v2.0+)

### Dual-Mode Operation

**Programming Mode:**
- Water algorithm **DISABLED** for security
- CLI interface available for credential programming
- FRAM credential storage and encryption
- No pump operations allowed

**Production Mode:**
- Water algorithm **ENABLED** and fully operational
- CLI interface disabled for security
- Dynamic credential loading from FRAM
- All features described in this document active

### Dynamic Configuration

**Device Identification:**
- Device ID loaded dynamically from FRAM encrypted storage
- Fallback to hardcoded ID if FRAM unavailable
- VPS logging uses dynamic device identification

**Network Integration:**
- WiFi credentials loaded from FRAM
- VPS authentication tokens from FRAM
- Automatic fallback to hardcoded credentials if needed

**Security Model:**
- Mode switching requires firmware upload (physical access)
- Conditional compilation eliminates unused code
- FRAM credentials protected with AES-256 encryption

---

**📊 The algorithm continuously learns from sensor patterns and pump performance to optimize water management while preventing system failures and ensuring reliable operation. v2.0+ adds secure credential management without affecting core algorithm performance.**