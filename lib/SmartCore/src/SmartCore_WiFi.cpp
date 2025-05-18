#include "SmartCore_Wifi.h"
#include "SmartCore_MQTT.h"
#include "SmartCore_EEPROM.h"
#include "SmartCore_LED.h"
#include "SmartCore_Log.h"
#include "SmartCore_config.h"
#include "SmartCore_Network.h"
#include "esp_task_wdt.h"
#include "SmartCore_System.h"
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

namespace SmartCore_WiFi {

    TaskHandle_t wifiProvisionTaskHandle = NULL;

    static char apName[32];
    static char apPassword[32];

    // Local state (TODO: move globals to a struct if needed)
    AsyncWebServer server(5000);


    // Public entry point
    void startWiFiProvisionTask() {
        xTaskCreatePinnedToCore(wifiProvisionTask, "WiFiManager Task", 8192, NULL, 1, &wifiProvisionTaskHandle, 1);
    }

    void wifiProvisionTask(void* parameter) {
        logMessage(LOG_INFO, "📡 Starting WiFi Provisioning Task...");

        // Flags
        resetConfig = SmartCore_EEPROM::readResetConfigFlag();
        wifiSetupComplete = SmartCore_EEPROM::readBoolFromEEPROM(WIFI_SETUP_COMPLETE_ADDR);

        // Load WiFi fail counter
        uint8_t wifiFailCounter = EEPROM.read(WIFI_FAIL_COUNTER_ADDR);

        // ---- RESET CONFIG: Force AP for Provisioning ----
        if (resetConfig) {
            SmartCore_EEPROM::resetParameters();
            logMessage(LOG_INFO, "🔁 resetConfig = true → Starting provisioning AP...");

            // Unique AP SSID: <prefix>_<MAC>
            uint8_t mac[6];
            WiFi.softAPmacAddress(mac);
            char macSuffix[7];
            sprintf(macSuffix, "%02X%02X%02X", mac[3], mac[4], mac[5]);
            String apSsid = String(SmartCore_System::getModuleSettings().apSSID) + "_" + String(macSuffix);

            WiFi.mode(WIFI_AP);
            WiFi.softAP(apSsid.c_str(), nullptr);

            logMessage(LOG_INFO, "🔊 Provisioning AP started. SSID: " + apSsid);

            setLEDMode(LEDMODE_WAITING);
            currentProvisioningState = PROVISIONING_PORTAL;

            // HTTP POST /setup endpoint (as in earlier examples)
            server.on("/setup", HTTP_POST, [](AsyncWebServerRequest *request){},
            NULL,
            [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                String body = "";
                for (size_t i = 0; i < len; ++i) body += (char)data[i];
                Serial.println("Received setup POST:");
                Serial.println(body);

                DynamicJsonDocument doc(512);
                DeserializationError err = deserializeJson(doc, body);
                if (err) {
                request->send(400, "application/json", "{\"status\":\"fail\",\"msg\":\"Bad JSON\"}");
                return;
                }
                String ssid = doc["ssid"] | "";
                String pass = doc["password"] | "";
                String mqttIp = doc["mqttIp"] | "";
                String mqttPort = doc["mqttPort"] | "";
                String serial = doc["serialNumber"] | "";

                logMessage(LOG_INFO, "📥 Received WiFi/MQTT creds via provisioning:");
                logMessage(LOG_INFO, "  📶 SSID: " + ssid);
                logMessage(LOG_INFO, "  🔐 PASS: " + pass);
                logMessage(LOG_INFO, "  🟦 MQTT: " + mqttIp + ":" + mqttPort);
                logMessage(LOG_INFO, "  #️⃣ Serial: " + serial);

                SmartCore_EEPROM::writeStringToEEPROM(WIFI_SSID_ADDR, ssid, 41);
                SmartCore_EEPROM::writeStringToEEPROM(WIFI_PASS_ADDR, pass, 41);
                SmartCore_EEPROM::writeStringToEEPROM(MQTT_IP_ADDR, mqttIp, 17);
                SmartCore_EEPROM::writeStringToEEPROM(MQTT_PORT_ADDR, mqttPort, 7);
                SmartCore_EEPROM::writeStringToEEPROM(SN_ADDR, serial, 41);
                SmartCore_EEPROM::writeBoolToEEPROM(WIFI_SETUP_COMPLETE_ADDR, true);

                // Reset WiFi fail counter after successful provisioning
                EEPROM.write(WIFI_FAIL_COUNTER_ADDR, 0);
                EEPROM.commit();

                resetConfig = false;
                SmartCore_EEPROM::writeResetConfigFlag(false);

                logMessage(LOG_INFO, "💾 WiFi/MQTT creds saved to EEPROM. Rebooting...");

                setLEDMode(LEDMODE_STATUS); 

                vTaskDelay(pdMS_TO_TICKS(250));

                request->send(200, "application/json", "{\"status\":\"success\",\"msg\":\"Provisioned. Rebooting.\"}");
                Serial.println("Provisioning response sent!");

                // Reboot after response sent
                request->onDisconnect([]() {
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    ESP.restart();
                });
            });
            server.begin();

            // Block forever (will reboot on provisioning)
            while (true) vTaskDelay(pdMS_TO_TICKS(200));
        }

        // ---- NORMAL BOOT: Try STA, Retry, Safe Mode if Needed ----
        if (wifiSetupComplete) {
            SmartCore_System::getModuleConfig();
            String ssid = SmartCore_EEPROM::readStringFromEEPROM(WIFI_SSID_ADDR, 41);
            String pass = SmartCore_EEPROM::readStringFromEEPROM(WIFI_PASS_ADDR, 41);
            String mqttIp = SmartCore_EEPROM::readStringFromEEPROM(MQTT_IP_ADDR, 17);
            String mqttPort = SmartCore_EEPROM::readStringFromEEPROM(MQTT_PORT_ADDR, 7);
            String serial = SmartCore_EEPROM::readStringFromEEPROM(SN_ADDR, 41);

            logMessage(LOG_INFO, "🔍 [POST-READ] EEPROM DEBUG:");
            logMessage(LOG_INFO, "  📶 SSID: " + ssid);
            logMessage(LOG_INFO, "  🔐 PASS: " + pass);
            logMessage(LOG_INFO, "  🟦 MQTT: " + mqttIp + ":" + mqttPort);
            logMessage(LOG_INFO, "  #️⃣ Serial: " + serial);

            WiFi.mode(WIFI_STA);

            int retry = 0;
            bool connected = false;
            unsigned long backoffMs = WIFI_RETRY_START_MS;

            while (retry < WIFI_RETRY_LIMIT && !connected) {
                WiFi.disconnect();
                WiFi.begin(ssid.c_str(), pass.c_str());
                unsigned long start = millis();
                while (WiFi.status() != WL_CONNECTED && millis() - start < backoffMs) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                if (WiFi.status() == WL_CONNECTED) {
                    connected = true;
                    // Reset WiFi fail counter on successful connect
                    EEPROM.write(WIFI_FAIL_COUNTER_ADDR, 0);
                    EEPROM.commit();
                    break;
                } else {
                    logMessage(LOG_WARN, "❌ WiFi connect attempt failed, retrying...");
                    retry++;
                    backoffMs *= 2; // Exponential backoff
                }
            }

            if (connected) {
                logMessage(LOG_INFO, "✅ Connected as WiFi client.");
                setLEDMode(LEDMODE_STATUS);
                startNetworkServices(true);
            } else {
                // Increment and store WiFi fail counter
                wifiFailCounter++;
                EEPROM.write(WIFI_FAIL_COUNTER_ADDR, wifiFailCounter);
                EEPROM.commit();

                if (wifiFailCounter >= WIFI_FAIL_LIMIT_SAFE) {
                    logMessage(LOG_ERROR, "🚨 Too many WiFi boot fails! Entering Safe Mode for recovery.");
                    SmartCore_System::enterSafeMode();  // Reuse your robust Safe Mode recovery UI/task
                } else {
                    logMessage(LOG_WARN, "🔄 Will retry WiFi on next boot (fail count: " + String(wifiFailCounter) + ")");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    ESP.restart();
                }
            }
        } else {
            // This case: Not resetConfig, but no setup complete (shouldn’t usually occur)
            logMessage(LOG_WARN, "⚠️ Not provisioned and not in reset state — falling back to AP for provisioning.");
            resetConfig = true;
            SmartCore_EEPROM::writeResetConfigFlag(true);
            vTaskDelay(pdMS_TO_TICKS(500));
            ESP.restart();
        }

        vTaskDelete(NULL);
    }
    
