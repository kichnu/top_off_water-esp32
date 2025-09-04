#include "wifi_manager.h"
#include "../config/config.h"
#include "../core/logging.h"
#include <WiFi.h>



unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 30000; // 30 seconds

void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.config(STATIC_IP, GATEWAY, SUBNET);
    
    LOG_INFO("Connecting to WiFi: %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        LOG_INFO("WiFi connected - IP: %s", WiFi.localIP().toString().c_str());
    } else {
        LOG_ERROR("WiFi connection failed");
    }
}

void updateWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > RECONNECT_INTERVAL) {
            LOG_WARNING("WiFi reconnecting...");
            WiFi.reconnect();
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