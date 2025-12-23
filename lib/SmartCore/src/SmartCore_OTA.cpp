#include "SmartCore_OTA.h"
#include <otadrive_esp.h>
#include <ArduinoJson.h>
#include "SmartCore_LED.h"
#include "SmartCore_System.h"
#include "SmartCore_Log.h"
#include "SmartCore_MQTT.h"
#include "SmartCore_Network.h"
#include "SmartCore_SmartNet.h"
#include "SmartCore_EEPROM.h"
#include "FirmwareVersion.h"
#include "config.h"

// Removed: #include <AsyncWebSocket.h>

namespace SmartCore_OTA {
    bool isUpgradeAvailable = false;
    bool shouldUpdateFirmware = false;
    volatile bool otaInProgress = false;

    TaskHandle_t otaTaskHandle = NULL;

    void otaTask(void *parameter) {
        while (true) {

            if (otaInProgress)
            {
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }

            if (shouldUpdateFirmware) {
                logMessage(LOG_INFO, "🚀 Starting OTA update...");
                otaInProgress = true;

                if (flashLEDTaskHandle) vTaskSuspend(flashLEDTaskHandle);
                if (wifiMqttCheckTaskHandle) vTaskSuspend(wifiMqttCheckTaskHandle);
                if (SmartCore_System::resetButtonTaskHandle) vTaskSuspend(SmartCore_System::resetButtonTaskHandle);
                if (provisioningBlinkTaskHandle) vTaskSuspend(provisioningBlinkTaskHandle);
                if (SmartCore_MQTT::metricsTaskHandle) vTaskSuspend(SmartCore_MQTT::metricsTaskHandle);
                if (SmartCore_MQTT::timeSyncTaskHandle) vTaskSuspend(SmartCore_MQTT::timeSyncTaskHandle);
                if (SmartCore_SmartNet::smartNetTaskHandle) vTaskSuspend(SmartCore_SmartNet::smartNetTaskHandle);

                logMessage(LOG_INFO, "🚀 Clearing Crash Counters - OTA update...");
                SmartCore_System::clearCrashCounter(CRASH_COUNTER_ALL);

                ota();  // 🔁 OTA handler

                if (flashLEDTaskHandle) vTaskResume(flashLEDTaskHandle);
                if (wifiMqttCheckTaskHandle) vTaskResume(wifiMqttCheckTaskHandle);
                if (SmartCore_System::resetButtonTaskHandle) vTaskResume(SmartCore_System::resetButtonTaskHandle);
                if (provisioningBlinkTaskHandle) vTaskResume(provisioningBlinkTaskHandle);
                if (SmartCore_MQTT::metricsTaskHandle) vTaskResume(SmartCore_MQTT::metricsTaskHandle);
                if (SmartCore_MQTT::timeSyncTaskHandle) vTaskResume(SmartCore_MQTT::timeSyncTaskHandle);
                if (SmartCore_SmartNet::smartNetTaskHandle) vTaskResume(SmartCore_SmartNet::smartNetTaskHandle);

                logMessage(LOG_INFO, "✅ OTA complete. System tasks resumed.");
                otaInProgress = false;
                shouldUpdateFirmware = false;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        vTaskDelete(NULL);  
    }

    void onUpdateProgress(int progress, int total) {
        // Defensive: Flag OTA in progress, not available!
        isUpgradeAvailable = false;
        SmartCore_EEPROM::saveUpgradeFlag(false);

        int percent = (progress * 100) / total;
        static int lastPercent = -1;

        if (percent != lastPercent) {
            lastPercent = percent;
            DynamicJsonDocument doc(128);
            doc["type"] = "otaProgress";
            doc["serialNumber"] = serialNumber;
            doc["progress"] = percent;
            String json;
            serializeJson(doc, json);
            if (mqttIsConnected && mqttClient) {
                mqttClient->publish("module/upgrade", 0, false, json.c_str());
            }
            Serial.printf("OTA Progress: %d%% published to module/upgrade\n", percent);
        }
    }

    void ota() {
        // At the start, clear the flag
        isUpgradeAvailable = false;
        SmartCore_EEPROM::saveUpgradeFlag(false);

        StaticJsonDocument<256> status_response;
        status_response["firmwareVersion"] = FW_VER;
        status_response["updateAvailable"] = isUpgradeAvailable;

        String payload_status;
        serializeJson(status_response, payload_status);
        SmartCore_MQTT::mqttSafePublish("module/config/update", 1, true, payload_status.c_str());

        Serial.println("✅ Published generic module config");

        Serial.println("Calling OTADRIVE.updateFirmware()...");
        updateInfo result = OTADRIVE.updateFirmware(); // ESP reboots if successful

        // If code continues, OTA probably failed or was not available
        // Re-check upgrade availability just in case
        if (!result.available) {
            // OTA failed, check again
            updateInfo checkResult = OTADRIVE.updateFirmwareInfo();
            if (checkResult.available) {
                isUpgradeAvailable = true;
                SmartCore_EEPROM::saveUpgradeFlag(true);
            } else {
                isUpgradeAvailable = false;
                SmartCore_EEPROM::saveUpgradeFlag(false);
            }
        }

        // Publish current status/config
        StaticJsonDocument<256> response;
        response["firmwareVersion"] = FW_VER;
        response["updateAvailable"] = isUpgradeAvailable;

        String payload;
        serializeJson(response, payload);
        SmartCore_MQTT::mqttSafePublish("module/config/update", 1, true, payload.c_str());

        Serial.println("✅ Published generic module config");

        if (result.available) {
            logMessage(LOG_INFO, "OTA update completed successfully. Version: " + String(result.version));
        } else {
            Serial.println("ota update failed!!");
            logMessage(LOG_WARN, "OTA update failed or no update available.");
        }
    }

    void initOtaKey()
{
    const char* selectedKey;
    const char* reportedVersion;

    if (SmartCore_System::bootSafeMode)
    {
        selectedKey = SAFE_APIKEY;
        reportedVersion = "v0.0.1";   // sentinel, intentionally low
    }
    else
    {
        selectedKey = APIKEY;
        reportedVersion = FW_VER;     // real firmware version
    }

    OTADRIVE.setInfo(selectedKey, reportedVersion);

    Serial.printf(
        "🔐 OTA identity: %s | reported version: %s\n",
        SmartCore_System::bootSafeMode ? "SAFE" : "PRIMARY",
        reportedVersion
    );
}
}
