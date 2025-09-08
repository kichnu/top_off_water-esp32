// #ifndef WEB_SERVER_H
// #define WEB_SERVER_H

// #include <ESPAsyncWebServer.h>

// void initWebServer();
// bool checkAuthentication(AsyncWebServerRequest* request);

// #endif






#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "../mode_config.h"

#if ENABLE_WEB_SERVER
    #include <ESPAsyncWebServer.h>

    void initWebServer();
    bool checkAuthentication(AsyncWebServerRequest* request);
#else
    // Web server disabled in this mode
    inline void initWebServer() {}
#endif

#endif

