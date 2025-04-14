#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include "SmartCore_Config.h"

#define EEPROM_STR_LEN     39   // Max usable characters (String)
#define EEPROM_STR_TOTAL   40   // Includes null terminator

namespace SmartCore_EEPROM {

    void init(const SmartCoreSettings& settings);

    // --- Integer Read/Write ---
    int readIntFromEEPROM(int address);
    void writeIntToEEPROM(int number, int address);

    // --- EEPROM Strings (Arduino String) ---
    String readStringFromEEPROM(int address, int maxLength);
    void writeStringToEEPROM(int address, String data);

    // --- EEPROM Booleans ---
    void writeBoolToEEPROM(int address, bool value);
    bool readBoolFromEEPROM(int address);

    // --- EEPROM Update Utility ---
    void updateEEPROMIfNeeded(int address, byte value);

    // --- Boolean Display Utility ---
    String boolToString(bool value);

    // --- EEPROM Serial Number ---
    void writeSerialNumberToEEPROM();
    char* readSerialNumberFromEEPROM();

    // --- EEPROM Custom Webname ---
    void writeCustomWebnameToEEPROM();
    char* readCustomWebnameFromEEPROM();

    // --- EEPROM Module Name ---
    void writeModuleNameToEEPROM(const String& moduleName);
    String readModuleNameFromEEPROM();

    // --- EEPROM Location Name ---
    void writeLocationToEEPROM(const String& location);
    String readLocationFromEEPROM();

    // --- EEPROM Flags ---
    void writeSmartBoatToEEPROM(bool smartBoat);
    bool readSmartBoatFromEEPROM();

    void writeResetConfigFlag(bool flag);
    bool readResetConfigFlag();

    void writeStandaloneModeToEEPROM(bool mode);
    bool readStandaloneModeFromEEPROM();

    void writeFirstWiFiConnectFlag(bool flag);
    bool readFirstWiFiConnectFlag();

    void writeSerialNumberAssignedFlag(bool flag);
    bool readSerialNumberAssignedFlag();

    void writeSmartNetAddress(uint8_t addr);
    uint8_t readSmartNetAddress();

    // --- Upgrade Tracking ---
    void saveUpgradeFlag(bool value);
    bool loadUpgradeFlag();

    void resetParameters();
    void resetModuleSpecificParameters();
}

// --- EEPROM Address Map ---
#define SN_ADDR                    0      // char[40] - Serial number
#define WEBNAME_ADDR               40     // char[40] - Web name
#define MOD_NAME_ADDR              80     // char[40] - Module name

// --- Flag Byte Addresses ---
#define CUSTOM_MQTT_ADDR           120    // 1 byte - Custom MQTT enabled
#define WIFI_SETUP_COMPLETE_ADDR   121    // 1 byte - WiFi setup complete
#define RESET_ADDR_FLAG            122    // 1 byte - Reset WiFi config
#define SB_BOOL_ADDR               123    // 1 byte - SmartBoat mode
#define SC_BOOL_ADDR               124    // 1 byte - SmartBoat mode


// --- Configuration Addresses ---
#define CUSTOM_MQTT_SERVER_ADDR    125    // char[40] - MQTT server
#define CUSTOM_MQTT_PORT_ADDR      165    // 2 bytes - MQTT port
#define STANDALONE_MODE_ADDR       167    // 1 byte - Standalone mode

// --- Misc ---
#define EEPROM_ADDR_FLAG           168    // 1 byte - First WiFi connect
#define UPGRADE_FLAG_ADDR          169    // 1 byte - Firmware upgrade in progress

#define WIFI_SSID_ADDR              170
#define WIFI_PASS_ADDR              210

#define CUSTOM_AP_NAME_ADDR         250
#define CUSTOM_AP_PASS_ADDR         290

#define SERIAL_ASSIGNED_ADDR         291

#define MOD_LOCATION_ADDR           292

#define SMARTNET_ADDR_EEPROM          332

#define CRASH_COUNTER_ADDR            350    // crash counter address
#define RUNTIME_CRASH_COUNTER_ADDR            360    // crash counter address

#define EEPROM_RESERVED            370    // Reserved
#define EEPROM_TOTAL_SIZE          400   // Set in main template
