#include "SmartCore_EEPROM.h"
#include "SmartCore_System.h"
#include "SmartCore_Network.h"
#include "SmartCore_Log.h"
#include "SmartCore_MQTT.h"
#include "config.h"
#include "module_reset.h"
#include <WiFi.h>

namespace SmartCore_EEPROM
{

    static SmartCoreSettings _settings;

    void init(const SmartCoreSettings &settings)
    {
        _settings = settings;
    }

    // --- Integer Read/Write ---
    int readIntFromEEPROM(int address)
    {
        return (EEPROM.read(address) << 8) + EEPROM.read(address + 1);
    }

    void writeIntToEEPROM(int number, int address)
    {
        // Write high/low bytes
        uint8_t high = (number >> 8) & 0xFF;
        uint8_t low = number & 0xFF;
        EEPROM.write(address, high);
        EEPROM.write(address + 1, low);

        Serial.printf("📝 EEPROM pre-commit @ [0x%X]=%u (high), [0x%X]=%u (low)\n",
                      address, high, address + 1, low);

        EEPROM.commit();

        // Immediate read-back
        uint8_t r_high = EEPROM.read(address);
        uint8_t r_low = EEPROM.read(address + 1);
        int readback = (r_high << 8) | r_low;
        Serial.printf("🔍 EEPROM post-commit @ [0x%X]=%u, [0x%X]=%u → int=%d\n",
                      address, r_high, address + 1, r_low, readback);

        // Bonus: show raw bytes for the whole MQTT_PRIORITY_COUNT_ADDR region
        Serial.print("🦴 RAW EEPROM BYTES: ");
        for (int i = address - 2; i <= address + 3; ++i)
        {
            Serial.printf("0x%02X ", EEPROM.read(i));
        }
        Serial.println();
    }

    // --- Serial Number (char array) ---
    void writeSerialNumberToEEPROM()
    {
        size_t length = strlen(serialNumber);
        if (length >= EEPROM_STR_LEN)
            length = EEPROM_STR_LEN;

        for (size_t i = 0; i < length; ++i)
        {
            EEPROM.write(SN_ADDR + i, serialNumber[i]);
        }

        EEPROM.write(SN_ADDR + length, '\0');

        for (size_t i = length + 1; i < EEPROM_STR_TOTAL; ++i)
        {
            EEPROM.write(SN_ADDR + i, '\0');
        }

        EEPROM.commit();
    }

    char *readSerialNumberFromEEPROM()
    {
        static char result[EEPROM_STR_TOTAL];
        for (int i = 0; i < EEPROM_STR_TOTAL - 1; ++i)
        {
            result[i] = EEPROM.read(SN_ADDR + i);
            if (result[i] == '\0')
                break;
        }
        result[EEPROM_STR_TOTAL - 1] = '\0';
        return result;
    }

    // --- String Helper (for String-based variables) ---
    String readStringFromEEPROM(int address, int maxLen)
    {
        char buffer[maxLen + 1];
        for (int i = 0; i < maxLen; ++i)
        {
            buffer[i] = EEPROM.read(address + i);
            if (buffer[i] == '\0')
                break;
        }
        buffer[maxLen] = '\0';
        return String(buffer);
    }

    void writeStringToEEPROM(int address, String data, int maxLen)
    {
        int length = data.length();
        if (length > maxLen - 1)
            length = maxLen - 1; // always leave room for null
        for (int i = 0; i < length; ++i)
        {
            EEPROM.write(address + i, data[i]);
        }
        EEPROM.write(address + length, '\0');
        // pad the rest
        for (int i = length + 1; i < maxLen; ++i)
        {
            EEPROM.write(address + i, '\0');
        }
        EEPROM.commit();
    }

