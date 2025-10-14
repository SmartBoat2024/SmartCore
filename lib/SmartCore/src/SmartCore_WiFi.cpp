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

        resetConfig = SmartCore_EEPROM::readResetConfigFlag();
        //resetConfig = true;
        wifiSetupComplete = SmartCore_EEPROM::readBoolFromEEPROM(WIFI_SETUP_COMPLETE_ADDR);

        Serial.printf("wifiSetupComplete flag = %s\n", wifiSetupComplete ? "true" : "false");
        Serial.printf("resetConfig: %d\n", resetConfig);

        // ─────────────────────────────────────────────
        // STEP 1: FORCED PROVISIONING (Reset button held)
        if (resetConfig) {
            SmartCore_EEPROM::resetParameters();
            logMessage(LOG_INFO, "🔁 ResetConfig = true → Starting provisioning AP...");

            uint8_t mac[6];
            WiFi.softAPmacAddress(mac);
            char macSuffix[7];
            sprintf(macSuffix, "%02X%02X%02X", mac[3], mac[4], mac[5]);
            String apSsid = String(SmartCore_System::getModuleSettings().apSSID) + "_" + String(macSuffix);

            WiFi.mode(WIFI_AP);
            WiFi.softAP(apSsid.c_str(), nullptr);

            setLEDMode(LEDMODE_WAITING);
            currentProvisioningState = PROVISIONING_PORTAL;

            server.on("/setup", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
            [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                String body = String((char*)data, len);
                DynamicJsonDocument doc(512);
                if (deserializeJson(doc, body)) {
                    request->send(400, "application/json", "{\"status\":\"fail\",\"msg\":\"Bad JSON\"}");
                    return;
                }

                SmartCore_EEPROM::writeStringToEEPROM(WIFI_SSID_ADDR, doc["ssid"] | "", 41);
                SmartCore_EEPROM::writeStringToEEPROM(WIFI_PASS_ADDR, doc["password"] | "", 41);
                SmartCore_EEPROM::writeStringToEEPROM(MQTT_IP_ADDR, doc["mqttIp"] | "", 17);
                SmartCore_EEPROM::writeStringToEEPROM(MQTT_PORT_ADDR, doc["mqttPort"] | "", 7);
                SmartCore_EEPROM::writeStringToEEPROM(SN_ADDR, doc["serialNumber"] | "", 41);
                SmartCore_EEPROM::writeBoolToEEPROM(WIFI_SETUP_COMPLETE_ADDR, true);
                EEPROM.write(WIFI_FAIL_COUNTER_ADDR, 0);
                EEPROM.commit();

                SmartCore_EEPROM::writeResetConfigFlag(false);
                resetConfig = false;

                setLEDMode(LEDMODE_STATUS);
                request->send(200, "application/json", "{\"status\":\"success\",\"msg\":\"Provisioned. Rebooting.\"}");
                request->onDisconnect([]() {
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    ESP.restart();
                });
            });

            server.begin();
            while (true) {
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }

        // ─────────────────────────────────────────────
        // STEP 2: NORMAL BOOT — TRY INITIAL CONNECTION
        if (wifiSetupComplete) {
            SmartCore_System::getModuleConfig();
            String ssid = SmartCore_EEPROM::readStringFromEEPROM(WIFI_SSID_ADDR, 41);
            String pass = SmartCore_EEPROM::readStringFromEEPROM(WIFI_PASS_ADDR, 41);

            logMessage(LOG_INFO, "🧪 [WiFi DEBUG] Attempting initial connection");
            logMessage(LOG_INFO, "📶 SSID=[" + ssid + "] (" + String(ssid.length()) + ")");
            logMessage(LOG_INFO, "🔐 PASS=[" + pass + "] (" + String(pass.length()) + ")");

            WiFi.mode(WIFI_STA);
            bool connected = false;

            unsigned long backoff = WIFI_RETRY_START_MS;

            while (!connected) {
                logMessage(LOG_INFO, "🔄 WiFi attempt with backoff: " + String(backoff) + " ms");
                logMessage(LOG_INFO, "🧪 [WiFi DEBUG] Attempting initial connection");
                logMessage(LOG_INFO, "📶 SSID=[" + ssid + "] (" + String(ssid.length()) + ")");
                logMessage(LOG_INFO, "🔐 PASS=[" + pass + "] (" + String(pass.length()) + ")");
                WiFi.disconnect(true, false);
                WiFi.begin(ssid.c_str(), pass.c_str());

                unsigned long start = millis();
                while (WiFi.status() != WL_CONNECTED && millis() - start < backoff) {
                    vTaskDelay(pdMS_TO_TICKS(200));
                }

                if (WiFi.status() == WL_CONNECTED) {
                    connected = true;
                    logMessage(LOG_INFO, "✅ Initial WiFi connection successful");
                    EEPROM.write(WIFI_FAIL_COUNTER_ADDR, 0);
                    EEPROM.commit();
                    startNetworkServices(true);
                    break;
                }

                logMessage(LOG_WARN, "❌ Initial WiFi attempt failed. Retrying...");
                backoff = min(backoff * 2, (unsigned long)60000); // cap at 60s
            }

            // 💡 Once WiFi is up → the monitor task takes over
            setLEDMode(LEDMODE_STATUS);
        } else {
            // 💣 No provisioning + not in reset = force reset mode
            logMessage(LOG_WARN, "⚠️ Not provisioned and not in reset state — forcing AP provisioning");
            SmartCore_EEPROM::writeResetConfigFlag(true);
            delay(500);
            ESP.restart();
        }

        vTaskDelete(NULL);
    }

    void startRecoveryServer() {
    logMessage(LOG_INFO, "🌐 Starting Recovery Server...");

    // --- Endpoint: GET /provisioned.json ---
    server.on("/provisioned.json", HTTP_GET, [](AsyncWebServerRequest* request) {
        DynamicJsonDocument doc(512);
        doc["ssid"] = SmartCore_EEPROM::readStringFromEEPROM(WIFI_SSID_ADDR, 41);
        doc["password"] = SmartCore_EEPROM::readStringFromEEPROM(WIFI_PASS_ADDR, 41);
        doc["mqttIp"] = SmartCore_EEPROM::readStringFromEEPROM(MQTT_IP_ADDR, 17);
        doc["mqttPort"] = SmartCore_EEPROM::readStringFromEEPROM(MQTT_PORT_ADDR, 7);
        doc["serialNumber"] = SmartCore_EEPROM::readStringFromEEPROM(SN_ADDR, 41);

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);

        logMessage(LOG_INFO, "📤 Sent /provisioned.json to recovery client.");
        logMessage(LOG_INFO, "📡 AP IP: " + WiFi.softAPIP().toString());
    });

    // --- Endpoint: POST /setup ---
    server.on("/setup", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        String body = String((char*)data, len);
        DynamicJsonDocument doc(512);
        if (deserializeJson(doc, body)) {
            request->send(400, "application/json", "{\"status\":\"fail\",\"msg\":\"Bad JSON\"}");
            return;
        }

        const String ssid = doc["ssid"] | "";
        const String pass = doc["password"] | "";
        const String mqttIp = doc["mqttIp"] | "";
        const String mqttPort = doc["mqttPort"] | "";
        const String serial = doc["serialNumber"] | "";
        const bool credsVerified = doc["verified"] | false;

        if (credsVerified) {
            logMessage(LOG_INFO, "✅ App verified existing WiFi creds → rebooting and retrying...");
            logMessage(LOG_INFO, "  📶 SSID: " + SmartCore_EEPROM::readStringFromEEPROM(WIFI_SSID_ADDR, 41));
        } else {
            logMessage(LOG_INFO, "📥 Updated WiFi/MQTT credentials from recovery app:");
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
            EEPROM.write(WIFI_FAIL_COUNTER_ADDR, 0);
            EEPROM.commit();
        }

        setLEDMode(LEDMODE_STATUS);
        request->send(200, "application/json", "{\"status\":\"success\",\"msg\":\"Rebooting to connect.\"}");

        request->onDisconnect([]() {
            vTaskDelay(pdMS_TO_TICKS(1500));
            WiFi.disconnect(true, true); // Clean exit
            ESP.restart();
        });
    });

    // --- Optional: health ping ---
    server.on("/ping", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "pong");
    });

    server.begin();
    logMessage(LOG_INFO, "✅ Recovery Server running (AP mode)");
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

        // 🧠 Start WiFi/MQTT health check task
        if (wifiMqttCheckTaskHandle == nullptr) {
            xTaskCreatePinnedToCore(
                wifiMqttCheckTask,
                "WiFi/MQTT Check",
                4096,
                NULL,
                1,
                &wifiMqttCheckTaskHandle,
                0
            );
        }
    
        SmartCore_System::createAppTasks();
    }

    void waitForValidIP() {
        int tries = 0;
        while ((WiFi.localIP() == IPAddress(0,0,0,0)) && (tries++ < 20)) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

}