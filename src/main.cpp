#include <Arduino.h>
#include "SmartCore_System.h"
#include "SmartCore_Log.h"
#include "SmartCore_LED.h"
#include "SmartCore_I2C.h"
#include "SmartCore_MCP.h"
#include "config.h"
#define ESPASYNC_WIFI_MANAGER_IMPLEMENTATION
#include <SmartConnect_Async_WiFiManager.h>

// Define the EEPROM size and the end of the SmartCore settings
#define EEPROM_SIZE 1024
#define SMARTCORE_EEPROM_END 400  // SmartCore settings end at address 200
#define WebServerEnabled true  //determines if webservices should start


// ========================
//  Module Settings
// ========================

SmartCoreSettings myModuleSettings = {
    .serialNumber = "rel8XXXX",
    .moduleName = "Smart Relay Controller"
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

  // Log the system initialization for debugging purposes
  logMessage(LOG_INFO, "Module initialized successfully.");
}

// --- Loop Function ---
void loop() {
  // Main module-specific loop code goes here
  // Example: read sensor data, update outputs, etc.
  // This loop runs continuously after setup()
}

// ========================= Module-Specific Parameter Reset =========================

/**
 * @brief Reset the parameters specific to this module.
 * 
 * This function is used to reset any module-specific settings like sensor calibration,
 * relay states, or configurations.
 */
void resetModuleSpecificParameters() {
  // Reset module-specific parameters (e.g., clear sensor data, reset state)
  // Example: resetRelayStates();

  // Log the reset event
  Serial.println("Module-specific parameters have been reset.");
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
