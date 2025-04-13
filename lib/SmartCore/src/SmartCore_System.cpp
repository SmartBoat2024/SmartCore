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
#include "SmartCore_MQTT.h"
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <otadrive_esp.h>
#include "SmartCore_SmartNet.h"
#include "SmartCore_MCP.h"


namespace SmartCore_System {

    TaskHandle_t resetButtonTaskHandle = NULL;
    TaskHandle_t otaTaskHandle = NULL;

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
    
        // 💾 WRITE TO EEPROM
        SmartCore_EEPROM::writeStringToEEPROM(SN_ADDR, String(settings.serialNumber));
        SmartCore_EEPROM::writeModuleNameToEEPROM(settings.moduleName);
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
        SmartCore_MCP::init();
        //  SmartNet init + task
        if (SmartCore_SmartNet::initSmartNet()) {
            xTaskCreatePinnedToCore(SmartCore_SmartNet::smartNetTask, "SmartNet_RX_Task", 4096, NULL, 1, &SmartCore_SmartNet::smartNetTaskHandle, 1);
        } else {
            logMessage(LOG_ERROR, "❌ Failed to initialize SmartNet CAN bus");
        }
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

        location = SmartCore_EEPROM::readLocationFromEEPROM();
        
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

        // Read first WiFi connect flag
        firstWiFiConnect = SmartCore_EEPROM::readFirstWiFiConnectFlag();
        logMessage(LOG_INFO, "📡 firstWiFiConnect: " + String(firstWiFiConnect));

        // Read serial number assigned flag
        serialNumberAssigned = SmartCore_EEPROM::readSerialNumberAssignedFlag();
        logMessage(LOG_INFO, "🧠 serialNumberAssigned: " + String(serialNumberAssigned));
    
        isUpgradeAvailable = SmartCore_EEPROM::loadUpgradeFlag();
    
        // Read smartBoatEnabled flag
        smartBoatEnabled = EEPROM.read(SB_BOOL_ADDR) == 1;
    
        // Read smartConnectEnabled flag
        smartConnectEnabled = EEPROM.read(SC_BOOL_ADDR) == 1;
        logMessage(LOG_INFO, "smartConnect: " + String(smartConnectEnabled));
    
        moduleName = SmartCore_EEPROM::readModuleNameFromEEPROM();

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
                        logMessage(LOG_INFO, "🟢 Reset module triggered — launching reset worker...");
    
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
        SmartCore_EEPROM::resetParameters();  // ⚙️ Clean config wipe
    
        Serial.println("🧹 Suspending other tasks...");
        TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    
        if (otaTaskHandle && otaTaskHandle != currentTask)                      vTaskSuspend(otaTaskHandle);
        if (wifiMqttCheckTaskHandle && wifiMqttCheckTaskHandle != currentTask) vTaskSuspend(wifiMqttCheckTaskHandle);
        if (SmartCore_MQTT::metricsTaskHandle && SmartCore_MQTT::metricsTaskHandle != currentTask) vTaskSuspend(SmartCore_MQTT::metricsTaskHandle);
        if (SmartCore_MQTT::timeSyncTaskHandle && SmartCore_MQTT::timeSyncTaskHandle != currentTask) vTaskSuspend(SmartCore_MQTT::timeSyncTaskHandle);
        if (flashLEDTaskHandle && flashLEDTaskHandle != currentTask)           vTaskSuspend(flashLEDTaskHandle);
        if (provisioningBlinkTaskHandle && provisioningBlinkTaskHandle != currentTask) vTaskSuspend(provisioningBlinkTaskHandle);
        if (SmartCore_WiFi::wifiManagerTaskHandle && SmartCore_WiFi::wifiManagerTaskHandle != currentTask) vTaskSuspend(SmartCore_WiFi::wifiManagerTaskHandle);
        //if (smartNetTaskHandle && smartNetTaskHandle != currentTask)           vTaskSuspend(smartNetTaskHandle);
        // TODO: re-enable SmartNet suspend if needed
    
        Serial.println("✅ All tasks suspended.");
        setRGBColor(0, 255, 0);  // 🟢 Visual feedback
    
