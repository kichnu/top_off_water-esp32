#ifndef MODE_CONFIG_H
#define MODE_CONFIG_H

// ===============================
// DUAL MODE CONFIGURATION
// ===============================

#ifdef PROGRAMMING_MODE
    #define MODE_PROGRAMMING 1
    #define MODE_PRODUCTION 0
    #define MODE_NAME "PROGRAMMING"
#elif defined(PRODUCTION_MODE)
    #define MODE_PROGRAMMING 0
    #define MODE_PRODUCTION 1
    #define MODE_NAME "PRODUCTION"
#else
    #error "No mode defined! Use -DPROGRAMMING_MODE or -DPRODUCTION_MODE"
#endif

// Feature flags based on mode
#if MODE_PROGRAMMING
    #define ENABLE_CLI_INTERFACE 1
    #define ENABLE_WATER_SYSTEM 0
    #define ENABLE_WEB_SERVER 0
    #define ENABLE_VPS_LOGGER 0
    #define ENABLE_WIFI_MANAGER 0
#else
    #define ENABLE_CLI_INTERFACE 0
    #define ENABLE_WATER_SYSTEM 1
    #define ENABLE_WEB_SERVER 1
    #define ENABLE_VPS_LOGGER 1
    #define ENABLE_WIFI_MANAGER 1
#endif

#endif