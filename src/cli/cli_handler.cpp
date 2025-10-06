#include "cli_handler.h"
#include "../mode_config.h"

#if ENABLE_CLI_INTERFACE

#include "../hardware/fram_controller.h"
#include "../crypto/fram_encryption.h"
#include "../hardware/rtc_controller.h"
#include "../core/logging.h"
#include <ArduinoJson.h>
#include <Wire.h>

// CLI state
static String inputBuffer = "";
static bool waitingForInput = false;

void initCLI() {
    Serial.println("✅ CLI initialized - Programming Mode");
    inputBuffer.reserve(1024); // Reserve space for input buffer
    printPrompt();
}

void handleCLI() {
    if (Serial.available()) {
        char c = Serial.read();
        
        if (c == '\n' || c == '\r') {
            if (inputBuffer.length() > 0) {
                // Process command
                inputBuffer.trim();
                
                CLICommand cmd = parseCommand(inputBuffer);
                executeCommand(cmd, inputBuffer);
                
                inputBuffer = "";
                if (!waitingForInput) {
                    printPrompt();
                }
            }
        } else if (c == '\b' || c == 127) { // Backspace
            if (inputBuffer.length() > 0) {
                inputBuffer.remove(inputBuffer.length() - 1);
                Serial.print("\b \b");
            }
        } else if (c >= 32 && c < 127) { // Printable characters
            inputBuffer += c;
            Serial.print(c);
        }
    }
}

CLICommand parseCommand(const String& input) {
    String cmd = input;
    int spaceIndex = cmd.indexOf(' ');
    if (spaceIndex > 0) {
        cmd = cmd.substring(0, spaceIndex);
    }
    cmd.toLowerCase();
    
    if (cmd == "help" || cmd == "h") return CMD_HELP;
    if (cmd == "detect" || cmd == "d") return CMD_DETECT;
    if (cmd == "info" || cmd == "i") return CMD_INFO;
    if (cmd == "backup" || cmd == "b") return CMD_BACKUP;
    if (cmd == "restore" || cmd == "r") return CMD_RESTORE;
    if (cmd == "program" || cmd == "p") return CMD_PROGRAM;
    if (cmd == "verify" || cmd == "v") return CMD_VERIFY;
    if (cmd == "config" || cmd == "c") return CMD_CONFIG;
    if (cmd == "test" || cmd == "t") return CMD_TEST;
    
    return CMD_UNKNOWN;
}

void executeCommand(CLICommand cmd, const String& args) {
    Serial.println(); // New line after command
    
    switch (cmd) {
        case CMD_HELP:      cmdHelp(); break;
        case CMD_DETECT:    cmdDetect(); break;
        case CMD_INFO:      cmdInfo(); break;
        case CMD_BACKUP:    cmdBackup(); break;
        case CMD_RESTORE:   cmdRestore(); break;
        case CMD_PROGRAM:   cmdProgram(); break;
        case CMD_VERIFY:    cmdVerify(); break;
        case CMD_CONFIG:    cmdConfig(); break;
        case CMD_TEST:      cmdTest(); break;
        case CMD_UNKNOWN:
        default:
            printError("Unknown command. Type 'help' for available commands.");
            break;
    }
}

void cmdHelp() {
    Serial.println("ESP32-C3 FRAM Programmer Commands:");
    Serial.println("==================================");
    Serial.println("  help (h)     - Show this help");
    Serial.println("  detect (d)   - Detect FRAM device");
    Serial.println("  info (i)     - Show FRAM information");
    Serial.println("  backup (b)   - Backup entire FRAM content");
    Serial.println("  restore (r)  - Restore FRAM from backup");
    Serial.println("  program (p)  - Program credentials to FRAM");
    Serial.println("  verify (v)   - Verify stored credentials");
    Serial.println("  config (c)   - Configure via JSON input");
    Serial.println("  test (t)     - Test FRAM read/write");
    Serial.println();
    Serial.println("Examples:");
    Serial.println("  program      - Interactive credential input");
    Serial.println("  config       - JSON configuration mode");
    Serial.println("  backup       - Creates hex dump for external storage");
    Serial.println();
    Serial.println("🔒 Programming Mode Features:");
    Serial.println("  • Full CLI access via Serial");
    Serial.println("  • FRAM credential programming");
    Serial.println("  • Water system functions DISABLED");
}

