#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include "SmartCore_Config.h"

namespace SmartCore_EEPROM {
    void init(const SmartCoreSettings& settings);
    void writeSerialNumberToEEPROM();
    const char* getSerialNumber();
}

// Start of strings (char arrays)
#define SN_ADDR                    0      // Serial number (char[40])
#define WEBNAME_ADDR               40     // Web name (char[40])
#define MOD_NAME_ADDR              80     // Module name (char[40])

// Start of flags (single bytes)
#define CUSTOM_MQTT_ADDR           120    // 1 byte flag (Custom MQTT enabled?)
#define WIFI_SETUP_COMPLETE_ADDR   121    // 1 byte flag (WiFi setup complete?)
#define RESET_ADDR_FLAG            122    // 1 byte flag (Reset config flag)
#define SB_BOOL_ADDR               123    // 1 byte flag (SmartBoat enabled?)

// Start of configurations (like ports or IPs)
#define CUSTOM_MQTT_SERVER_ADDR    124    // Max 40 bytes (Custom MQTT server address)
#define CUSTOM_MQTT_PORT_ADDR      164    // 2 bytes (Custom MQTT port)
#define STANDALONE_MODE_ADDR       166    // 1 byte flag (Standalone mode?)

// Miscellaneous
#define EEPROM_ADDR_FLAG           167    // 1 byte flag (First WiFi connection flag)
#define UPGRADE_FLAG_ADDR          168    // 1 byte flag (Firmware upgrade flag)

// Remaining EEPROM space reserved for data
#define EEPROM_RESERVED            170    // Reserved for future use

// Current max smartcore eeprom size is 200 - if this increases then modify the template (main.cpp)

namespace SmartCore_EEPROM {
        // Integer operations
    int readIntFromEEPROM(int address);
    void writeIntToEEPROM(int number, int address);

    // String operations
    String readStringFromEEPROM(int address, int maxLength);
    void writeStringToEEPROM(int address, const String& data);

    // Character array (C-string) operations
    void writeCStringToEEPROM(int address, const char* str, size_t maxLen);
    void readCStringFromEEPROM(int address, char* dest, size_t maxLen);

    // Boolean operations
    void writeBoolToEEPROM(int address, bool value);
    bool readBoolFromEEPROM(int address);

    // Byte update
    void updateEEPROMIfNeeded(int address, byte value);

    // Utility
    String boolToString(bool value);

    // Upgrade flag
    void saveUpgradeFlag(bool value);
    bool loadUpgradeFlag();

    // Reset parameters (placeholder)
    void resetParameters();
}