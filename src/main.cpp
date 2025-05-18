#include <Arduino.h>
#include "SmartCore_System.h"
#include "SmartCore_Network.h"
#include "SmartCore_Log.h"
#include "SmartCore_LED.h"
#include "SmartCore_I2C.h"
#include "SmartCore_MCP.h"
#include "config.h"

// Define the EEPROM size and the end of the SmartCore settings
#define EEPROM_SIZE 1024
#define SMARTCORE_EEPROM_END 300  // SmartCore settings end at address 200


// ========================
//  Module Settings
// ========================

// ==   prefix for ap start with ===
// 'SB_relay': 'Smart Relay Module',
//  'SB_stm': 'Smart Temp Monitor',
//  'SB_stc': 'Smart Terminal Controller',
//  'SB_sen': 'Smart Sensor Module',
// =====================================

SmartCoreSettings myModuleSettings = {
    .serialNumber = "rel8XXXX",
    .moduleName = "Smart Relay Controller",
    .apSSID = "SB_relay_XXXX"  // change to module specific ssid
};

//////////////////////////////////////////////////////////////

// --- Setup Function ---
void setup() {
    SmartCore_System::preinit();
    // Initialize the core SmartCore system (EEPROM, I2C, LEDs, etc.)
    if (resetConfig){
      SmartCore_System::setModuleSettings(myModuleSettings);
      Serial.println("default settings applied");
      delay(1000);
    }  
  
    SmartCore_System::init();

  // Custom module setup code goes here:
  // Initialize sensors, modules, etc.

  //Clears the crash counter
  SmartCore_System::clearCrashCounter(CRASH_COUNTER_BOOT);
  // Log the system initialization for debugging purposes
  logMessage(LOG_INFO, "Module initialized successfully.");
}

// --- Loop Function ---
void loop() {
  // Main module-specific loop code goes here
  // Example: read sensor data, update outputs, etc.
  // This loop runs continuously after setup()
}


// ========================= Module-Specific Configuration =========================

/**
 * @brief Retrieve the configuration specific to this module.
 * 
 * This function is used to fetch the settings for the module (e.g., from EEPROM).
 * You should retrieve and configure the module's parameters here.
 */
void getModuleSpecificConfig() {
  // Get module-specific settings from EEPROM or external sources
  // Example: loadRelayConfig();

  // Log the configuration retrieval for debugging purposes
  Serial.println("Module-specific configuration has been loaded.");
}
