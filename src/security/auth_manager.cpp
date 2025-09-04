#include "auth_manager.h"
#include "../config/config.h"
#include "../core/logging.h"
#include <mbedtls/md.h>



void initAuthManager() {
    LOG_INFO("Authentication manager initialized");
}

bool isIPAllowed(IPAddress ip) {
    for (int i = 0; i < ALLOWED_IPS_COUNT; i++) {
        if (ALLOWED_IPS[i] == ip) {
            return true;
        }
    }
    return false;
}

String hashPassword(const String& password) {
    byte hash[32];
    
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char*)password.c_str(), password.length());
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);
    
    String hashString = "";
    for (int i = 0; i < 32; i++) {
        char str[3];
        sprintf(str, "%02x", (int)hash[i]);
        hashString += str;
    }
    
    return hashString;
}

bool verifyPassword(const String& password) {
    String inputHash = hashPassword(password);
    bool valid = (inputHash == ADMIN_PASSWORD_HASH);
    
    if (valid) {
        LOG_INFO("Password verification successful");
    } else {
        LOG_WARNING("Password verification failed");
    }
    
    return valid;
}