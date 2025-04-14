#include "SmartCore_MQTT.h"
#include <arduino.h>
#include <AsyncMqttClient.h>
#include "SmartCore_Network.h"
#include "SmartCore_LED.h"
#include "SmartCore_EEPROM.h"
#include "SmartCore_Log.h"
#include <functional>
#include <ArduinoJson.h>
#include "driver/temp_sensor.h"
#include "SmartCore_System.h"
#include <otadrive_esp.h>
#include "mqtt_handlers.h"
#include "SmartCore_OTA.h"

namespace SmartCore_MQTT {
    char mqttWillTopic[64];
    TaskHandle_t metricsTaskHandle = NULL;
    TaskHandle_t timeSyncTaskHandle = NULL;
    void onMqttConnect(bool sessionPresent);
    void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
    void mqttReconnectTask(void *param);

    static std::function<void(bool)> mqttConnectCallback = SmartCore_MQTT::onMqttConnect;
    static std::function<void(AsyncMqttClientDisconnectReason)> mqttDisconnectCallback = SmartCore_MQTT::onMqttDisconnect;

    TaskHandle_t mqttReconnectTaskHandle = NULL;

    void generateMqttPrefix() {
        // Set mqttPrefix based on the first 4 chars of serialNumber
        strncpy(mqttPrefix, serialNumber, 4);
        mqttPrefix[4] = '\0';  // Ensure null-termination
    
        // Build the mqttWillTopic from the prefix
        snprintf(mqttWillTopic, sizeof(mqttWillTopic), "%s/disconnected", mqttPrefix);
        mqttWillTopic[sizeof(mqttWillTopic) - 1] = '\0';  // Safety null-termination
    
        // Debugging
        logMessage(LOG_INFO, "🧠 MQTT prefix: " + String(mqttPrefix));
        logMessage(LOG_INFO, "🧠 MQTT will topic: " + String(mqttWillTopic));
    
    }

    void setupMQTTClient() {
        logMessage(LOG_INFO, "🔧 Configuring MQTT...");
        generateMqttPrefix();  // Also sets mqttWillTopic internally
    
        // Set up callbacks and keepalive
        mqttClient.onConnect(onMqttConnect);
        mqttClient.onDisconnect(onMqttDisconnect);
        mqttClient.onMessage(onMqttMessage);
        mqttClient.setKeepAlive(15);
    
        // Configure Will
        mqttClient.setWill(mqttWillTopic, 1, true, serialNumber, strlen(serialNumber));
        logMessage(LOG_INFO, "🧠 MQTT will topic: " + String(mqttWillTopic));
        logMessage(LOG_INFO, "🧠 MQTT will payload: " + String(serialNumber));
        logMessage(LOG_INFO, "🧠 MQTT will configured using setWill().");
    
        // Determine MQTT server & port
        const char* resolvedServer = nullptr;
        int resolvedPort = 0;
    
        if (customMqtt) {
            resolvedServer = custom_mqtt_server;
            resolvedPort = custom_mqtt_port;
            logMessage(LOG_INFO, "🌐 Using custom MQTT server (customMqtt = true)");
        } else if (smartBoatEnabled) {
            String ip = WiFi.localIP().toString();
            int lastDot = ip.lastIndexOf('.');
            if (lastDot != -1)
                ip = ip.substring(0, lastDot + 1) + "20";
    
            strncpy(mqtt_server, ip.c_str(), sizeof(mqtt_server) - 1);
            mqtt_server[sizeof(mqtt_server) - 1] = '\0';
    
            resolvedServer = mqtt_server;
            resolvedPort = mqtt_port;
    
            logMessage(LOG_INFO, "⚓ SmartBoat mode enabled (smartBoatEnabled = true)");
            logMessage(LOG_INFO, "➡️  Inferred MQTT IP: " + ip);
        } else {
            logMessage(LOG_WARN, "⚠️ No valid MQTT configuration found. Exiting setup.");
            return;
        }
    
        // Final configuration
        mqttClient.setServer(resolvedServer, resolvedPort);
        logMessage(LOG_INFO, "📡 MQTT server set to: " + String(resolvedServer));
        logMessage(LOG_INFO, "📟 MQTT port set to: " + String(resolvedPort));
    
        // Connect if WiFi is ready
        if (WiFi.status() == WL_CONNECTED) {
            logMessage(LOG_INFO, "🔗 WiFi connected. Attempting MQTT connection...");
            Serial.print("📣 [DEBUG] Attempting connection to: ");
            Serial.print(resolvedServer);
            Serial.print(":");
            Serial.println(resolvedPort);
    
            Serial.print("📶 WiFi status: ");
            Serial.println(WiFi.status());
    
            Serial.print("🧠 MQTT is connected: ");
            Serial.println(mqttClient.connected() ? "yes" : "no");
    
            mqttClient.connect();
        }  else {
            logMessage(LOG_WARN, "📶 WiFi not connected, skipping MQTT connect.");
        }
    }