    void writeByteToEEPROM(int address, uint8_t value)
    {
        if (address < 0 || address >= EEPROM_TOTAL_SIZE)
        {
            logMessage(LOG_ERROR, "❌ writeByteToEEPROM: Address out of range!");
            return;
        }

        EEPROM.write(address, value);

        if (!EEPROM.commit())
        {
            logMessage(LOG_ERROR, "❌ writeByteToEEPROM: EEPROM commit failed!");
        }
        else
        {
            logMessage(LOG_INFO, "💾 EEPROM byte written at " + String(address) + " = " + String(value));
        }
    }

    uint8_t readByteFromEEPROM(int address)
    {
        if (address < 0 || address >= EEPROM_TOTAL_SIZE)
        {
            logMessage(LOG_ERROR, "❌ readByteFromEEPROM: Address out of range!");
            return 0; // safe default
        }

        uint8_t value = EEPROM.read(address);
        logMessage(LOG_INFO, "📖 EEPROM byte read at " + String(address) + " = " + String(value));
        return value;
    }

    // --- Module Name (String) ---
    void writeModuleNameToEEPROM(const String &moduleName)
    {
        writeStringToEEPROM(MOD_NAME_ADDR, moduleName, 41);
    }

    String readModuleNameFromEEPROM()
    {
        return readStringFromEEPROM(MOD_NAME_ADDR, EEPROM_STR_TOTAL);
    }

    // --- Location (String) ---
    void writeLocationToEEPROM(const String &location)
    {
        writeStringToEEPROM(MOD_LOCATION_ADDR, location, 41);
    }

    String readLocationFromEEPROM()
    {
        return readStringFromEEPROM(MOD_LOCATION_ADDR, EEPROM_STR_TOTAL);
    }

    // --- Boolean Helpers ---
    void writeBoolToEEPROM(int address, bool value)
    {
        EEPROM.write(address, value ? 1 : 0);
        EEPROM.commit();
    }

    bool readBoolFromEEPROM(int address)
    {
        return EEPROM.read(address) == 1;
    }

    String boolToString(bool value)
    {
        return value ? "true" : "false";
    }

    // --- Flags & Modes ---

    void writeFirstWiFiConnectFlag(bool flag)
    {
        writeBoolToEEPROM(EEPROM_ADDR_FLAG, flag);
    }

    bool readFirstWiFiConnectFlag()
    {
        return readBoolFromEEPROM(EEPROM_ADDR_FLAG);
    }

    void writeSerialNumberAssignedFlag(bool flag)
    {
        writeBoolToEEPROM(SERIAL_ASSIGNED_ADDR, flag);
    }

    bool readSerialNumberAssignedFlag()
    {
        return readBoolFromEEPROM(SERIAL_ASSIGNED_ADDR);
    }

    void writeResetConfigFlag(bool flag)
    {
        writeBoolToEEPROM(RESET_ADDR_FLAG, flag);
    }

    bool readResetConfigFlag()
    {
        return readBoolFromEEPROM(RESET_ADDR_FLAG);
    }

    // --- Upgrade Flag ---
    void saveUpgradeFlag(bool value)
    {
        writeBoolToEEPROM(UPGRADE_FLAG_ADDR, value);
    }

    bool loadUpgradeFlag()
    {
        return readBoolFromEEPROM(UPGRADE_FLAG_ADDR);
    }

    void writeSmartNetAddress(uint8_t addr)
    {
        EEPROM.write(SMARTNET_ADDR_EEPROM, addr);
        EEPROM.commit();
    }

    uint8_t readSmartNetAddress()
    {
        return EEPROM.read(SMARTNET_ADDR_EEPROM);
    }

    // --- Update Helper ---
    void updateEEPROMIfNeeded(int address, byte value)
    {
        if (EEPROM.read(address) != value)
        {
            EEPROM.write(address, value);
            EEPROM.commit();
        }
    }

