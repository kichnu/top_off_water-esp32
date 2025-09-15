
#ifndef HTML_PAGES_H
#define HTML_PAGES_H

#include <Arduino.h>
#include "../mode_config.h"

#if ENABLE_WEB_SERVER
    String getDashboardHTML();
    String getLoginHTML();
#endif

#endif