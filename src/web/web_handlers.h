


#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include "../mode_config.h"

#if ENABLE_WEB_SERVER
    #include <ESPAsyncWebServer.h>

    // Page handlers
    void handleDashboard(AsyncWebServerRequest* request);
    void handleLoginPage(AsyncWebServerRequest* request);

    // Authentication handlers
    void handleLogin(AsyncWebServerRequest* request);
    void handleLogout(AsyncWebServerRequest* request);

    // API handlers
    void handleStatus(AsyncWebServerRequest* request);
    void handlePumpNormal(AsyncWebServerRequest* request);
    void handlePumpExtended(AsyncWebServerRequest* request);
    void handlePumpStop(AsyncWebServerRequest* request);
    void handlePumpSettings(AsyncWebServerRequest* request);
    void handlePumpToggle(AsyncWebServerRequest* request);

    // Statistics handlers
    void handleResetStatistics(AsyncWebServerRequest* request);
    void handleGetStatistics(AsyncWebServerRequest* request);
#endif

#endif