void cmdDetect() {
    Serial.print("Scanning for FRAM... ");
    
    // I2C scan for FRAM
    Wire.beginTransmission(0x50);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
        printSuccess("FRAM detected at address 0x50");
        
        // Try to read a test byte
        Wire.beginTransmission(0x50);
        Wire.write(0x00);  // Address high byte
        Wire.write(0x00);  // Address low byte
        Wire.endTransmission();
        
        Wire.requestFrom(0x50, 1);
        if (Wire.available()) {
            uint8_t testByte = Wire.read();
            Serial.print("First byte: 0x");
            if (testByte < 16) Serial.print("0");
            Serial.println(testByte, HEX);
        }
    } else {
        printError("FRAM not found");
        
        // I2C scan for any devices  
        Serial.println("Scanning I2C bus for any devices:");
        bool found = false;
        
        for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
            Wire.beginTransmission(addr);
            uint8_t scan_error = Wire.endTransmission();
            
            if (scan_error == 0) {
                Serial.print("  Device found at address 0x");
                if (addr < 16) Serial.print("0");
                Serial.println(addr, HEX);
                found = true;
            }
        }
        
        if (!found) {
            Serial.println("  No I2C devices found!");
        }
    }
}

void cmdInfo() {
    Serial.println("FRAM Information:");
    Serial.println("================");
    Serial.print("I2C Address: 0x50");
    Serial.println();
    Serial.print("Credentials Address: 0x");
    Serial.println(FRAM_CREDENTIALS_ADDR, HEX);
    Serial.print("Credentials Size: ");
    Serial.print(FRAM_CREDENTIALS_SIZE);
    Serial.println(" bytes");
    Serial.print("Magic Number: 0x");
    Serial.println(FRAM_MAGIC_NUMBER, HEX);
    Serial.print("Data Version: ");
    Serial.println(FRAM_DATA_VERSION);
    
    // Check if credentials are present
    if (verifyCredentialsInFRAM()) {
        printSuccess("Valid credentials found in FRAM");
        
        // Try to read device name
        FRAMCredentials creds;
        if (readCredentialsFromFRAM(creds)) {
            Serial.print("Device Name: ");
            Serial.println(creds.device_name);
        }
    } else {
        printWarning("No valid credentials found in FRAM");
    }
    
    Serial.print("RTC Status: ");
    Serial.println(getRTCInfo());
    Serial.print("Current Time: ");
    Serial.println(getCurrentTimestamp());
}

void cmdBackup() {
    printInfo("Starting FRAM backup (32KB)");
    Serial.println("Copy the following output to save your backup:");
    Serial.println();
    
    const size_t fram_size = 32768;  // 32KB
    const size_t chunk_size = 16;    // 16 bytes per line
    
    Serial.println("BACKUP_START");
    Serial.print("SIZE:");
    Serial.println(fram_size);
    
    for (size_t addr = 0; addr < fram_size; addr += chunk_size) {
        // Read chunk
        Wire.beginTransmission(0x50);
        Wire.write((addr >> 8) & 0xFF);  // Address high byte
        Wire.write(addr & 0xFF);         // Address low byte
        Wire.endTransmission();
        
        Wire.requestFrom(0x50, (uint8_t)chunk_size);
        
        // Print address
        Serial.print(":");
        if (addr < 0x1000) Serial.print("0");
        if (addr < 0x100) Serial.print("0");
        if (addr < 0x10) Serial.print("0");
        Serial.print(addr, HEX);
        Serial.print(":");
        
        // Print data
        for (size_t i = 0; i < chunk_size && Wire.available(); i++) {
            uint8_t byte = Wire.read();
            if (byte < 16) Serial.print("0");
            Serial.print(byte, HEX);
        }
        Serial.println();
        
        delay(1); // Small delay for serial processing
    }
    
    Serial.println("BACKUP_END");
    printInfo("Backup complete. Save the hex dump above for restore.");
}

void cmdRestore() {
    printWarning("FRAM restore will overwrite ALL data!");
    Serial.print("Type 'YES' to confirm: ");
    
    waitingForInput = true;
    String confirmation = readSerialLine();
    waitingForInput = false;
    
    if (confirmation != "YES") {
        printInfo("Restore cancelled");
        return;
    }
    
    printWarning("Hex restore not fully implemented in this version");
    printInfo("Please use FRAM programmer device for complete restore");
}

