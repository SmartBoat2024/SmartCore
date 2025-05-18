#include "SmartCore_EEPROM.h"
#include "SmartCore_Webserver.h"
#include "SmartCore_System.h"
#include "SmartCore_Network.h"
#include "SmartCore_Log.h"
#include "config.h"
#include <SmartConnect_Async_WiFiManager.h>
#include "module_reset.h"

namespace SmartCore_EEPROM {

    static SmartCoreSettings _settings;

    void init(const SmartCoreSettings& settings) {
        _settings = settings;
    }

    // --- Integer Read/Write ---
    int readIntFromEEPROM(int address) {
        return (EEPROM.read(address) << 8) + EEPROM.read(address + 1);
    }

    void writeIntToEEPROM(int number, int address) {
        EEPROM.write(address, number >> 8);
        EEPROM.write(address + 1, number & 0xFF);
        EEPROM.commit();
    }

    // --- Serial Number (char array) ---
    void writeSerialNumberToEEPROM() {
        size_t length = strlen(serialNumber);
        if (length >= EEPROM_STR_LEN) length = EEPROM_STR_LEN;

        for (size_t i = 0; i < length; ++i) {
            EEPROM.write(SN_ADDR + i, serialNumber[i]);
        }

        EEPROM.write(SN_ADDR + length, '\0');

        for (size_t i = length + 1; i < EEPROM_STR_TOTAL; ++i) {
            EEPROM.write(SN_ADDR + i, '\0');
        }

        EEPROM.commit();
    }

    char* readSerialNumberFromEEPROM() {
        static char result[EEPROM_STR_TOTAL];
        for (int i = 0; i < EEPROM_STR_TOTAL - 1; ++i) {
            result[i] = EEPROM.read(SN_ADDR + i);
            if (result[i] == '\0') break;
        }
        result[EEPROM_STR_TOTAL - 1] = '\0';
        return result;
    }

    // --- Webname (char array) ---
    void writeCustomWebnameToEEPROM() {
        size_t length = strlen(webnamechar);
        if (length >= EEPROM_STR_LEN) length = EEPROM_STR_LEN;

        for (size_t i = 0; i < length; ++i) {
            EEPROM.write(WEBNAME_ADDR + i, webnamechar[i]);
        }

        EEPROM.write(WEBNAME_ADDR + length, '\0');

        for (size_t i = length + 1; i < EEPROM_STR_TOTAL; ++i) {
            EEPROM.write(WEBNAME_ADDR + i, '\0');
        }

        EEPROM.commit();
    }

    char* readCustomWebnameFromEEPROM() {
        static char result[EEPROM_STR_TOTAL];
        for (int i = 0; i < EEPROM_STR_TOTAL - 1; ++i) {
            result[i] = EEPROM.read(WEBNAME_ADDR + i);
            if (result[i] == '\0') break;
        }
        result[EEPROM_STR_TOTAL - 1] = '\0';
        return result;
    }

    // --- String Helper (for String-based variables) ---
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
        if (length > EEPROM_STR_LEN) length = EEPROM_STR_LEN;

        for (int i = 0; i < length; ++i) {
            EEPROM.write(address + i, data[i]);
        }

        EEPROM.write(address + length, '\0');

        for (int i = length + 1; i < EEPROM_STR_TOTAL; ++i) {
            EEPROM.write(address + i, '\0');
        }

        EEPROM.commit();
    }

    // --- Module Name (String) ---
    void writeModuleNameToEEPROM(const String& moduleName) {
        writeStringToEEPROM(MOD_NAME_ADDR, moduleName);
    }

    String readModuleNameFromEEPROM() {
        return readStringFromEEPROM(MOD_NAME_ADDR, EEPROM_STR_TOTAL);
    }

    // --- Location (String) ---
    void writeLocationToEEPROM(const String& location) {
        writeStringToEEPROM(MOD_LOCATION_ADDR, location);
    }

    String readLocationFromEEPROM() {
        return readStringFromEEPROM(MOD_LOCATION_ADDR, EEPROM_STR_TOTAL);
    }

    // --- Boolean Helpers ---
    void writeBoolToEEPROM(int address, bool value) {
        EEPROM.write(address, value ? 1 : 0);
        EEPROM.commit();
    }

    bool readBoolFromEEPROM(int address) {
        return EEPROM.read(address) == 1;
    }

    String boolToString(bool value) {
        return value ? "true" : "false";
    }

    // --- Flags & Modes ---
    void writeSmartBoatToEEPROM(bool smartBoat) {
        writeBoolToEEPROM(SB_BOOL_ADDR, smartBoat);
    }

    bool readSmartBoatFromEEPROM() {
        return readBoolFromEEPROM(SB_BOOL_ADDR);
    }

    void writeFirstWiFiConnectFlag(bool flag) {
        writeBoolToEEPROM(EEPROM_ADDR_FLAG, flag);
    }

    bool readFirstWiFiConnectFlag() {
        return readBoolFromEEPROM(EEPROM_ADDR_FLAG);
    }

    void writeSerialNumberAssignedFlag(bool flag) {
        writeBoolToEEPROM(SERIAL_ASSIGNED_ADDR, flag);
    }

    bool readSerialNumberAssignedFlag() {
        return readBoolFromEEPROM(SERIAL_ASSIGNED_ADDR);
    }

    void writeResetConfigFlag(bool flag) {
        writeBoolToEEPROM(RESET_ADDR_FLAG, flag);
    }

    bool readResetConfigFlag() {
        return readBoolFromEEPROM(RESET_ADDR_FLAG);
    }

    void writeStandaloneModeToEEPROM(bool mode) {
        writeBoolToEEPROM(STANDALONE_MODE_ADDR, mode);
    }

    bool readStandaloneModeFromEEPROM() {
        return readBoolFromEEPROM(STANDALONE_MODE_ADDR);
    }

    // --- Upgrade Flag ---
    void saveUpgradeFlag(bool value) {
        writeBoolToEEPROM(UPGRADE_FLAG_ADDR, value);
    }

    bool loadUpgradeFlag() {
        return readBoolFromEEPROM(UPGRADE_FLAG_ADDR);
    }

    void writeSmartNetAddress(uint8_t addr) {
        EEPROM.write(SMARTNET_ADDR_EEPROM, addr);
        EEPROM.commit();
    }
    
    uint8_t readSmartNetAddress() {
        return EEPROM.read(SMARTNET_ADDR_EEPROM);
    }

    // --- Update Helper ---
    void updateEEPROMIfNeeded(int address, byte value) {
        if (EEPROM.read(address) != value) {
            EEPROM.write(address, value);
            EEPROM.commit();
        }
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

    String defaultLocation = "location";
    writeLocationToEEPROM(defaultLocation);

    // --- Flags
    resetConfig = true;
    writeResetConfigFlag(true);
    
    wifiSetupComplete = false;
    writeBoolToEEPROM(WIFI_SETUP_COMPLETE_ADDR, false);

    smartConnectEnabled = false;
    writeBoolToEEPROM(SC_BOOL_ADDR, false);

    smartBoatEnabled = false;
    writeBoolToEEPROM(SB_BOOL_ADDR, false);

    standaloneMode = false;
    writeStandaloneModeToEEPROM(false);

    standaloneFlag = false;

    // Reset first WiFi connect flag
    writeFirstWiFiConnectFlag(true);

    // Reset serial number assigned flag
    writeSerialNumberAssignedFlag(false);

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

} // namespace

