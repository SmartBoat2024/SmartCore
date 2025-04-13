#include "SmartCore_config.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include "SmartCore_System.h"
#include "SmartCore_LED.h"
#include "SmartCore_I2C.h"
#include "SmartCore_Network.h"
#include "SmartCore_EEPROM.h"
#include "SmartCore_OTA.h"
#include "SmartCore_Log.h"
#include <SmartConnect_Async_WiFiManager.h>
#include "SmartCore_WiFi.h"

namespace SmartCore_System {

    TaskHandle_t resetButtonTaskHandle = NULL;

    const unsigned long holdTime = 5000;
    unsigned long buttonPressStart = 0;
    bool buttonPressed = false;

    static SmartCoreSettings _settings = {
        .serialNumber = "UNKNOWN",
        .moduleName = "Generic SmartCore"
    };

    void setModuleSettings(const SmartCoreSettings& settings) {
        _settings = settings;
    
        logMessage(LOG_INFO, String("📦 Module settings updated → serial: ") + 
                   (settings.serialNumber ? settings.serialNumber : "nullptr") + 
                   ", name: " + 
                   (settings.moduleName ? settings.moduleName : "nullptr"));
    
        // 🧠 SYNC TO GLOBAL BUFFERS
        strncpy(serialNumber, settings.serialNumber, sizeof(serialNumber) - 1);
        serialNumber[sizeof(serialNumber) - 1] = '\0';
    
        strncpy(moduleName, settings.moduleName, sizeof(moduleName) - 1);
        moduleName[sizeof(moduleName) - 1] = '\0';
    
        // 💾 WRITE TO EEPROM
        SmartCore_EEPROM::writeStringToEEPROM(SN_ADDR, String(settings.serialNumber));
        SmartCore_EEPROM::writeStringToEEPROM(MOD_NAME_ADDR, String(settings.moduleName));
    }

    void preinit(){
        Serial.begin(115200);
        delay(4000);

        if (!EEPROM.begin(1024)) {
            logMessage(LOG_ERROR, "❌ EEPROM.begin failed!");
            while (true) yield();
        }

        resetConfig = SmartCore_EEPROM::readResetConfigFlag();
    }
     
    void init() {

        if (!LittleFS.begin()) {
           logMessage(LOG_ERROR, "❌ LittleFS mount failed!");
            while (true) yield();
        }
        
        xTaskCreatePinnedToCore(checkresetButtonTask,   "Check Reset Button",  4096, NULL, 1, &resetButtonTaskHandle,        0);
        initSmartCoreLED();
        SmartCore_I2C::init();
        SmartCore_WiFi::startWiFiManagerTask();
    }

    void getModuleConfig() {
        logMessage(LOG_INFO, "Inside getModuleConfig");
        // Read serial number
        strncpy(serialNumber, SmartCore_EEPROM::readSerialNumberFromEEPROM(), sizeof(serialNumber) - 1);
        serialNumber[sizeof(serialNumber) - 1] = '\0'; // Ensure null-termination
        logMessage(LOG_INFO, "get config --- serialnumber: " + String(serialNumber));
    
        // Read web name
        webname = SmartCore_EEPROM::readStringFromEEPROM(WEBNAME_ADDR, sizeof(webnamechar));
        webname.toCharArray(webnamechar, sizeof(webnamechar));
    
        // Read standalone mode flag
        standaloneMode = SmartCore_EEPROM::readStandaloneModeFromEEPROM();
    
        // Read AP Name and Password from EEPROM and copy them into apName and apPassword buffers
        strncpy(apName, SmartCore_EEPROM::readStringFromEEPROM(CUSTOM_AP_NAME_ADDR, 40).c_str(), sizeof(apName) - 1);
        apName[sizeof(apName) - 1] = '\0';  // Ensure null termination
        
        strncpy(apPassword, SmartCore_EEPROM::readStringFromEEPROM(CUSTOM_AP_PASS_ADDR, 40).c_str(), sizeof(apPassword) - 1);
        apPassword[sizeof(apPassword) - 1] = '\0';  // Ensure null termination
        
        // Read custom MQTT enabled flag
        customMqtt = SmartCore_EEPROM::readBoolFromEEPROM(CUSTOM_MQTT_ADDR);
        logMessage(LOG_INFO, "Custom MQTT enabled: " + String(customMqtt));
    
        // Check for conflicts between SmartBoat and Custom MQTT
        if (customMqtt && smartBoatEnabled) {
            logMessage(LOG_WARN, "Error: Both SmartBoat and Custom MQTT are enabled. Disabling Custom MQTT.");
            customMqtt = false;
            SmartCore_EEPROM::writeBoolToEEPROM(CUSTOM_MQTT_ADDR, customMqtt);
        }
    
        // Load custom MQTT settings if enabled
        if (customMqtt) {
            // Read custom MQTT server
            strncpy(custom_mqtt_server, SmartCore_EEPROM::readStringFromEEPROM(CUSTOM_MQTT_SERVER_ADDR, sizeof(custom_mqtt_server)).c_str(), sizeof(custom_mqtt_server) - 1);
            custom_mqtt_server[sizeof(custom_mqtt_server) - 1] = '\0';  // Ensure null termination
        
            // Read custom MQTT port (as int)
            custom_mqtt_port = SmartCore_EEPROM::readIntFromEEPROM(CUSTOM_MQTT_PORT_ADDR);
        
            logMessage(LOG_INFO, "Custom MQTT Server: "); Serial.println(custom_mqtt_server);
            logMessage(LOG_INFO, "Custom MQTT Port: "); Serial.println(custom_mqtt_port);
        }
    
        isUpgradeAvailable = SmartCore_EEPROM::loadUpgradeFlag();
    
        // Read smartBoatEnabled flag
        smartBoatEnabled = EEPROM.read(SB_BOOL_ADDR) == 1;
    
        // Read smartConnectEnabled flag
        smartConnectEnabled = EEPROM.read(SC_BOOL_ADDR) == 1;
        logMessage(LOG_INFO, "smartConnect: " + String(smartConnectEnabled));
    
        // Read module name and set default if not stored
        static char moduleNameBuffer[50];
        SmartCore_EEPROM::readModuleNameFromEEPROM(moduleNameBuffer, sizeof(moduleNameBuffer));
        _settings.moduleName = moduleNameBuffer;

        if (_settings.moduleName == nullptr || strlen(_settings.moduleName) == 0) {
            _settings.moduleName = "Smart Module";
            SmartCore_EEPROM::writeModuleNameToEEPROM(_settings.moduleName, 50);
        }

        getModuleSpecificConfig();  //call module specific config here
    }