    void onMqttConnect(bool sessionPresent) {
        Serial.println("Connected to MQTT broker.");
        mqttIsConnected = true;
    
        if (mqttReconnectTaskHandle) {
            vTaskDelete(mqttReconnectTaskHandle);
            mqttReconnectTaskHandle = NULL;
            Serial.println("🛑 Reconnect task stopped");
        }
    
        // Subscribe to topics
        mqttClient.subscribe((String(mqttPrefix) + "/#").c_str(), 1);
    
        // Publish initial presence messages
        mqttClient.publish((String(mqttPrefix) + "/connected").c_str(), 1, true, "connected");
        requestSmartBoatTime();
        
        logMessage(LOG_INFO, "🔍 firstWifiCOnnect: " + String(firstWiFiConnect ? "true" : "false"));
        if (firstWiFiConnect) {
            logMessage(LOG_INFO, "🚀 firstWiFiConnect is TRUE — preparing to publish newModule message...");
        
            logMessage(LOG_INFO, "🔍 serialNumberAssigned: " + String(serialNumberAssigned ? "true" : "false"));
            logMessage(LOG_INFO, "🔍 serialNumber: " + String(serialNumber));
            logMessage(LOG_INFO, "🔍 mqttPrefix: " + String(mqttPrefix));
        
            char topic[50];
            snprintf(topic, sizeof(topic), (String(mqttPrefix) + "/newModule").c_str());
            logMessage(LOG_INFO, String("🧭 Publishing to topic: ") + topic);
        
            DynamicJsonDocument doc(256);
            doc["mac"] = WiFi.macAddress();
            doc["ip"] = WiFi.localIP().toString();
        
            if (serialNumberAssigned) {
                doc["serialNumber"] = serialNumber;
            } else {
                doc["serialNumber"] = "unassigned";
            }
        
            String payload;
            serializeJson(doc, payload);
            logMessage(LOG_INFO, "📦 Payload to publish: " + payload);
        
            if (mqttClient.connected()) {
                mqttClient.publish(topic, 1, true, payload.c_str());
                logMessage(LOG_INFO, "✅ MQTT publish call made.");
            } else {
                logMessage(LOG_WARN, "❌ MQTT client not connected — skipping publish.");
            }
        
            firstWiFiConnect = false;
            SmartCore_EEPROM::writeFirstWiFiConnectFlag(false);
            logMessage(LOG_INFO, "📝 firstWiFiConnect flag set to false and saved.");
        } else {
            logMessage(LOG_INFO, "⏭️ Skipping newModule publish — firstWiFiConnect is false.");
        }
        
        // 🌡️ Start metrics task
        if (!metricsTaskHandle) {
            xTaskCreate(metricsTask, "metricsTask", 4096, nullptr, 1, &metricsTaskHandle);
        }

        //start time get task
        if (!timeSyncTaskHandle) {
            xTaskCreatePinnedToCore(timeSyncTask, "timeSyncTask", 4096, nullptr, 1, &timeSyncTaskHandle, 0);
            Serial.println("🕓 Time sync task started");
        }
    }

    void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
        // ✅ Safely copy the payload into a null-terminated buffer
        char safePayload[len + 1];
        memcpy(safePayload, payload, len);
        safePayload[len] = '\0';
    
        String message = String(safePayload);
        Serial.printf("📨 MQTT Message on [%s]: %s\n", topic, message.c_str());
    
        // ✅ Use dynamic mqttPrefix instead of hardcoded "stc/"
        String topicStr = String(topic);
        String expectedPrefix = String(mqttPrefix) + "/" + serialNumber + "/";
    
        if (!topicStr.startsWith(expectedPrefix)) {
            //Serial.println("⚠️ Received message for a different serial number. Ignoring.");
            return;
        }
    
        String subTopic = topicStr.substring(expectedPrefix.length());
        
