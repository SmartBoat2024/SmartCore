#include "SmartCore_EEPROM.h"
#include "SmartCore_Webserver.h"
#include "SmartCore_System.h"
#include "SmartCore_Network.h"
#include "SmartCore_Log.h"
#include "config.h"
#include <SmartConnect_Async_WiFiManager.h>

namespace SmartCore_EEPROM {

static SmartCoreSettings _settings; 

void init(const SmartCoreSettings& settings) {
    _settings = settings;
}
    

int readIntFromEEPROM(int address) {
    return (EEPROM.read(address) << 8) + EEPROM.read(address + 1);
}

void writeIntToEEPROM(int number, int address) {
    EEPROM.write(address, number >> 8);
    EEPROM.write(address + 1, number & 0xFF);
    EEPROM.commit();
}

// --- Serial Number
void writeSerialNumberToEEPROM() {
    size_t length = strlen(serialNumber); 
    if (length >= 39) length = 38;
    for (size_t i = 0; i < length; ++i) {
        EEPROM.write(SN_ADDR + i, serialNumber[i]);
    }
    EEPROM.write(SN_ADDR + length, '\0');
    EEPROM.commit();
}

char* readSerialNumberFromEEPROM() {
    static char result[40];
    for (int i = 0; i < 40; ++i) {
        result[i] = EEPROM.read(SN_ADDR + i);
    }
    result[39] = '\0';
    return result;
}

// --- Webname
void writeCustomWebnameToEEPROM() {
    for (int i = 0; i < sizeof(webnamechar); ++i) {
        EEPROM.write(WEBNAME_ADDR + i, (i < strlen(webnamechar)) ? webnamechar[i] : '\0');
    }
    EEPROM.commit();
}

char* readCustomWebnameFromEEPROM() {
    static char result[40];
    for (int i = 0; i < sizeof(result) - 1; ++i) {
        result[i] = EEPROM.read(WEBNAME_ADDR + i);
    }
    result[sizeof(result) - 1] = '\0';
    return result;
}

// --- Module Name
void writeModuleNameToEEPROM(const char* moduleName, size_t maxLen) {
    size_t i = 0;
    for (; i < maxLen - 1 && moduleName[i] != '\0'; ++i) {
        EEPROM.write(MOD_NAME_ADDR + i, moduleName[i]);
    }
    EEPROM.write(MOD_NAME_ADDR + i, '\0');
    for (++i; i < maxLen; ++i) {
        EEPROM.write(MOD_NAME_ADDR + i, '\0');
    }
    EEPROM.commit();
}

void readModuleNameFromEEPROM(char* moduleName, size_t maxLen) {
    size_t i = 0;
    for (; i < maxLen - 1; ++i) {
        moduleName[i] = EEPROM.read(MOD_NAME_ADDR + i);
        if (moduleName[i] == '\0') break;
    }
    moduleName[maxLen - 1] = '\0';
    Serial.printf("Module Name: %s\n", moduleName);
}

// --- Flags & Booleans
void writeSmartBoatToEEPROM(bool smartBoat) {
    EEPROM.write(SB_BOOL_ADDR, smartBoat ? 1 : 0);
    EEPROM.commit();
}

bool readSmartBoatFromEEPROM() {
    return EEPROM.read(SB_BOOL_ADDR) == 1;
}

bool readFirstWiFiConnectFlag() {
    return EEPROM.read(EEPROM_ADDR_FLAG) == 1;
}

void writeFirstWiFiConnectFlag(bool flag) {
    EEPROM.write(EEPROM_ADDR_FLAG, flag ? 1 : 0);
    EEPROM.commit();
}

bool readResetConfigFlag() {
    return EEPROM.read(RESET_ADDR_FLAG) == 1;
}

void writeResetConfigFlag(bool flag) {
    EEPROM.write(RESET_ADDR_FLAG, flag ? 1 : 0);
    EEPROM.commit();
}

void writeStandaloneModeToEEPROM(bool mode) {
    EEPROM.write(STANDALONE_MODE_ADDR, mode ? 1 : 0);
    EEPROM.commit();
}

bool readStandaloneModeFromEEPROM() {
    return EEPROM.read(STANDALONE_MODE_ADDR) == 1;
}

// --- String Helper
String readStringFromEEPROM(int address, int maxLength) {
    char buffer[maxLength + 1];
    for (int i = 0; i < maxLength; ++i) {
        buffer[i] = EEPROM.read(address + i);
        if (buffer[i] == '\0') break;
    }
    buffer[maxLength] = '\0';
    return String(buffer);
}

void writeStringToEEPROM(int address, const String& data) {
    int length = data.length();
    for (int i = 0; i < length; ++i) {
        EEPROM.write(address + i, data[i]);
    }
    EEPROM.write(address + length, '\0');
    EEPROM.commit();
}

void writeBoolToEEPROM(int address, bool value) {
    EEPROM.write(address, value ? 1 : 0);
    EEPROM.commit();
}

bool readBoolFromEEPROM(int address) {
    return EEPROM.read(address) == 1;
}

void updateEEPROMIfNeeded(int address, byte value) {
    if (EEPROM.read(address) != value) {
        EEPROM.write(address, value);
        EEPROM.commit();
    }
}

String boolToString(bool value) {
    return value ? "true" : "false";
}

// --- Upgrade Flag
void saveUpgradeFlag(bool value) {
    EEPROM.write(UPGRADE_FLAG_ADDR, value ? 1 : 0);
    EEPROM.commit();
}

bool loadUpgradeFlag() {
    return EEPROM.read(UPGRADE_FLAG_ADDR) == 1;
}

// --- Reset All (if needed)
void resetParameters() {
    logMessage(LOG_INFO, "🔄 Resetting parameters to default...");

    // --- Webname
    webname = "smartmodule";
    writeStringToEEPROM(WEBNAME_ADDR, webname);
    strncpy(webnamechar, webname.c_str(), sizeof(webnamechar) - 1);
    webnamechar[sizeof(webnamechar) - 1] = '\0';

    // --- WiFi AP credentials
    strncpy(apName, "SmartConnectAP", sizeof(apName) - 1);
    apName[sizeof(apName) - 1] = '\0';
    writeStringToEEPROM(CUSTOM_AP_NAME_ADDR, String(apName));

    strncpy(apPassword, "12345678", sizeof(apPassword) - 1);
    apPassword[sizeof(apPassword) - 1] = '\0';
    writeStringToEEPROM(CUSTOM_AP_PASS_ADDR, String(apPassword));

    // --- Flags
    resetConfig = true;
    SmartCore_EEPROM::writeResetConfigFlag(true);
    
    wifiSetupComplete = false;
    writeBoolToEEPROM(WIFI_SETUP_COMPLETE_ADDR, false);

    smartConnectEnabled = false;
    writeBoolToEEPROM(SC_BOOL_ADDR, false);

    smartBoatEnabled = false;
    writeBoolToEEPROM(SB_BOOL_ADDR, false);

    standaloneMode = false;
    writeStandaloneModeToEEPROM(false);

    standaloneFlag = false;

    // First WiFi connect flag
    writeFirstWiFiConnectFlag(true);

    // --- MQTT reset
    customMqtt = false;
    writeBoolToEEPROM(CUSTOM_MQTT_ADDR, false);

    memset(custom_mqtt_server, 0, sizeof(custom_mqtt_server));
    writeStringToEEPROM(CUSTOM_MQTT_SERVER_ADDR, String(""));

    custom_mqtt_port = 1883;
    writeIntToEEPROM(CUSTOM_MQTT_PORT_ADDR, 1883);

    // --- Commit all
    if (!EEPROM.commit()) {
        logMessage(LOG_ERROR, "❌ Failed to commit EEPROM changes during reset");
    } else {
        logMessage(LOG_INFO, "✅ EEPROM values reset and committed successfully");
    }

    delay(250);

    // --- Call user-defined overrides (if provided)
    resetModuleSpecificParameters();
}

void __attribute__((weak)) resetModuleSpecificParameters() {
    logMessage(LOG_INFO, "🧩 No module-specific reset logic provided.");
}

} // namespace