    void handleFailedConnection() {
        logMessage(LOG_INFO, "🚨 handleFailedConnection() triggered.");
        logMessage(LOG_INFO, "⚠️ WiFi credentials likely invalid — flashing error pattern...");
        triggerFlashPattern(".....", DEBUG_COLOR_RED);  // Five short red blinks

        vTaskDelay(pdMS_TO_TICKS(3000));  // Allow partial flashing before restart
        ESP.restart();  // Or use resetModule() if cleanup is needed
        
    }

    void startNetworkServices(bool mqttRequired = true) {
        logMessage(LOG_INFO, "🌐 Starting Network Services...");
        waitForValidIP(); // Wait for DHCP/IP ready
        vTaskDelay(pdMS_TO_TICKS(200)); // Small extra buffer

        if (mqttRequired) {
            SmartCore_MQTT::setupMQTTClient();
        } else {
            Serial.println("🚫 MQTT skipped (not required)");
        }
        SmartCore_System::createAppTasks();
    }

    void waitForValidIP() {
        int tries = 0;
        while ((WiFi.localIP() == IPAddress(0,0,0,0)) && (tries++ < 20)) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    //SAFE CONNECT FOR RECOVERY
    void startMinimalWifiSetup() {
        logMessage(LOG_INFO, "🛟 Safe Mode: Starting minimal AP for recovery.");

        WiFi.disconnect(true, true);
        delay(100);
        WiFi.mode(WIFI_AP);
        String apSsid = String(SmartCore_System::getModuleSettings().apSSID) + "_RECOVERY";
        WiFi.softAP(apSsid.c_str(), nullptr);
        logMessage(LOG_INFO, "🛟 Safe Mode AP SSID: " + apSsid);

        static AsyncWebServer safeServer(81);
        // [ ... add simple HTML UI/handlers for "Clear EEPROM", "Reboot", "OTA", etc ... ]
        safeServer.begin();

        while (true) delay(1000); // Loop forever
    }

}