void cmdProgram() {
    printInfo("=== Interactive Credential Programming ===");
    
    DeviceCredentials creds;
    
    // Get device name
    Serial.print("Device Name (1-31 chars, alphanumeric + _): ");
    waitingForInput = true;
    creds.device_name = readSerialLine();
    waitingForInput = false;
    
    if (!validateDeviceName(creds.device_name)) {
        printError("Invalid device name");
        return;
    }
    
    // Get WiFi SSID
    Serial.print("WiFi SSID (1-63 chars): ");
    waitingForInput = true;
    creds.wifi_ssid = readSerialLine();
    waitingForInput = false;
    
    if (!validateWiFiSSID(creds.wifi_ssid)) {
        printError("Invalid WiFi SSID");
        return;
    }
    
    // Get WiFi password
    Serial.print("WiFi Password (1-127 chars): ");
    waitingForInput = true;
    creds.wifi_password = readSerialLine();
    waitingForInput = false;
    
    if (!validateWiFiPassword(creds.wifi_password)) {
        printError("Invalid WiFi password");
        return;
    }
    
    // Get admin password
    Serial.print("Admin Password: ");
    waitingForInput = true;
    creds.admin_password = readSerialLine();
    waitingForInput = false;
    
    if (creds.admin_password.length() == 0) {
        printError("Admin password cannot be empty");
        return;
    }
    
    // Get VPS token
    Serial.print("VPS Token: ");
    waitingForInput = true;
    creds.vps_token = readSerialLine();
    waitingForInput = false;
    
    if (!validateVPSToken(creds.vps_token)) {
        printError("Invalid VPS token");
        return;
    }
    
    // 🆕 NEW: Get VPS URL
    Serial.print("VPS URL (e.g. http://server:5000/api/water-events): ");
    waitingForInput = true;
    creds.vps_url = readSerialLine();
    waitingForInput = false;
    
    if (!validateVPSURL(creds.vps_url)) {
        printError("Invalid VPS URL - must start with http:// or https://");
        return;
    }
    
    // Confirm programming
    Serial.println();
    Serial.println("=== CREDENTIALS SUMMARY ===");
    Serial.print("Device Name: "); Serial.println(creds.device_name);
    Serial.print("WiFi SSID: "); Serial.println(creds.wifi_ssid);
    Serial.print("WiFi Password: "); Serial.println("******* (hidden)");
    Serial.print("Admin Password: "); Serial.println("******* (will be hashed)");
    Serial.print("VPS Token: "); Serial.println(creds.vps_token);
    Serial.print("VPS URL: "); Serial.println(creds.vps_url);  // 🆕 NEW
    Serial.println();
    
    Serial.print("Program these credentials to FRAM? (YES/no): ");
    waitingForInput = true;
    String confirm = readSerialLine();
    waitingForInput = false;
    
    if (confirm == "YES" || confirm == "yes" || confirm == "y" || confirm == "") {
        // Encrypt and write credentials
        FRAMCredentials fram_creds;
        if (encryptCredentials(creds, fram_creds)) {
            if (writeCredentialsToFRAM(fram_creds)) {
                printSuccess("Credentials programmed successfully!");
                Serial.println("🆕 NOTE: This includes VPS URL - device will use dynamic VPS endpoint");
            } else {
                printError("Failed to write credentials to FRAM");
            }
        } else {
            printError("Failed to encrypt credentials");
        }
    } else {
        printInfo("Programming cancelled");
    }
}


void cmdVerify() {
    printInfo("Verifying FRAM credentials...");
    
    if (verifyCredentialsInFRAM()) {
        printSuccess("Credentials verification PASSED");
        
        // Try to decrypt and show info
        FRAMCredentials fram_creds;
        if (readCredentialsFromFRAM(fram_creds)) {
            DeviceCredentials creds;
            if (decryptCredentials(fram_creds, creds)) {
                Serial.println();
                Serial.println("=== DECRYPTED CREDENTIALS ===");
                Serial.print("FRAM Version: "); Serial.println(fram_creds.version);
                Serial.print("Device Name: "); Serial.println(creds.device_name);
                Serial.print("WiFi SSID: "); Serial.println(creds.wifi_ssid);
                Serial.print("WiFi Password: "); Serial.println("******* (hidden)");
                Serial.print("Admin Hash: "); Serial.println(creds.admin_password); // This is the hash
                Serial.print("VPS Token: "); Serial.println(creds.vps_token);
                Serial.print("VPS URL: "); 
                if (creds.vps_url.length() > 0) {
                    Serial.println(creds.vps_url);  // 🆕 NEW
                } else {
                    Serial.println("(not set - using fallback)");
                }
            } else {
                printWarning("Could not decrypt credentials (key derivation issue?)");
            }
        }
    } else {
        printError("Credentials verification FAILED");
    }
}