        vTaskDelay(pdMS_TO_TICKS(250));  // Let things settle
        Serial.println("♻️ Restarting...");
        esp_restart();  // 💥 Full restart
    }

    void otaTask(void *parameter) {
        while (true) {
            if (shouldUpdateFirmware) {
                logMessage(LOG_INFO, "🚀 Starting OTA update...");
    
                if (flashLEDTaskHandle) vTaskSuspend(flashLEDTaskHandle);
                if (wifiMqttCheckTaskHandle) vTaskSuspend(wifiMqttCheckTaskHandle);
                if (resetButtonTaskHandle) vTaskSuspend(resetButtonTaskHandle);
                if (provisioningBlinkTaskHandle) vTaskSuspend(provisioningBlinkTaskHandle);
                if (SmartCore_MQTT::metricsTaskHandle) vTaskSuspend(SmartCore_MQTT::metricsTaskHandle);
                if (SmartCore_MQTT::timeSyncTaskHandle) vTaskSuspend(SmartCore_MQTT::timeSyncTaskHandle);
                //if (smartNetTaskHandle && smartNetTaskHandle != currentTask) vTaskSuspend(smartNetTaskHandle);
    
                ota();  // 🔁 Call your OTA handler — must yield or watch-dog safe!
    
                if (flashLEDTaskHandle) vTaskResume(flashLEDTaskHandle);
                if (wifiMqttCheckTaskHandle) vTaskResume(wifiMqttCheckTaskHandle);
                if (resetButtonTaskHandle) vTaskResume(resetButtonTaskHandle);
                if (provisioningBlinkTaskHandle) vTaskResume(provisioningBlinkTaskHandle);
                if (SmartCore_MQTT::metricsTaskHandle) vTaskResume(SmartCore_MQTT::metricsTaskHandle);
                if (SmartCore_MQTT::timeSyncTaskHandle) vTaskResume(SmartCore_MQTT::timeSyncTaskHandle);
                //if (smartNetTaskHandle && smartNetTaskHandle != currentTask) vTaskResume(smartNetTaskHandle);
    
                logMessage(LOG_INFO, "✅ OTA complete. System tasks resumed.");
                shouldUpdateFirmware = false;
            }
    
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    
        vTaskDelete(NULL);  
    }

    void onUpdateProgress(int progress, int total) {
        int percent = (progress * 100) / total;
        static int lastPercent = -1;
    
        if (percent != lastPercent) {
            lastPercent = percent;
    
            char progressMsg[50];
            snprintf(progressMsg, sizeof(progressMsg), "Progress: %d%%\n", percent);
            Serial.print(progressMsg);
    
            DynamicJsonDocument progressDoc(256);
            progressDoc["type"] = "otaProgress";
            progressDoc["serial"] = serialNumber;
            progressDoc["progress"] = percent;
    
            String progressJson;
            serializeJson(progressDoc, progressJson);
    
            #if WEBSERVER_ENABLED
            ws.textAll(progressJson);
            #endif
    
            if (mqttIsConnected) {
                mqttClient.publish((String(mqttPrefix) + "/ota/progress").c_str(), 0, false, progressJson.c_str());
            }
        }
    }

    void ota() {
        Serial.println("Calling OTADRIVE.updateFirmware()...");
        updateInfo result = OTADRIVE.updateFirmware();
    
        DynamicJsonDocument resultDoc(128);
        resultDoc["type"] = "otaProgress";
        resultDoc["progress"] = result.available ? 100 : -1;
    
        String resultJson;
        serializeJson(resultDoc, resultJson);
    
        #if WEBSERVER_ENABLED
            ws.textAll(resultJson);  // Guarded by flag
        #endif
    
        if (result.available) {
            logMessage(LOG_INFO, "ota update successful");
            isUpgradeAvailable = false;
            logMessage(LOG_INFO, "OTA update completed successfully. Version: " + String(result.version));
        } else {
            Serial.println("ota update failed!!");
            logMessage(LOG_WARN, "OTA update failed or no update available.");
        }
    }
    
}