    void ensureInitialized()
    {
#if defined(FORCE_EEPROM_RESET)
        logMessage(LOG_WARN, "🧨 [FORCE RESET] EEPROM override triggered via build flag...");
        for (int i = 0; i < EEPROM_TOTAL_SIZE; ++i)
            EEPROM.write(i, 0xFF);
        EEPROM.commit();
        resetParameters();
        EEPROM.write(INIT_MAGIC_ADDR, INIT_MAGIC_VALUE);
        EEPROM.commit();
        return;
#endif

        uint8_t magic = EEPROM.read(INIT_MAGIC_ADDR);
        if (magic != INIT_MAGIC_VALUE)
        {
            logMessage(LOG_WARN, "🧨 EEPROM uninitialized or invalid — wiping and resetting...");
            for (int i = 0; i < EEPROM_TOTAL_SIZE; ++i)
                EEPROM.write(i, 0xFF);
            EEPROM.commit();
            resetParameters();
            EEPROM.write(INIT_MAGIC_ADDR, INIT_MAGIC_VALUE);
            EEPROM.commit();
        }
    }

    // --- Reset All (if needed)
    void resetParameters(bool softReset)
{
    logMessage(LOG_WARN,
        softReset
            ? "🔄 Performing SOFT reset (logic + state only)"
            : "💣 Performing HARD reset (full reprovision)");

    // ---------------------------------------------------------
    // 1. MODULE-SPECIFIC LOGIC RESET (ALWAYS)
    // ---------------------------------------------------------
    resetModuleSpecificParameters();

    // ---------------------------------------------------------
    // 2. CRASH / SAFE MODE RESET (ALWAYS)
    // ---------------------------------------------------------

    SmartCore_System::clearCrashCounter(CRASH_COUNTER_ALL);

    logMessage(LOG_INFO, "🧹 Crash counters + safe mode cleared");

    // ---------------------------------------------------------
    // 3. EARLY EXIT FOR SOFT RESET
    // ---------------------------------------------------------
    if (softReset)
    {
        resetConfig = false;
        writeResetConfigFlag(false);

        if (!EEPROM.commit())
            logMessage(LOG_ERROR, "❌ EEPROM commit failed (soft reset)");
        else
            logMessage(LOG_INFO, "✅ Soft reset committed");

        return;
    }

    // ---------------------------------------------------------
    // 4. HARD RESET ONLY (network + provisioning)
    // ---------------------------------------------------------

    logMessage(LOG_WARN, "⚠️ Clearing provisioning + network parameters");

    resetConfig = true;
    writeResetConfigFlag(true);

    wifiSetupComplete = false;
    writeBoolToEEPROM(WIFI_SETUP_COMPLETE_ADDR, false);

    writeFirstWiFiConnectFlag(true);
    writeSerialNumberAssignedFlag(false);

    // --- Clear WiFi ---
    writeStringToEEPROM(WIFI_SSID_ADDR, "", 41);
    writeStringToEEPROM(WIFI_PASS_ADDR, "", 41);

    // --- MQTT defaults ---
    writeStringToEEPROM(MQTT_IP_ADDR, "0.0.0.0", 17);
    writeStringToEEPROM(MQTT_PORT_ADDR, "1883", 7);

    // --- Failover reset ---
    mqttPriorityCount = 0;
    writeIntToEEPROM(0, MQTT_PRIORITY_COUNT_ADDR);

    for (int i = 0; i < 8; i++)
        writeStringToEEPROM(MQTT_PRIORITY_LIST_ADDR + (i * 17), "", 17);

    writeByteToEEPROM(MQTT_LAST_PRIORITY_ADDR, 0);

    writeIntToEEPROM(0, MQTT_BACKUP_COUNT_ADDR);

    // ---------------------------------------------------------
    // 5. COMMIT
    // ---------------------------------------------------------
    if (!EEPROM.commit())
        logMessage(LOG_ERROR, "❌ EEPROM commit failed (hard reset)");
    else
        logMessage(LOG_INFO, "✅ Hard reset committed");
}


} // namespace
