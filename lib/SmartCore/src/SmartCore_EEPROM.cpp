#include "SmartCore_EEPROM.h"
#include "SmartCore_System.h"
#include "SmartCore_Network.h"
#include "SmartCore_Log.h"
#include "config.h"
#include "module_reset.h"
#include <WiFi.h>

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

    // --- String Helper (for String-based variables) ---
    String readStringFromEEPROM(int address, int maxLen) {
        char buffer[maxLen + 1];
        for (int i = 0; i < maxLen; ++i) {
            buffer[i] = EEPROM.read(address + i);
            if (buffer[i] == '\0') break;
        }
        buffer[maxLen] = '\0';
        return String(buffer);
    }

    void writeStringToEEPROM(int address, String data, int maxLen) {
        int length = data.length();
        if (length > maxLen - 1) length = maxLen - 1;   // always leave room for null
        for (int i = 0; i < length; ++i) {
            EEPROM.write(address + i, data[i]);
        }
        EEPROM.write(address + length, '\0');
        // pad the rest
        for (int i = length + 1; i < maxLen; ++i) {
            EEPROM.write(address + i, '\0');
        }
        EEPROM.commit();
    }

    // --- Module Name (String) ---
    void writeModuleNameToEEPROM(const String& moduleName) {
        writeStringToEEPROM(MOD_NAME_ADDR, moduleName, 41);
    }

    String readModuleNameFromEEPROM() {
        return readStringFromEEPROM(MOD_NAME_ADDR, EEPROM_STR_TOTAL);
    }

    // --- Location (String) ---
    void writeLocationToEEPROM(const String& location) {
        writeStringToEEPROM(MOD_LOCATION_ADDR, location, 41);
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

    void ensureInitialized() {
        #if defined(FORCE_EEPROM_RESET)
            logMessage(LOG_WARN, "🧨 [FORCE RESET] EEPROM override triggered via build flag...");
            for (int i = 0; i < EEPROM_TOTAL_SIZE; ++i) EEPROM.write(i, 0xFF);
            EEPROM.commit();
            resetParameters();
            EEPROM.write(INIT_MAGIC_ADDR, INIT_MAGIC_VALUE);
            EEPROM.commit();
            return;
        #endif
        
            uint8_t magic = EEPROM.read(INIT_MAGIC_ADDR);
            if (magic != INIT_MAGIC_VALUE) {
                logMessage(LOG_WARN, "🧨 EEPROM uninitialized or invalid — wiping and resetting...");
                for (int i = 0; i < EEPROM_TOTAL_SIZE; ++i) EEPROM.write(i, 0xFF);
                EEPROM.commit();
                resetParameters();
                EEPROM.write(INIT_MAGIC_ADDR, INIT_MAGIC_VALUE);
                EEPROM.commit();
            }
        }


    // --- Reset All (if needed)
    void resetParameters() {
        logMessage(LOG_INFO, "🔄 Resetting parameters to default...");

        String defaultLocation = "location";
        writeLocationToEEPROM(defaultLocation);

        // --- Flags
        resetConfig = true;
        writeResetConfigFlag(true);
        saveUpgradeFlag(false);
        
        wifiSetupComplete = false;
        writeBoolToEEPROM(WIFI_SETUP_COMPLETE_ADDR, false);

        // ---- MQTT Defaults ----
        // Default to local "x.x.x.20" and port 1883
        String ip = WiFi.localIP().toString();
        int lastDot = ip.lastIndexOf('.');
        if (lastDot != -1) {
            ip = ip.substring(0, lastDot + 1) + "20";
        } else {
            ip = "192.168.4.20";  // fallback if no valid local IP
        }
        writeStringToEEPROM(MQTT_IP_ADDR, ip, 17);
        writeStringToEEPROM(MQTT_PORT_ADDR, "1883", 7);

        // --- Reset SSID and Password (clear to empty string with null terminator) ---
        writeStringToEEPROM(WIFI_SSID_ADDR, "", 41); // 40+1 for null
        writeStringToEEPROM(WIFI_PASS_ADDR, "", 41); // 40+1 for null

        // Reset first WiFi connect flag
        writeFirstWiFiConnectFlag(true);

        // Reset serial number assigned flag
        writeSerialNumberAssignedFlag(false);

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

