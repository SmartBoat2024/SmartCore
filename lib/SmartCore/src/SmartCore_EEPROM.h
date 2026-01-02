#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include "SmartCore_Config.h"

#define EEPROM_STR_LEN 40   // Max usable characters (String)
#define EEPROM_STR_TOTAL 41 // Includes null terminator

namespace SmartCore_EEPROM
{
    extern bool resetComplete;

    void init(const SmartCoreSettings &settings);

    // --- Integer Read/Write ---
    int readIntFromEEPROM(int address);
    void writeIntToEEPROM(int number, int address);

    // --- EEPROM Strings (Arduino String) ---
    String readStringFromEEPROM(int address, int maxLength);
    void writeStringToEEPROM(int address, String data, int maxLen);

    // --- EEPEOM Bytes ---
    void writeByteToEEPROM(int address, uint8_t value);
    uint8_t readByteFromEEPROM(int address);

    // --- EEPROM Booleans ---
    void writeBoolToEEPROM(int address, bool value);
    bool readBoolFromEEPROM(int address);

    // --- EEPROM Update Utility ---
    void updateEEPROMIfNeeded(int address, byte value);

    // --- Boolean Display Utility ---
    String boolToString(bool value);

    // --- EEPROM Serial Number ---
    void writeSerialNumberToEEPROM();
    char *readSerialNumberFromEEPROM();

    // --- EEPROM Module Name ---
    void writeModuleNameToEEPROM(const String &moduleName);
    String readModuleNameFromEEPROM();

    // --- EEPROM Location Name ---
    void writeLocationToEEPROM(const String &location);
    String readLocationFromEEPROM();

    // --- EEPROM Flags ---

    void writeResetCompleteFlag(bool flag);
    bool readResetCompleteFlag();

    void writeResetConfigFlag(bool flag);
    bool readResetConfigFlag();

    void writeFirstWiFiConnectFlag(bool flag);
    bool readFirstWiFiConnectFlag();

    void writeSerialNumberAssignedFlag(bool flag);
    bool readSerialNumberAssignedFlag();

    void writeSmartNetAddress(uint8_t addr);
    uint8_t readSmartNetAddress();

    // --- Upgrade Tracking ---
    void saveUpgradeFlag(bool value);
    bool loadUpgradeFlag();

    void ensureInitialized();
    void resetParameters(bool softReset = false);
}

// ===== EEPROM Address Map for SmartCore Modules =====

#define INIT_MAGIC_VALUE 0xA5

#define INIT_MAGIC_ADDR 0

#define SN_ADDR 1        // 1–41
#define MOD_NAME_ADDR 42 // 42–82

#define WIFI_SETUP_COMPLETE_ADDR 83
#define RESET_ADDR_FLAG 84
#define WIFI_FAIL_COUNTER_ADDR 85

#define MQTT_IP_ADDR 86    // 86–102
#define MQTT_PORT_ADDR 103 // int → 103–104

#define EEPROM_ADDR_FLAG 110
#define UPGRADE_FLAG_ADDR 111

#define WIFI_SSID_ADDR 112 // 112–152
#define WIFI_PASS_ADDR 153 // 153–193

#define SERIAL_ASSIGNED_ADDR 194
#define MOD_LOCATION_ADDR 195 // 195–235

#define SMARTNET_ADDR_EEPROM 236
#define CRASH_COUNTER_ADDR 237
#define RUNTIME_CRASH_COUNTER_ADDR 238

// -------------------------------------------------------
// SMARTBOX SETTINGS
// -------------------------------------------------------

#define MQTT_STATIC_FLAG_ADDR 239 // 1 byte
#define MQTT_STATIC_IP_ADDR 240   // 240–256
#define MQTT_STATIC_MASK_ADDR 257 // 257–273

#define MQTT_BACKUP_COUNT_ADDR 274 // 274–275 (int, 2 bytes)

#define MQTT_BACKUP_LIST_ADDR 276 // first free byte
// slot0  → 276–292
// slot1  → 293–309

// -------------------------------------------------------
// SMARTMODULE SETTINGS
// -------------------------------------------------------

#define MQTT_PRIORITY_COUNT_ADDR 310 // 310–311 (int, 2 bytes)

#define MQTT_PRIORITY_LIST_ADDR 312
// slot0  → 312–328
// slot1  → 329–345
// slot2  → 346–362

#define MQTT_LAST_PRIORITY_ADDR 363
#define SMARTBOX_PRIORITY_ADDR 364

#define EEPROM_RESET_COMPLETE_FLAG 365

#define EEPROM_RESERVED 366
#define EEPROM_TOTAL_SIZE 400
