// #ifndef FRAM_ENCRYPTION_H
// #define FRAM_ENCRYPTION_H

// #include <Arduino.h>
// #include "aes.h"
// #include "sha256.h"

// // #include "../hardware/fram_controller.h"  // <-- DODAJ TĘ LINIĘ

// #include "../hardware/fram_constants.h"

// // Constants from FRAM programmer
// // #define ENCRYPTION_SALT "ESP32_FRAM_SALT_2024"
// // #define ENCRYPTION_SEED "WATER_SYSTEM_SEED_V1"

// // Maximum field sizes for encrypted data
// // #define MAX_DEVICE_NAME_LEN 31
// // #define MAX_WIFI_SSID_LEN 63
// // #define MAX_WIFI_PASSWORD_LEN 127
// // #define MAX_VPS_TOKEN_LEN 255

// // Structure definitions (compatible with FRAM programmer)
// struct DeviceCredentials {
//     String device_name;
//     String wifi_ssid;
//     String wifi_password;
//     String admin_password;    // Will be hashed to SHA-256 hex
//     String vps_token;
// };

// struct FRAMCredentials {
//     uint32_t magic;                     // 4 bytes
//     uint16_t version;                   // 2 bytes
//     char device_name[32];               // 32 bytes (plain text)
//     uint8_t iv[8];                      // 8 bytes
//     uint8_t encrypted_wifi_ssid[64];    // 64 bytes
//     uint8_t encrypted_wifi_password[128]; // 128 bytes
//     uint8_t encrypted_admin_hash[96];   // 96 bytes
//     uint8_t encrypted_vps_token[160];   // 160 bytes
//     uint8_t reserved[528];              // 528 bytes (padding to 1024)
//     uint16_t checksum;                  // 2 bytes
// };

// // Magic number and version (compatible with FRAM programmer)
// // #define FRAM_MAGIC_NUMBER 0x57415452   // "WATR"
// // #define FRAM_DATA_VERSION 0x0001

// // Core encryption functions
// bool generateEncryptionKey(const String& device_name, uint8_t* key);
// bool generateRandomIV(uint8_t* iv);
// bool encryptData(const uint8_t* plaintext, size_t plaintext_len,
//                  const uint8_t* key, const uint8_t* iv,
//                  uint8_t* ciphertext, size_t* ciphertext_len);
// bool decryptData(const uint8_t* ciphertext, size_t ciphertext_len,
//                  const uint8_t* key, const uint8_t* iv,
//                  uint8_t* plaintext, size_t* plaintext_len);

// // PKCS7 padding functions
// size_t addPKCS7Padding(uint8_t* data, size_t data_len, size_t block_size);
// size_t removePKCS7Padding(const uint8_t* data, size_t data_len);

// // High-level credential functions
// bool encryptCredentials(const DeviceCredentials& creds, FRAMCredentials& fram_creds);
// bool decryptCredentials(const FRAMCredentials& fram_creds, DeviceCredentials& creds);

// // Validation functions
// bool validateDeviceName(const String& name);
// bool validateWiFiSSID(const String& ssid);
// bool validateWiFiPassword(const String& password);
// bool validateVPSToken(const String& token);

// // Hash functions
// bool sha256Hash(const String& input, uint8_t* hash);
// bool sha256Hash(const uint8_t* data, size_t len, uint8_t* hash);

// // Checksum calculation
// uint16_t calculateChecksum(const uint8_t* data, size_t size);

// #endif




#ifndef FRAM_ENCRYPTION_H
#define FRAM_ENCRYPTION_H

#include <Arduino.h>
#include "aes.h"
#include "sha256.h"
#include "../hardware/fram_constants.h"

// Structure definitions (compatible with FRAM programmer)
struct DeviceCredentials {
    String device_name;
    String wifi_ssid;
    String wifi_password;
    String admin_password;    // Will be hashed to SHA-256 hex
    String vps_token;
    String vps_url;          // 🆕 NEW: VPS URL
};

struct FRAMCredentials {
    uint32_t magic;                     // 4 bytes
    uint16_t version;                   // 2 bytes - NOW VERSION 3
    char device_name[32];               // 32 bytes (plain text)
    uint8_t iv[8];                      // 8 bytes
    uint8_t encrypted_wifi_ssid[64];    // 64 bytes
    uint8_t encrypted_wifi_password[128]; // 128 bytes
    uint8_t encrypted_admin_hash[96];   // 96 bytes
    uint8_t encrypted_vps_token[160];   // 160 bytes
    uint8_t encrypted_vps_url[128];     // 128 bytes 🆕 NEW
    uint8_t reserved[400];              // 400 bytes (reduced from 528)
    uint16_t checksum;                  // 2 bytes
};

// Core encryption functions
bool generateEncryptionKey(const String& device_name, uint8_t* key);
bool generateRandomIV(uint8_t* iv);
bool encryptData(const uint8_t* plaintext, size_t plaintext_len,
                 const uint8_t* key, const uint8_t* iv,
                 uint8_t* ciphertext, size_t* ciphertext_len);
bool decryptData(const uint8_t* ciphertext, size_t ciphertext_len,
                 const uint8_t* key, const uint8_t* iv,
                 uint8_t* plaintext, size_t* plaintext_len);

// PKCS7 padding functions
size_t addPKCS7Padding(uint8_t* data, size_t data_len, size_t block_size);
size_t removePKCS7Padding(const uint8_t* data, size_t data_len);

// High-level credential functions
bool encryptCredentials(const DeviceCredentials& creds, FRAMCredentials& fram_creds);
bool decryptCredentials(const FRAMCredentials& fram_creds, DeviceCredentials& creds);

// Validation functions
bool validateDeviceName(const String& name);
bool validateWiFiSSID(const String& ssid);
bool validateWiFiPassword(const String& password);
bool validateVPSToken(const String& token);
bool validateVPSURL(const String& url);        // 🆕 NEW

// Hash functions
bool sha256Hash(const String& input, uint8_t* hash);
bool sha256Hash(const uint8_t* data, size_t len, uint8_t* hash);

// Checksum calculation
uint16_t calculateChecksum(const uint8_t* data, size_t size);

#endif