void cmdConfig() {
    printInfo("=== JSON Configuration Mode ===");
    Serial.println("Paste JSON configuration below (single line):");
    Serial.println("Format:");
    Serial.println("{");
    Serial.println("  \"device_name\": \"DOLEWKA\",");
    Serial.println("  \"wifi_ssid\": \"MyNetwork\",");
    Serial.println("  \"wifi_password\": \"MyPassword\",");
    Serial.println("  \"admin_password\": \"admin123\",");
    Serial.println("  \"vps_token\": \"YourVpsToken\",");
    Serial.println("  \"vps_url\": \"YourVpsUrl\""); 
    Serial.println("}");
    Serial.println();
    Serial.print("JSON: ");
    
    waitingForInput = true;
    String jsonInput = readSerialLine();
    waitingForInput = false;
    
    DeviceCredentials creds;
    if (parseJSONCredentials(jsonInput, creds)) {
        Serial.println();
        Serial.println("=== PARSED CREDENTIALS ===");
        Serial.print("Device Name: "); Serial.println(creds.device_name);
        Serial.print("WiFi SSID: "); Serial.println(creds.wifi_ssid);
        Serial.print("WiFi Password: "); Serial.println("******* (hidden)");
        Serial.print("Admin Password: "); Serial.println("******* (will be hashed)");
        Serial.print("VPS Token: "); Serial.println(creds.vps_token);
        Serial.print("VPS URL: "); Serial.println(creds.vps_url);      // 🆕 NEW
        Serial.println();
        
        Serial.print("Program these credentials? (YES/no): ");
        waitingForInput = true;
        String confirm = readSerialLine();
        waitingForInput = false;
        
        if (confirm == "YES" || confirm == "yes" || confirm == "y" || confirm == "") {
            // Encrypt and write credentials
            FRAMCredentials fram_creds;
            if (encryptCredentials(creds, fram_creds)) {
                if (writeCredentialsToFRAM(fram_creds)) {
                    printSuccess("JSON credentials programmed successfully!");
                } else {
                    printError("Failed to write credentials to FRAM");
                }
            } else {
                printError("Failed to encrypt credentials");
            }
        } else {
            printInfo("Programming cancelled");
        }
    } else {
        printError("Invalid JSON format or missing required fields");
    }
}

