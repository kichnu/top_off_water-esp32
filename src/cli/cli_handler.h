#ifndef CLI_HANDLER_H
#define CLI_HANDLER_H

#include <Arduino.h>
#include "../mode_config.h"

#if ENABLE_CLI_INTERFACE

// CLI Commands
enum CLICommand {
    CMD_HELP = 0,
    CMD_DETECT,
    CMD_INFO,
    CMD_BACKUP,
    CMD_RESTORE,
    CMD_PROGRAM,
    CMD_VERIFY,
    CMD_CONFIG,
    CMD_TEST,
    CMD_UNKNOWN
};

// CLI functions
void initCLI();
void handleCLI();
CLICommand parseCommand(const String& input);
void executeCommand(CLICommand cmd, const String& args);

// Command implementations
void cmdHelp();
void cmdDetect();
void cmdInfo();
void cmdBackup();
void cmdRestore();
void cmdProgram();
void cmdVerify();
void cmdConfig();
void cmdTest();

// Utility functions
String readSerialLine();
void printPrompt();
void printSuccess(const String& message);
void printError(const String& message);
void printWarning(const String& message);
void printInfo(const String& message);
void printHexDump(const uint8_t* data, size_t length);

// JSON parsing
bool parseJSONCredentials(const String& json, struct DeviceCredentials& creds);

#else
// CLI disabled - empty inline functions
inline void initCLI() {}
inline void handleCLI() {}
#endif

#endif