    void createAppTasks() {
        //xTaskCreatePinnedToCore(otaTask, "OTA Task", 16384, NULL, 2, &otaTaskHandle, 0);
        //xTaskCreatePinnedToCore(smartNetTask, "SmartNet_RX_Task", 4096, NULL, 1, &smartNetTaskHandle, 1);
    }

    void __attribute__((weak)) getModuleSpecificConfig() {
        logMessage(LOG_WARN, "⚠️ Warning: getModuleSpecificConfig() not overridden in project.");
        // Default empty implementation
    }

    void checkresetButtonTask(void *parameter) {
        for (;;) {
            bool currentButtonState = digitalRead(buttonPin) == LOW;
    
            if (currentButtonState) {
                if (!buttonPressed) {
                    buttonPressStart = millis();
                    buttonPressed = true;
                } else {
                    if (millis() - buttonPressStart >= holdTime) {
                        Serial.println("🟢 Reset module triggered — launching reset worker...");
    
                        // Clear handle BEFORE deleting self
                        resetButtonTaskHandle = NULL;
    
                        // Spawn reset worker task
                        xTaskCreatePinnedToCore(resetWorkerTask, "ResetWorker", 4096, NULL, 1, NULL, 0);
    
                        // Kill this task
                        vTaskDelete(NULL);
                    }
                }
            } else {
                buttonPressed = false;
            }
    
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    void resetWorkerTask(void *param) {
    
        SmartCore_EEPROM::resetParameters();
        
        Serial.println("🧹 Suspending other tasks...");
    
    
        TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    
        // Suspend others except this task
        //if (otaTaskHandle && otaTaskHandle != currentTask)              vTaskSuspend(otaTaskHandle);
        //if (smartNetTaskHandle && smartNetTaskHandle != currentTask)           vTaskSuspend(smartNetTaskHandle);
    
        //if (wifiMqttCheckTaskHandle && wifiMqttCheckTaskHandle != currentTask)    vTaskSuspend(wifiMqttCheckTaskHandle);
        if (flashLEDTaskHandle && flashLEDTaskHandle != currentTask)         vTaskSuspend(flashLEDTaskHandle);
        if (provisioningBlinkTaskHandle && provisioningBlinkTaskHandle != currentTask) vTaskSuspend(provisioningBlinkTaskHandle);
        if (SmartCore_WiFi::wifiManagerTaskHandle && SmartCore_WiFi::wifiManagerTaskHandle != currentTask)      vTaskSuspend(SmartCore_WiFi::wifiManagerTaskHandle);
    
        Serial.println("✅ All tasks suspended.");
        setRGBColor(0, 255, 0);  // Visual cue
    
        // Do not touch flash, EEPROM, or heavy lifting at this point
        vTaskDelay(pdMS_TO_TICKS(250));
        Serial.println("♻️ Restarting...");
        esp_restart();  // Use lower-level version to avoid any Arduino cleanup hang
    }
    
}

