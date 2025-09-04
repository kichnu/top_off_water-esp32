#include "session_manager.h"
#include "../config/config.h"
#include "../core/logging.h"



std::vector<Session> activeSessions;

void initSessionManager() {
    activeSessions.clear();
    LOG_INFO("Session manager initialized");
}

void updateSessionManager() {
    unsigned long now = millis();
    
    for (auto it = activeSessions.begin(); it != activeSessions.end();) {
        if (!it->isValid || (now - it->lastActivity > SESSION_TIMEOUT_MS)) {
            LOG_INFO("Removing expired session for IP: %s", it->ip.toString().c_str());
            it = activeSessions.erase(it);
        } else {
            ++it;
        }
    }
}

String createSession(IPAddress ip) {
    // Remove existing sessions for this IP
    for (auto it = activeSessions.begin(); it != activeSessions.end();) {
        if (it->ip == ip) {
            it = activeSessions.erase(it);
        } else {
            ++it;
        }
    }
    
    Session newSession;
    newSession.token = "";
    for (int i = 0; i < 32; i++) {
        newSession.token += String(random(0, 16), HEX);
    }
    newSession.ip = ip;
    newSession.createdAt = millis();
    newSession.lastActivity = millis();
    newSession.isValid = true;
    
    activeSessions.push_back(newSession);
    
    LOG_INFO("Session created for IP: %s", ip.toString().c_str());
    return newSession.token;
}

bool validateSession(const String& token, IPAddress ip) {
    for (auto& session : activeSessions) {
        if (session.token == token && session.ip == ip && session.isValid) {
            if (millis() - session.lastActivity > SESSION_TIMEOUT_MS) {
                session.isValid = false;
                return false;
            }
            session.lastActivity = millis();
            return true;
        }
    }
    return false;
}

void destroySession(const String& token) {
    for (auto it = activeSessions.begin(); it != activeSessions.end(); ++it) {
        if (it->token == token) {
            activeSessions.erase(it);
            break;
        }
    }
}