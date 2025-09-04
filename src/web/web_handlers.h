// #ifndef WEB_HANDLERS_H
// #define WEB_HANDLERS_H

// #include <ESPAsyncWebServer.h>

// void handleDashboard(AsyncWebServerRequest* request);
// void handleLoginPage(AsyncWebServerRequest* request);
// void handleLogin(AsyncWebServerRequest* request);
// void handleLogout(AsyncWebServerRequest* request);
// void handlePumpNormal(AsyncWebServerRequest* request);
// void handlePumpExtended(AsyncWebServerRequest* request);
// void handlePumpStop(AsyncWebServerRequest* request);
// void handlePumpSettings(AsyncWebServerRequest* request);
// void handlePumpToggle(AsyncWebServerRequest* request);
// void handleResetStatistics(AsyncWebServerRequest* request);
// void handleGetStatistics(AsyncWebServerRequest* request);

// void handleStatusAggregate(AsyncWebServerRequest* request);
// void handleStatus(AsyncWebServerRequest* request);


// #endif


#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

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

// Statistics handlers (NEW)
void handleResetStatistics(AsyncWebServerRequest* request);
void handleGetStatistics(AsyncWebServerRequest* request);

#endif