// #include "auth_manager.h"
// #include "../config/config.h"
// #include "../config/credentials_manager.h" 
// #include "../core/logging.h"
// #include <mbedtls/md.h>



// void initAuthManager() {
//     LOG_INFO("Authentication manager initialized");
// }

// bool isIPAllowed(IPAddress ip) {
//     for (int i = 0; i < ALLOWED_IPS_COUNT; i++) {
//         if (ALLOWED_IPS[i] == ip) {
//             return true;
//         }
//     }
//     return false;
// }

// String hashPassword(const String& password) {
//     byte hash[32];
    
//     mbedtls_md_context_t ctx;
//     mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    
//     mbedtls_md_init(&ctx);
//     mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
//     mbedtls_md_starts(&ctx);
//     mbedtls_md_update(&ctx, (const unsigned char*)password.c_str(), password.length());
//     mbedtls_md_finish(&ctx, hash);
//     mbedtls_md_free(&ctx);
    
//     String hashString = "";
//     for (int i = 0; i < 32; i++) {
//         char str[3];
//         sprintf(str, "%02x", (int)hash[i]);
//         hashString += str;
//     }
    
//     return hashString;
// }

// bool verifyPassword(const String& password) {
//     String inputHash = hashPassword(password);
//     bool valid = (inputHash == ADMIN_PASSWORD_HASH);
    
//     if (valid) {
//         LOG_INFO("Password verification successful");
//     } else {
//         LOG_WARNING("Password verification failed");
//     }
    
//     return valid;
// }


// #include "auth_manager.h"
// #include "../config/config.h"
// #include "../config/credentials_manager.h"  // <-- DODAJ TĘ LINIĘ
// #include "../core/logging.h"
// #include <mbedtls/md.h>

// void initAuthManager() {
//     LOG_INFO("Authentication manager initialized");
//     LOG_INFO("Using %s admin credentials", areCredentialsLoaded() ? "FRAM" : "fallback");
// }

// bool isIPAllowed(IPAddress ip) {
//     for (int i = 0; i < ALLOWED_IPS_COUNT; i++) {
//         if (ALLOWED_IPS[i] == ip) {
//             return true;
//         }
//     }
//     return false;
// }

// String hashPassword(const String& password) {
//     byte hash[32];
    
//     mbedtls_md_context_t ctx;
//     mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    
//     mbedtls_md_init(&ctx);
//     mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
//     mbedtls_md_starts(&ctx);
//     mbedtls_md_update(&ctx, (const unsigned char*)password.c_str(), password.length());
//     mbedtls_md_finish(&ctx, hash);
//     mbedtls_md_free(&ctx);
    
//     String hashString = "";
//     for (int i = 0; i < 32; i++) {
//         char str[3];
//         sprintf(str, "%02x", (int)hash[i]);
//         hashString += str;
//     }
    
//     return hashString;
// }

// bool verifyPassword(const String& password) {
//     String inputHash = hashPassword(password);
    
//     // Use dynamic admin password hash
//     String expectedHash = String(getAdminPasswordHash());
//     bool valid = (inputHash == expectedHash);
    
//     if (valid) {
//         LOG_INFO("Password verification successful (%s)", 
//                 areCredentialsLoaded() ? "FRAM credentials" : "fallback credentials");
//     } else {
//         LOG_WARNING("Password verification failed");
//         if (areCredentialsLoaded()) {
//             LOG_WARNING("Check if admin password was correctly programmed to FRAM");
//         } else {
//             LOG_WARNING("Using fallback credentials - consider programming FRAM");
//         }
//     }
    
//     return valid;
// }


#include "auth_manager.h"
#include "../config/config.h"
#include "../core/logging.h"
#include <mbedtls/md.h>

#if MODE_PRODUCTION
    #include "../config/credentials_manager.h"
#endif

void initAuthManager() {
    LOG_INFO("Authentication manager initialized");
#if MODE_PRODUCTION
    LOG_INFO("Using %s admin credentials", areCredentialsLoaded() ? "FRAM" : "fallback");
#else
    LOG_INFO("Using hardcoded admin credentials (programming mode)");
#endif
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
    
#if MODE_PRODUCTION
    // Production mode: use dynamic admin password hash
    String expectedHash = String(getAdminPasswordHash());
    bool valid = (inputHash == expectedHash);
    
    if (valid) {
        LOG_INFO("Password verification successful (%s)", 
                areCredentialsLoaded() ? "FRAM credentials" : "fallback credentials");
    } else {
        LOG_WARNING("Password verification failed");
        if (areCredentialsLoaded()) {
            LOG_WARNING("Check if admin password was correctly programmed to FRAM");
        } else {
            LOG_WARNING("Using fallback credentials - consider programming FRAM");
        }
    }
#else
    // Programming mode: use hardcoded admin password hash
    String expectedHash = String(ADMIN_PASSWORD_HASH);
    bool valid = (inputHash == expectedHash);
    
    if (valid) {
        LOG_INFO("Password verification successful (hardcoded - programming mode)");
    } else {
        LOG_WARNING("Password verification failed (programming mode)");
    }
#endif
    
    return valid;
}