void cmdTest() {
    printInfo("=== FRAM Test Sequence ===");
    
    cmdDetect(); // Run detection first
    
    Serial.println();
    Serial.println("Test 1: Basic Read/Write");
    
    // Test basic read/write
    uint8_t test_data[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                             0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t read_data[16];
    
    // Write test data to safe area (0x7F00)
    Wire.beginTransmission(0x50);
    Wire.write(0x7F);  // Address high byte
    Wire.write(0x00);  // Address low byte
    for (int i = 0; i < 16; i++) {
        Wire.write(test_data[i]);
    }
    Wire.endTransmission();
    
    delay(10); // Small delay for write completion
    
    // Read back test data
    Wire.beginTransmission(0x50);
    Wire.write(0x7F);  // Address high byte  
    Wire.write(0x00);  // Address low byte
    Wire.endTransmission();
    
    Wire.requestFrom(0x50, 16);
    for (int i = 0; i < 16 && Wire.available(); i++) {
        read_data[i] = Wire.read();
    }
    
    bool test1_pass = (memcmp(test_data, read_data, 16) == 0);
    Serial.print("  Result: ");
    if (test1_pass) {
        printSuccess("PASS");
    } else {
        printError("FAIL - Data mismatch");
    }
    
    Serial.println();
    Serial.println("Test 2: Encryption/Decryption");
    
    String test_string = "Hello, ESP32-C3!";
    uint8_t key[32] = {0}; // Test key
    uint8_t iv[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    
    uint8_t encrypted[64];
    size_t enc_len = 64;
    bool enc_ok = encryptData((uint8_t*)test_string.c_str(), test_string.length(),
                              key, iv, encrypted, &enc_len);
    
    uint8_t decrypted[64];
    size_t dec_len = 64;
    bool dec_ok = decryptData(encrypted, enc_len, key, iv, decrypted, &dec_len);
    
    decrypted[dec_len] = '\0';
    String decrypted_string = String((char*)decrypted);
    
    bool test2_pass = (enc_ok && dec_ok && decrypted_string == test_string);
    Serial.print("  Original: "); Serial.println(test_string);
    Serial.print("  Decrypted: "); Serial.println(decrypted_string);
    Serial.print("  Result: ");
    if (test2_pass) {
        printSuccess("PASS");
    } else {
        printError("FAIL - Encryption test failed");
    }
    
    // Summary
    Serial.println();
    Serial.print("=== TEST SUMMARY: ");
    if (test1_pass && test2_pass) {
        printSuccess("ALL TESTS PASSED");
    } else {
        printError("SOME TESTS FAILED");
    }
}


bool parseJSONCredentials(const String& json, DeviceCredentials& creds) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.print("JSON parsing error: ");
        Serial.println(error.c_str());
        return false;
    }
    
    // Check required fields using modern ArduinoJson syntax
    if (!doc["device_name"].is<String>() || !doc["wifi_ssid"].is<String>() ||
        !doc["wifi_password"].is<String>() || !doc["admin_password"].is<String>() ||
        !doc["vps_token"].is<String>()) {
        Serial.println("Missing required JSON fields (device_name, wifi_ssid, wifi_password, admin_password, vps_token)");
        return false;
    }
    
    creds.device_name = doc["device_name"].as<String>();
    creds.wifi_ssid = doc["wifi_ssid"].as<String>();
    creds.wifi_password = doc["wifi_password"].as<String>();
    creds.admin_password = doc["admin_password"].as<String>();
    creds.vps_token = doc["vps_token"].as<String>();
    
    // 🆕 NEW: VPS URL is optional for backward compatibility
    if (doc["vps_url"].is<String>()) {
        creds.vps_url = doc["vps_url"].as<String>();
    } else {
        creds.vps_url = "http://localhost:5000/api/water-events"; // Default fallback
        Serial.println("Warning: vps_url not provided, using default");
    }
    
    // Validate all fields
    return validateDeviceName(creds.device_name) &&
           validateWiFiSSID(creds.wifi_ssid) &&
           validateWiFiPassword(creds.wifi_password) &&
           creds.admin_password.length() > 0 &&
           validateVPSToken(creds.vps_token) &&
           validateVPSURL(creds.vps_url);        // 🆕 NEW
}

String readSerialLine() {
    String input = "";
    
    while (true) {
        if (Serial.available()) {
            char c = Serial.read();
            
            if (c == '\n' || c == '\r') {
                if (input.length() > 0) {
                    Serial.println();
                    return input;
                }
            } else if (c == '\b' || c == 127) { // Backspace
                if (input.length() > 0) {
                    input.remove(input.length() - 1);
                    Serial.print("\b \b");
                }
            } else if (c >= 32 && c < 127) { // Printable characters
                input += c;
                Serial.print(c);
            }
        }
        delay(1);
    }
}

void printPrompt() {
    Serial.print("FRAM> ");
}

void printSuccess(const String& message) {
    Serial.print("[SUCCESS] ");
    Serial.println(message);
}

void printError(const String& message) {
    Serial.print("[ERROR] ");
    Serial.println(message);
}

void printWarning(const String& message) {
    Serial.print("[WARNING] ");
    Serial.println(message);
}

void printInfo(const String& message) {
    Serial.print("[INFO] ");
    Serial.println(message);
}

void printHexDump(const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (i % 16 == 0) {
            Serial.print(i, HEX);
            Serial.print(": ");
        }
        
        if (data[i] < 16) Serial.print("0");
        Serial.print(data[i], HEX);
        Serial.print(" ");
        
        if ((i + 1) % 16 == 0 || i == length - 1) {
            Serial.println();
        }
    }
}

#endif // ENABLE_CLI_INTERFACE



