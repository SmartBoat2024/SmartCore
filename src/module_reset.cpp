#include <Arduino.h>
#include "SmartCore_System.h"
#include <EEPROM.h>
#include "config.h"

extern TaskHandle_t voltmeterSensorTaskHandle;

/// @brief suspend module specific tasks here during reset
void suspendModuleTasksDuringReset() {
    Serial.println("[MODULE] suspending module specfic tasks.");
   
}

// ========================= Module-Specific Parameter Reset =========================

/**
 * @brief Reset the parameters specific to this module.
 * 
 * This function is used to reset any module-specific settings like sensor calibration,
 * relay states, or configurations.
 */
void resetModuleSpecificParameters() {
    Serial.println("Module-specific parameters have been reset.");
}