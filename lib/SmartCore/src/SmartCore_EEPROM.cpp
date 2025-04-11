#include "SmartCore_EEPROM.h"
#include "SmartCore_Webserver.h"
#include "config.h"

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
    size_t length = strlen(_settings.serialNumber); 
    if (length >= 39) length = 38;
    for (size_t i = 0; i < length; ++i) {
        EEPROM.write(SN_ADDR + i, _settings.serialNumber[i]);
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

void writeStringToEEPROM(int address, String data) {
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
    // Call each of the above with default values
    // This will get expanded later
    Serial.println("⚠️ EEPROM reset not fully implemented yet.");
}

} // namespace
