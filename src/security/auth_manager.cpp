
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
    if (areCredentialsLoaded()) {
       LOG_INFO("Using FRAM admin credentials");
    } else {
      LOG_WARNING("⚠️ NO FRAM CREDENTIALS - Web authentication DISABLED!");
      LOG_WARNING("⚠️ Use Programming Mode to configure credentials!");
    }
 #else
   LOG_INFO("Using hardcoded admin credentials (programming mode)");
   LOG_INFO("Programming Mode - Authentication bypassed for CLI access");
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

#if MODE_PRODUCTION

    if (!areCredentialsLoaded()) {
        LOG_ERROR("🔒 Authentication BLOCKED - No FRAM credentials loaded!");
        LOG_ERROR("🔧 Use Programming Mode to configure credentials first:");
        LOG_ERROR("   1. pio run -e programming -t upload");
        LOG_ERROR("   2. pio device monitor -e programming");
        LOG_ERROR("   3. FRAM> program");
        return false;  // ✅ Force FRAM setup!
    }
    
    String inputHash = hashPassword(password);
    String expectedHash = String(getAdminPasswordHash());

    
    if (expectedHash.length() == 0 || expectedHash == "NO_AUTH_REQUIRES_FRAM_PROGRAMMING") {
        LOG_ERROR("Invalid admin hash from FRAM - check credential programming");
        return false;
    }
    
    bool valid = (inputHash == expectedHash);

    if (valid) {
        LOG_INFO("✅ Password verification successful (FRAM credentials)");
    } else {
        LOG_WARNING("❌ Password verification failed");
        LOG_WARNING("💡 Verify admin password was correctly programmed to FRAM"); 
    }
    return valid;


#else
    // Programming mode: Allow CLI access for credential setup
    LOG_INFO("🔧 Programming Mode - Authentication bypassed for CLI access");
    return true;  // Allow access for credential programming
    
#endif
    
}