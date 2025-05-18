#pragma once

struct SmartCoreSettings {
    const char* serialNumber;
    const char* moduleName;
    const char* apSSID;  
};

#define WIFI_FAIL_LIMIT_SAFE   3     // Max consecutive WiFi boot fails before Safe Mode
#define WIFI_RETRY_LIMIT       3     // Retries per boot
#define WIFI_RETRY_START_MS    3000  // Initial backoff
