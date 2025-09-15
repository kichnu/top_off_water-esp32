
#include "wifi_manager.h"
#include "../config/config.h"
#include "../core/logging.h"
#include <WiFi.h>

#if MODE_PRODUCTION
    #include "../config/credentials_manager.h"
#endif

unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 30000; // 30 seconds

void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.config(STATIC_IP, GATEWAY, SUBNET);
    
#if MODE_PRODUCTION
    // Production mode: use dynamic credentials
    const char* ssid = getWiFiSSID();
    const char* password = getWiFiPassword();
    
    LOG_INFO("Connecting to WiFi: %s", ssid);
    LOG_INFO("Credentials source: %s", areCredentialsLoaded() ? "FRAM" : "Hardcoded fallback");
#else
    // Programming mode: use hardcoded credentials
    const char* ssid = WIFI_SSID;
    const char* password = WIFI_PASSWORD;
    
    LOG_INFO("Connecting to WiFi: %s (hardcoded - programming mode)", ssid);
#endif
    
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
        LOG_INFO("WiFi connection attempt %d/20", attempts);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        LOG_INFO("WiFi connected - IP: %s", WiFi.localIP().toString().c_str());
#if MODE_PRODUCTION
        LOG_INFO("Using %s credentials", areCredentialsLoaded() ? "FRAM" : "fallback");
#else
        LOG_INFO("Using hardcoded credentials (programming mode)");
#endif
    } else {
        LOG_ERROR("WiFi connection failed after 20 attempts");
#if MODE_PRODUCTION
        if (!areCredentialsLoaded()) {
            LOG_ERROR("Consider programming correct credentials via FRAM programmer");
        }
#endif
    }
}

void updateWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > RECONNECT_INTERVAL) {
            LOG_WARNING("WiFi reconnecting...");
            
#if MODE_PRODUCTION
            // Use dynamic credentials for reconnection
            WiFi.begin(getWiFiSSID(), getWiFiPassword());
#else
            // Use hardcoded credentials
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#endif
            lastReconnectAttempt = now;
        }
    }
}

bool isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String getWiFiStatus() {
    switch (WiFi.status()) {
        case WL_CONNECTED: return "Connected";
        case WL_NO_SSID_AVAIL: return "SSID not found";
        case WL_CONNECT_FAILED: return "Connection failed";
        case WL_CONNECTION_LOST: return "Connection lost";
        case WL_DISCONNECTED: return "Disconnected";
        default: return "Unknown";
    }
}

IPAddress getLocalIP() {
    return WiFi.localIP();
}
// ================= src/network/vps_logger.h =================
#ifndef VPS_LOGGER_H
#define VPS_LOGGER_H

#include <Arduino.h>

void initVPSLogger();
bool logEventToVPS(const String& eventType, uint16_t volumeML, const String& timestamp);

#endif