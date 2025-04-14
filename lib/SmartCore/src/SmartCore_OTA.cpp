#include "SmartCore_OTA.h"
#include <otadrive_esp.h>
#include <ArduinoJson.h>
#include "SmartCore_LED.h"
#include "SmartCore_System.h"
#include "SmartCore_Log.h"
#include "SmartCore_MQTT.h"
#include "SmartCore_Network.h"
#include "SmartCore_SmartNet.h"
#include <AsyncWebSocket.h> 


namespace SmartCore_OTA {
    bool isUpgradeAvailable = false;
    bool shouldUpdateFirmware = false;

    TaskHandle_t otaTaskHandle = NULL;
    AsyncWebSocket safeWs("/ws");

    void otaTask(void *parameter) {
        while (true) {
            if (shouldUpdateFirmware) {
                logMessage(LOG_INFO, "🚀 Starting OTA update...");

                if (flashLEDTaskHandle) vTaskSuspend(flashLEDTaskHandle);
                if (wifiMqttCheckTaskHandle) vTaskSuspend(wifiMqttCheckTaskHandle);
                if (SmartCore_System::resetButtonTaskHandle) vTaskSuspend(SmartCore_System::resetButtonTaskHandle);
                if (provisioningBlinkTaskHandle) vTaskSuspend(provisioningBlinkTaskHandle);
                if (SmartCore_MQTT::metricsTaskHandle) vTaskSuspend(SmartCore_MQTT::metricsTaskHandle);
                if (SmartCore_MQTT::timeSyncTaskHandle) vTaskSuspend(SmartCore_MQTT::timeSyncTaskHandle);
                if (SmartCore_SmartNet::smartNetTaskHandle) vTaskSuspend(SmartCore_SmartNet::smartNetTaskHandle);

                ota();  // 🔁 Call your OTA handler — must yield or watch-dog safe!

                if (flashLEDTaskHandle) vTaskResume(flashLEDTaskHandle);
                if (wifiMqttCheckTaskHandle) vTaskResume(wifiMqttCheckTaskHandle);
                if (SmartCore_System::resetButtonTaskHandle) vTaskResume(SmartCore_System::resetButtonTaskHandle);
                if (provisioningBlinkTaskHandle) vTaskResume(provisioningBlinkTaskHandle);
                if (SmartCore_MQTT::metricsTaskHandle) vTaskResume(SmartCore_MQTT::metricsTaskHandle);
                if (SmartCore_MQTT::timeSyncTaskHandle) vTaskResume(SmartCore_MQTT::timeSyncTaskHandle);
                if (SmartCore_SmartNet::smartNetTaskHandle) vTaskResume(SmartCore_SmartNet::smartNetTaskHandle);

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
        static int lastSafeWsPercent = -5;
    
        if (percent != lastPercent) {
            lastPercent = percent;
    
            char progressMsg[50];
            snprintf(progressMsg, sizeof(progressMsg), "Progress: %d%%\n", percent);
            Serial.print(progressMsg);
    
            // 🌍 Standard JSON with full info
            DynamicJsonDocument fullDoc(256);
            fullDoc["type"] = "otaProgress";
            fullDoc["serial"] = serialNumber;
            fullDoc["progress"] = percent;
    
            String fullJson;
            serializeJson(fullDoc, fullJson);
    
            #if WEBSERVER_ENABLED
            ws.textAll(fullJson); // Standard UI WS
            #endif
    
            if (mqttIsConnected) {
                mqttClient.publish((String(mqttPrefix) + "/ota/progress").c_str(), 0, false, fullJson.c_str());
            }
    
            // ✅ Throttle updates to Safe Mode WS (every 5%)
            if (SmartCore_OTA::safeWs.count() > 0 && percent - lastSafeWsPercent >= 5) {
                if (percent >= 100) {
                    SmartCore_System::clearCrashCounter(CRASH_COUNTER_ALL);
                }

                DynamicJsonDocument safeDoc(64);
                safeDoc["progress"] = percent;
    
                String safeJson;
                serializeJson(safeDoc, safeJson);
    
                Serial.println("🧪 Sending JSON to recovery UI:");
                Serial.println(safeJson);
                Serial.printf("🧪 safeWs.count() = %d\n", SmartCore_OTA::safeWs.count());
    
                SmartCore_OTA::safeWs.textAll(safeJson);
                lastSafeWsPercent = percent;
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