        // Dispatch to appropriate handler
        if (subTopic == "config") {
            handleConfigMessage(message);
        } else if (subTopic == "errors") {
            handleErrorMessage(message);
        } else if (subTopic == "module") {
            handleModuleMessage(message);
        } else if (subTopic == "reset") {
            handleResetMessage(message);
        } else if (subTopic == "upgrade") {
            handleUpgradeMessage(message);
        } else if (subTopic == "update") {
            handleUpdateMessage(message);
        } else {
            Serial.printf("❓ Unknown subtopic: %s\n", subTopic.c_str());
        }
    }

    void handleConfigMessage(const String& message) {
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, message);
    
        if (error) {
            Serial.println("❌ Failed to parse config JSON");
            return;
        }
    
        String type = doc["type"] | "";
    
        if (type == "getConfig") {
            StaticJsonDocument<1024> response;
            response["serialNumber"] = serialNumber;
            response["moduleName"] = moduleName;
            response["webName"] = webname;
            response["location"] = location;
            response["mac"] = WiFi.macAddress();
            response["ip"] = WiFi.localIP().toString();
            response["customMqtt"] = customMqtt;
            response["customMqttServer"] = custom_mqtt_server;
            response["customMqttPort"] = custom_mqtt_port;
            response["smartBoatEnabled"] = smartBoatEnabled;
            response["firmwareVersion"] = FW_VER;
    
            String payload;
            serializeJson(response, payload);
            mqttClient.publish((String(mqttPrefix) + "/config/update").c_str(), 1, true, payload.c_str());
    
            Serial.println("✅ Published generic module config");
        }
    
        else if (type == "setConfig") {
            bool settingsChanged = false;
    
            if (doc.containsKey("moduleName")) {
                String name = doc["moduleName"].as<String>();
                if (name.length() <= EEPROM_STR_LEN) {
                    moduleName = name;
                    SmartCore_EEPROM::writeModuleNameToEEPROM(moduleName);
                    settingsChanged = true;
                }
            }
    
            if (doc.containsKey("webName")) {
                const char* name = doc["webName"];
                if (name && strlen(name) < sizeof(webnamechar)) {
                    strncpy(webnamechar, name, sizeof(webnamechar) - 1);
                    webnamechar[sizeof(webnamechar) - 1] = '\0';
                    webname = String(webnamechar);
                    SmartCore_EEPROM::writeStringToEEPROM(WEBNAME_ADDR, webname);
                    settingsChanged = true;
                }
            }
            
            if (doc.containsKey("location")) {
                String loc = doc["location"].as<String>();
                if (loc.length() <= EEPROM_STR_LEN) {
                    location = loc;
                    SmartCore_EEPROM::writeLocationToEEPROM(location);
                    settingsChanged = true;
                }
            }
    
            if (doc.containsKey("customMqtt")) {
                customMqtt = doc["customMqtt"];
                SmartCore_EEPROM::writeBoolToEEPROM(CUSTOM_MQTT_ADDR, customMqtt);
                settingsChanged = true;
            }
    
            if (doc.containsKey("customMqttServer")) {
                strlcpy(custom_mqtt_server, doc["customMqttServer"], sizeof(custom_mqtt_server));
                SmartCore_EEPROM::writeStringToEEPROM(CUSTOM_MQTT_SERVER_ADDR, custom_mqtt_server);
                settingsChanged = true;
            }
    
            if (doc.containsKey("customMqttPort")) {
                custom_mqtt_port = doc["customMqttPort"];
                SmartCore_EEPROM::writeIntToEEPROM(CUSTOM_MQTT_PORT_ADDR, custom_mqtt_port);
                settingsChanged = true;
            }
    
            if (doc.containsKey("smartBoatEnabled")) {
                smartBoatEnabled = doc["smartBoatEnabled"];
                SmartCore_EEPROM::writeBoolToEEPROM(SB_BOOL_ADDR, smartBoatEnabled);
                settingsChanged = true;
            }
    
            if (settingsChanged) {
                Serial.println("💾 Generic config updated and saved.");
            }
    
            // Send back updated config
            handleConfigMessage("{\"type\":\"getConfig\"}");
        }
    
        else {
            Serial.printf("⚠️ Unknown config message type: '%s'\n", type.c_str());
        }
    }
    

    void handleModuleMessage(const String& message){
        Serial.println("message arrived for module");
        handleModuleSpecificModule();
    }

    void handleErrorMessage(const String& message){
        Serial.println("message arrived for errors");
        handleModuleSpecificErrors();
    }

    void handleResetMessage(const String& message) {
        Serial.println("🔄 Reset message received");
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, message);

        if (error) {
            Serial.print("❌ Failed to parse reset JSON: ");
            Serial.println(error.c_str());
            return;
        }

        String action = doc["action"] | "";

        if (action == "reset") {
            bool confirm = doc["confirm"] | false;
            if (confirm) {
                xTaskCreatePinnedToCore(SmartCore_System::resetWorkerTask, "ResetWorker", 4096, NULL, 1, NULL, 0);
            } else {
                Serial.println("⚠️ Reset requested but not confirmed. Ignoring.");
            }
        } else {
            Serial.println("⚠️ Unsupported action in reset message.");
        }
       
    }
    
    void handleUpgradeMessage(const String& message) {
        Serial.println("⬆️ Upgrade message received");
    
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, message);
    
        if (error) {
            Serial.print("❌ Failed to parse update JSON: ");
            Serial.println(error.c_str());
            return;
        }
    
        String action = doc["action"] | "";
        bool confirm = doc["confirm"] | false;
    
        if (action == "upgrade") {
            if (confirm) {
                Serial.println("✅ OTA confirmed. Spawning OTA task...");
                SmartCore_OTA::shouldUpdateFirmware = true;
                if (SmartCore_OTA::otaTaskHandle == nullptr) {
                    xTaskCreatePinnedToCore(SmartCore_OTA::otaTask, "OTATask", 8192, NULL, 1, &SmartCore_OTA::otaTaskHandle, 1);
                }
            } else {
                Serial.println("⚠️ Upgrade requested but not confirmed. Ignoring.");
            }
        } 
        else if (action == "checkUpdateAvailable") {
            Serial.println("🔍 Checking for OTA update availability...");
    
            updateInfo inf = OTADRIVE.updateFirmwareInfo();
    
            Serial.print("ℹ️ Upgrade available: ");
            Serial.println(inf.available ? "Yes" : "No");
            Serial.print("📦 OTA version: ");
            Serial.println(inf.version.c_str());
    
            DynamicJsonDocument response(512);
            response["type"] = "upgradeStatus";
            response["serial"] = serialNumber;
            response["available"] = inf.available;
            response["currentVersion"] = FW_VER;
            response["latestVersion"] = inf.available ? inf.version.c_str() : "N/A";
    
            if (!inf.available) {
                response["message"] = "System is up to date.";
            } else {
                SmartCore_OTA::isUpgradeAvailable = true;
                SmartCore_EEPROM::saveUpgradeFlag(true);
                response["message"] = "Upgrade available.";
            }
    
            String responseJson;
            serializeJson(response, responseJson);
    
            if (mqttIsConnected) {
                mqttClient.publish((String(mqttPrefix) + "/ota/status").c_str(), 0, false, responseJson.c_str());
            }
    
            #if WEBSERVER_ENABLED
                ws.textAll(responseJson);
            #endif
    
            Serial.println("✅ OTA check response published.");
        } 
        else {
            Serial.println("⚠️ Unsupported action in update message.");
        }
    }

    void handleUpdateMessage(const String& message) {
        Serial.println("🛠️ Update message received");
    
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, message);
    
        if (error) {
            Serial.print("❌ Failed to parse update JSON: ");
            Serial.println(error.c_str());
            return;
        }
    
        // ✅ Check for SmartBoat time sync
        if (doc.containsKey("update") &&
            doc["update"] == "time" &&
            doc.containsKey("epoch") &&
            awaitingSmartboatTimeSync) {
    
            smartBoatEpoch = doc["epoch"];  // assumed to be unsigned long / uint32_t
            smartBoatEpochSyncMillis = millis();
            awaitingSmartboatTimeSync = false;
    
            Serial.printf("🕒 Received SmartBoat time: %lu (syncMillis: %lu)\n",
                          smartBoatEpoch, smartBoatEpochSyncMillis);
        }
        else {
            Serial.println("⚠️ Unrecognized update message or already synced.");
        }
    }
    
      

    void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
        mqttIsConnected = false;  // Update connection state
        logMessage(LOG_WARN, "❌ Disconnected from MQTT (" + String((int)reason) + ")");
        currentLEDMode = LEDMODE_DEBUG_PATTERN;

        if (metricsTaskHandle) {
            vTaskDelete(metricsTaskHandle);
            metricsTaskHandle = nullptr;
            Serial.println("🛑 Metrics task stopped");
        }

        if (timeSyncTaskHandle) {
            vTaskDelete(timeSyncTaskHandle);
            timeSyncTaskHandle = nullptr;
            Serial.println("🛑 Time sync task stopped");
        }
    
        // Determine the flash pattern based on the disconnection reason
        switch (reason) {
            case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT:
                Serial.println("Bad fingerprint");
                flashPattern = ".-";  // 1 short, 1 long flash
                break;
            case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:
                Serial.println("TCP disconnected");
                flashPattern = "..";  // 2 short flashes
                break;
            case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
                Serial.println("Unacceptable protocol version");
                flashPattern = "...";  // 3 short flashes
                break;
            case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED:
                Serial.println("Identifier rejected");
                flashPattern = "--";  // 2 long flashes
                break;
            case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE:
                Serial.println("Server unavailable");
                flashPattern = "-";  // 1 long flash
                break;
            case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:
                Serial.println("Malformed credentials");
                flashPattern = "-.";  // 1 long, 1 short flash
                break;
            case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:
                Serial.println("Not authorized");
                flashPattern = "-..";  // 1 long, 2 short flashes
                break;
            case AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE:
                Serial.println("ESP8266 not enough space");
                flashPattern = "-.-";  // 1 long, 1 short, 1 long flash
                break;
            default:
                Serial.println("Unknown reason");
                flashPattern = "--.";  // 2 long, 1 short flash for unknown error
                break;
        }
    
        // Signal that a new pattern is available
        triggerFlashPattern(flashPattern, DEBUG_COLOR_YELLOW);
        if (mqttReconnectTaskHandle == NULL) {
            xTaskCreatePinnedToCore(mqttReconnectTask, "MQTT Reconnect", 4096, NULL, 1, &mqttReconnectTaskHandle, 0);
        }
    }

    void scheduleReconnect() {
        static unsigned long lastAttempt = 0;
        static unsigned long retryInterval = 5000;
    
        unsigned long now = millis();
       
        
    }
    
    void mqttReconnectTask(void *param) {
        for (;;) {
            if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
                SmartCore_MQTT::scheduleReconnect();  // Includes exponential backoff
            }
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }

    void metricsTask(void *parameter) {
        logMessage(LOG_INFO, "📊 Metrics task started.");

        // Simulate a crash during startup
        //int* ptr = nullptr;
        //*ptr = 42;  // 💥 Boom

        // Temperature sensor config
        temp_sensor_config_t temp_sensor = {
            .dac_offset = TSENS_DAC_L2,
            .clk_div = 6,
        };
        temp_sensor_set_config(temp_sensor);
        temp_sensor_start();

        char macStr[18];
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        for (;;) {
            if (!mqttIsConnected) {
                logMessage(LOG_INFO, "🛑 Metrics task exiting — MQTT disconnected.");
                vTaskDelete(nullptr);
            }

            StaticJsonDocument<384> doc;

            doc["serialNumber"] = serialNumber;
            JsonObject metrics = doc.createNestedObject("metrics");

            metrics["mac"] = macStr;
            metrics["ip"] = WiFi.localIP().toString();
            metrics["uptime"] = millis() / 1000;

            float tempC;
            if (temp_sensor_read_celsius(&tempC) == ESP_OK) {
                metrics["tempC"] = tempC;
            } else {
                metrics["tempC"] = nullptr;
            }

            metrics["heap"] = ESP.getFreeHeap();
            metrics["rssi"] = WiFi.RSSI();

            char buffer[512];
            size_t len = serializeJson(doc, buffer);

            mqttClient.publish((String(mqttPrefix) + "/metrics").c_str(), 0, false, buffer, len);
            logMessage(LOG_INFO, "📤 Metrics sent → " + String(buffer));

            vTaskDelay(10000 / portTICK_PERIOD_MS);  // 10s loop
        }
    }

    void timeSyncTask(void* parameter) {
        const TickType_t delay = pdMS_TO_TICKS(3600000);  // 1 hour = 3600000 ms
    
        for (;;) {
            if (mqttIsConnected) {
                Serial.println("🕓 Hourly SmartBoat time sync...");
                requestSmartBoatTime();
            }
            vTaskDelay(delay);
        }
    }

    void requestSmartBoatTime() {
        StaticJsonDocument<128> doc;
        doc["serialNumber"] = serialNumber;
        doc["action"] = "gettime";
    
        char payload[128];
        serializeJson(doc, payload, sizeof(payload));
    
        mqttClient.publish("module/update", 1, false, payload);
        awaitingSmartboatTimeSync = true;
    
        Serial.println("📡 Time sync requested via module/update");
    }
    
}
