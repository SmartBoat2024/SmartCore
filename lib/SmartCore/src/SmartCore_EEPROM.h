#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include "SmartCore_Config.h"

#define EEPROM_STR_LEN     40   // Max usable characters (String)
#define EEPROM_STR_TOTAL   41   // Includes null terminator

namespace SmartCore_EEPROM {

    void init(const SmartCoreSettings& settings);

    // --- Integer Read/Write ---
    int readIntFromEEPROM(int address);
    void writeIntToEEPROM(int number, int address);

    // --- EEPROM Strings (Arduino String) ---
    String readStringFromEEPROM(int address, int maxLength);
    void writeStringToEEPROM(int address, String data, int maxLen);

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

    // --- EEPROM Module Name ---
    void writeModuleNameToEEPROM(const String& moduleName);
    String readModuleNameFromEEPROM();

    // --- EEPROM Location Name ---
    void writeLocationToEEPROM(const String& location);
    String readLocationFromEEPROM();

    // --- EEPROM Flags ---

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
    void resetParameters();
}

// ===== EEPROM Address Map for SmartCore Modules =====
//SmartCore_EEPROM::writeStringToEEPROM(WIFI_PASS_ADDR, pass);
                //SmartCore_EEPROM::writeStringToEEPROM(MQTT_IP_ADDR, mqttIp);
               // SmartCore_EEPROM::writeStringToEEPROM(MQTT_PORT_ADDR, mqttPort);
               // SmartCore_EEPROM::writeStringToEEPROM(SN_ADDR, serial);
               // SmartCore_EEPROM::writeBoolToEEPROM(WIFI_SETUP_COMPLETE_ADDR, true);
// --- Magic Byte for Initialization ---
#define INIT_MAGIC_VALUE            0xA5
#define INIT_MAGIC_ADDR             0         // 1 byte

// --- Serial Number & Module Name ---
#define SN_ADDR                     1         // char[40] + 1 => 41 bytes (1–41)
#define MOD_NAME_ADDR               42        // char[40] + 1 => 41 bytes (42–82)

// --- Flags & Status Bytes ---
#define WIFI_SETUP_COMPLETE_ADDR    83        // 1 byte
#define RESET_ADDR_FLAG             84        // 1 byte
#define WIFI_FAIL_COUNTER_ADDR      85        // 1 byte

// --- Network Params ---
#define MQTT_IP_ADDR                86        // char[16] + 1 => 17 bytes (86–102)
#define MQTT_PORT_ADDR              103       // char[6] + 1 => 7 bytes (103–109)

// --- Miscellaneous Flags ---
#define EEPROM_ADDR_FLAG            110       // 1 byte
#define UPGRADE_FLAG_ADDR           111       // 1 byte

// --- WiFi Credentials ---
#define WIFI_SSID_ADDR              112       // char[40] + 1 => 41 bytes (112–152)
#define WIFI_PASS_ADDR              153       // char[40] + 1 => 41 bytes (153–193)

// --- Serial Assignment & Location ---
#define SERIAL_ASSIGNED_ADDR        194       // 1 byte
#define MOD_LOCATION_ADDR           195       // char[40] + 1 => 41 bytes (195–235)

// --- SmartNet/CAN Addressing ---
#define SMARTNET_ADDR_EEPROM        236       // 1 byte

// --- Crash Counters ---
#define CRASH_COUNTER_ADDR          237       // 1 byte
#define RUNTIME_CRASH_COUNTER_ADDR  238       // 1 byte

// --- Reserved / Future Use ---
#define EEPROM_RESERVED             239       // Start of reserved region
#define EEPROM_TOTAL_SIZE           300       // EEPROM size in bytes (as set in main)

