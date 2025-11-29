#include "SmartCore_MQTT.h"
#include <arduino.h>
#include <AsyncMqttClient.h>
#include <HTTPClient.h>
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

namespace SmartCore_MQTT
{
    String pendingBrokerIP = "";
    uint16_t pendingBrokerPort = 0;
    bool mqttFailoverAckReceived;
    bool failoverInProgress = false;
    int currentPriorityIndex = 0;
    String currentBrokerIP = "";
    uint16_t currentBrokerPort = 1883;
    char mqttWillTopic[64];
    TaskHandle_t metricsTaskHandle = NULL;
    TaskHandle_t timeSyncTaskHandle = NULL;
    void onMqttConnect(bool sessionPresent);
    void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);

    static std::function<void(bool)> mqttConnectCallback = SmartCore_MQTT::onMqttConnect;
    static std::function<void(AsyncMqttClientDisconnectReason)> mqttDisconnectCallback = SmartCore_MQTT::onMqttDisconnect;

    void generateMqttPrefix()
    {
        // Set mqttPrefix based on the first 4 chars of serialNumber
        strncpy(mqttPrefix, serialNumber, 4);
        mqttPrefix[4] = '\0'; // Ensure null-termination

        // Build the mqttWillTopic from the prefix
        snprintf(mqttWillTopic, sizeof(mqttWillTopic), "%s/disconnected", mqttPrefix);
        mqttWillTopic[sizeof(mqttWillTopic) - 1] = '\0'; // Safety null-termination

        // Debugging
        logMessage(LOG_INFO, "🧠 MQTT prefix: " + String(mqttPrefix));
        logMessage(LOG_INFO, "🧠 MQTT will topic: " + String(mqttWillTopic));
    }

    void setupMQTTClient(const String &ip, uint16_t port)
    {
        logMessage(LOG_INFO, "🔧 Configuring MQTT client for " + ip + ":" + String(port));

        generateMqttPrefix(); // sets mqttWillTopic

        // Clean previous instance
        if (mqttClient)
            delete mqttClient;

        mqttClient = new AsyncMqttClient();

        // --- Callbacks ---
        mqttClient->onConnect(onMqttConnect);
        mqttClient->onDisconnect(onMqttDisconnect);
        mqttClient->onMessage(onMqttMessage);

        mqttClient->setKeepAlive(15);

        // Client ID = MAC without colons
        String clientId = WiFi.macAddress();
        clientId.replace(":", "");
        mqttClient->setClientId(clientId.c_str());

        // Will
        mqttClient->setWill(
            mqttWillTopic,
            1,
            true,
            serialNumber,
            strlen(serialNumber));

        // Store new target
        currentBrokerIP = ip;
        currentBrokerPort = port;

        // Set server
        mqttClient->setServer(currentBrokerIP.c_str(), currentBrokerPort);

        // WiFi check
        if (WiFi.status() != WL_CONNECTED)
        {
            logMessage(LOG_WARN, "📶 WiFi not connected → skipping MQTT connect.");
            return;
        }

        // Attempt connection ONCE
        mqttClient->connect();

        // Optional: minimal blocking wait
        unsigned long start = millis();
        while (!mqttClient->connected() && millis() - start < 3000)
            delay(50);

        if (mqttClient->connected())
            logMessage(
                LOG_INFO,
                String("✅ MQTT connected to ") +
                    currentBrokerIP +
                    ":" +
                    String(currentBrokerPort));
        else
            logMessage(LOG_WARN, String("❌ MQTT connect FAILED to ") + currentBrokerIP + ":" + String(currentBrokerPort));
    }

    // ======================================================================================
    //  MQTT FAILOVER ENGINE — SmartModules + SmartBox Cluster Logic
    // ======================================================================================
    //
    //  This function is the *heart* of SmartBoat’s MQTT resilience system.
    //  It is called ONLY after repeated MQTT connection failures inside the
    //  wifiMqttCheckTask(). (mqttFailCount > 10)
    //
    // --------------------------------------------------------------------------------------
    //  PURPOSE
    // --------------------------------------------------------------------------------------
    //
    //   • Rotate to the next MQTT broker in the priority list.
    //   • Handle normal SmartModule failover (simple + fast).
    //   • Handle SmartBox-aware failover (cluster logic).
    //   • Notify the Raspberry Pi (SmartBox core) over UART:
    //        → Become Primary     (enable Mosquitto + switch Node-RED to localhost)
    //        → Switch Broker      (tell Pi to use another SmartBox broker IP)
    //
    //   • Apply new broker IP/port only after ACK from Pi (cluster coherence).
    //
    // --------------------------------------------------------------------------------------
    //  HOW FAILURE ROTATION WORKS
    // --------------------------------------------------------------------------------------
    //
    //   1) Increment currentPriorityIndex with wrap-around.
    //      Example list: [Primary, Backup1, Backup2, Backup3]
    //
    //   2) nextIP = mqttPriorityList[currentPriorityIndex]
    //
    //   3) Determine module type:
    //        • SmartModules (REL, SEN, STC, etc.)   → SIMPLE FAILOVER
    //        • SmartBox units (SBN, SBC, SBP)       → SMART FAILOVER
    //
    // --------------------------------------------------------------------------------------
    //  NORMAL SMARTMODULE FAILOVER (Non-SmartBox)
    // --------------------------------------------------------------------------------------
    //
    //   For normal modules:
    //       pendingBrokerIP = nextIP
    //       commitPendingBroker()
    //       return
    //
    //   No ACK required, no UART messaging.
    //   Very fast and completely self-contained.
    //
    // --------------------------------------------------------------------------------------
    //  SMARTBOX-AWARE FAILOVER
    // --------------------------------------------------------------------------------------
    //
    //   SmartBox modules are *cluster participants*:
    //
    //   Two scenarios:
    //
    //   A) nextIP == our own IP
    //      → We are next in the failover hierarchy.
    //      → Notify Pi: "Become Primary"
    //      → Wait for ACK (up to 10s)
    //      → After ACK, apply broker and continue
    //
    //   B) nextIP != ownIP
    //      → Another SmartBox is next in priority.
    //      → Notify Pi: "Switch Node-RED MQTT broker to nextIP"
    //      → Wait for ACK
    //      → After ACK, apply broker and continue
    //
    // --------------------------------------------------------------------------------------
    //  FAILOVER SAFETY DESIGN
    // --------------------------------------------------------------------------------------
    //
    //   • failoverInProgress freezes ONLY the MQTT reconnection logic.
    //     WiFi backoff continues normally.
    //     LED system continues normally.
    //
    //   • Only one failover attempt runs at a time.
    //   • ACK ensures the cluster stays synchronized.
    //   • No blind reconnects.
    //
    // --------------------------------------------------------------------------------------
    //  SUMMARY
    // --------------------------------------------------------------------------------------
    //
    //   - SmartModules rotate brokers immediately.
    //   - SmartBox units coordinate with the Pi for consistent cluster switching.
    //   - failoverInProgress ensures no reconnection fight occurs during failover.
    //   - Once complete, MQTT reconnects using the new broker automatically.
    //
    // ======================================================================================

    void handleMQTTFailover()
    {
        logMessage(LOG_WARN, "🚨 MQTT FAILOVER triggered.");

        // Freeze ONLY MQTT reconnect logic — NOT LED, NOT WiFi
        failoverInProgress = true;

        // ---------------------------------------------------------
        // Get own IP
        // ---------------------------------------------------------
        String ownIP = WiFi.localIP().toString();

        // ---------------------------------------------------------
        // Determine next broker IP
        // ---------------------------------------------------------
        if (mqttPriorityCount == 0)
        {
            logMessage(LOG_ERROR, "❌ No MQTT priority list configured.");
            failoverInProgress = false;
            return;
        }

        // Move index → next item (wrap-around)
        currentPriorityIndex++;
        if (currentPriorityIndex >= mqttPriorityCount)
            currentPriorityIndex = 0;

        String nextIP = mqttPriorityList[currentPriorityIndex];
        uint16_t nextPort = mqtt_port;

        logMessage(LOG_INFO,
                   "🔄 Next priority broker → " + nextIP + ":" + String(nextPort));

        // ---------------------------------------------------------
        // Determine if this is a SmartBox module
        // ---------------------------------------------------------
        String sn = String(serialNumber);
        sn.toUpperCase();

        bool isSmartBox =
            sn.startsWith("SBN") ||
            sn.startsWith("SBC") ||
            sn.startsWith("SBP");

        // ---------------------------------------------------------
        // CASE 1 — Non-SmartBox modules
        // ---------------------------------------------------------
        if (!isSmartBox)
        {
            logMessage(LOG_INFO, "📦 Normal SmartModule failover.");

            pendingBrokerIP = nextIP;
            pendingBrokerPort = nextPort;

            commitPendingBroker();

            failoverInProgress = false;
            return;
        }

        // ---------------------------------------------------------
        // CASE 2 — SmartBox-aware failover
        // ---------------------------------------------------------
        logMessage(LOG_INFO, "🧠 SmartBox-aware failover logic engaged.");

        mqttFailoverAckReceived = false;

        // =========================================================
        // A. Next broker IP == our own IP → we must become primary
        // =========================================================
        if (nextIP == ownIP)
        {
            logMessage(LOG_WARN,
                       "🏆 This SmartBox is next in line → requesting PRIMARY ROLE");

            SmartBox_notifyBecomePrimary(); // UART → Pi

            pendingBrokerIP = ownIP;
            pendingBrokerPort = nextPort;

            // Wait for ACK (max 10 sec)
            uint32_t start = millis();
            while (!mqttFailoverAckReceived && millis() - start < 10000)
                vTaskDelay(pdMS_TO_TICKS(50));

            if (!mqttFailoverAckReceived)
            {
                logMessage(LOG_ERROR,
                           "❌ No ACK from Pi (becomePrimary). Failover incomplete.");
            }
            else
            {
                logMessage(LOG_INFO,
                           "✅ ACK from Pi. SmartBox promoted to PRIMARY.");
                commitPendingBroker();
            }

            failoverInProgress = false;
            return;
        }

        // =========================================================
        // B. Next broker IP is another SmartBox → request switch
        // =========================================================
        logMessage(LOG_INFO,
                   "➡️ Requesting Pi to switch NodeRED MQTT → " + nextIP);

        SmartBox_notifySetBroker(nextIP); // UART → Pi

        pendingBrokerIP = nextIP;
        pendingBrokerPort = nextPort;

        // Wait for ACK (max 10 sec)
        uint32_t start = millis();
        while (!mqttFailoverAckReceived && millis() - start < 10000)
            vTaskDelay(pdMS_TO_TICKS(50));

        if (!mqttFailoverAckReceived)
        {
            logMessage(LOG_ERROR,
                       "❌ No ACK from Pi (setBroker). Failover incomplete.");
        }
        else
        {
            logMessage(LOG_INFO,
                       "✅ ACK from Pi. Switching MQTT → " + pendingBrokerIP);
            commitPendingBroker();
        }

        failoverInProgress = false;
    }

    // ======================================================================================
    //  COMMIT NEW MQTT BROKER (Used by failover + provisioning updates)
    // ======================================================================================
    //
    //  PURPOSE:
    //      Apply the new broker IP + port selected by the failover engine, and
    //      clear the pending values.
    //
    //  WHEN THIS IS CALLED:
    //      • After SmartModule failover (immediate).
    //      • After SmartBox cluster failover (only after ACK from Pi).
    //      • After SmartApp provisioning (manual reassignment).
    //
    //  WHAT IT DOES:
    //
    //      currentBrokerIP   = pendingBrokerIP
    //      currentBrokerPort = pendingBrokerPort
    //
    //      Log the new broker.
    //      Clear pending fields.
    //
    //  IMPORTANT NOTES:
    //
    //   • This function *does NOT* connect to MQTT.
    //       The wifiMqttCheckTask() will automatically attempt a reconnect on the next cycle.
    //
    //   • This function does NOT persist the priority index.
    //       That is done only when connection SUCCESSFULLY happens:
    //             → inside onMqttConnect()
    //
    //   • pendingBrokerIP allows safe staging:
    //       We never modify the active broker until the Pi confirms the failover.
    //
    //   • commitPendingBroker() always makes the new broker active instantly.
    //       It is intentionally simple and deterministic.
    //
    // ======================================================================================

    void commitPendingBroker()
    {
        if (pendingBrokerIP.length() == 0)
            return;

        currentBrokerIP = pendingBrokerIP;
        currentBrokerPort = pendingBrokerPort;

        logMessage(LOG_INFO,
                   "📡 Committed new MQTT broker → " +
                       currentBrokerIP + ":" + String(currentBrokerPort));

        pendingBrokerIP = "";
        pendingBrokerPort = 0;
    }

    void onMqttConnect(bool sessionPresent)
    {
        Serial.println("Connected to MQTT broker.");
        mqttIsConnected = true;

        // Subscribe to module-specific topic: serialNumber/#
        String serialTopic = String(serialNumber) + "/#";
        mqttClient->subscribe(serialTopic.c_str(), 1);
        Serial.print("✅ Subscribed to ");
        Serial.println(serialTopic);

        // Subscribe to global update topic
        mqttClient->subscribe("update/#", 1);
        Serial.println("✅ Subscribed to update/#");

        // Publish initial presence messages
        mqttClient->publish((String(mqttPrefix) + "/connected").c_str(), 1, true, "connected");
        requestSmartBoatTime();

        logMessage(LOG_INFO, "🔍 firstWifiCOnnect: " + String(firstWiFiConnect ? "true" : "false"));
        if (firstWiFiConnect)
        {
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

            if (serialNumberAssigned)
            {
                doc["serialNumber"] = serialNumber;
            }
            else
            {
                doc["serialNumber"] = "unassigned";
            }

            String payload;
            serializeJson(doc, payload);
            logMessage(LOG_INFO, "📦 Payload to publish: " + payload);

            if (mqttClient->connected())
            {
                mqttClient->publish(topic, 1, true, payload.c_str());
                logMessage(LOG_INFO, "✅ MQTT publish call made.");
            }
            else
            {
                logMessage(LOG_WARN, "❌ MQTT client not connected — skipping publish.");
            }

            firstWiFiConnect = false;
            SmartCore_EEPROM::writeFirstWiFiConnectFlag(false);
            logMessage(LOG_INFO, "📝 firstWiFiConnect flag set to false and saved.");
        }
        else
        {
            logMessage(LOG_INFO, "⏭️ Skipping newModule publish — firstWiFiConnect is false.");
        }

        // 🌡️ Start metrics task
        if (!metricsTaskHandle)
        {
            xTaskCreate(metricsTask, "metricsTask", 4096, nullptr, 1, &metricsTaskHandle);
        }

        // start time get task
        if (!timeSyncTaskHandle)
        {
            xTaskCreatePinnedToCore(timeSyncTask, "timeSyncTask", 4096, nullptr, 1, &timeSyncTaskHandle, 0);
            Serial.println("🕓 Time sync task started");
        }

        // -------------------------------------------------------------
        // Persist current priority index AFTER SUCCESSFUL MQTT CONNECT
        // -------------------------------------------------------------
        if (SmartCore_MQTT::failoverInProgress)
        {
            // Check correctness: ensure we're really connected to a priority IP
            String connectedIP = SmartCore_MQTT::currentBrokerIP;
            uint16_t connectedPort = SmartCore_MQTT::currentBrokerPort;

            bool match = false;
            if (SmartCore_MQTT::currentPriorityIndex < mqttPriorityCount)
            {
                String expectedIP = mqttPriorityList[SmartCore_MQTT::currentPriorityIndex];
                match = (connectedIP == expectedIP);
            }

            if (match)
            {
                logMessage(LOG_INFO,
                           "💾 Persisting MQTT priority index → " +
                               String(SmartCore_MQTT::currentPriorityIndex));

                SmartCore_EEPROM::writeByteToEEPROM(
                    MQTT_LAST_PRIORITY_ADDR,
                    SmartCore_MQTT::currentPriorityIndex);
                EEPROM.commit();
            }
            else
            {
                logMessage(LOG_WARN,
                           "⚠️ Priority mismatch — NOT saving priority index.");
            }

            SmartCore_MQTT::failoverInProgress = false;
        }
    }

    void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties,
                       size_t len, size_t index, size_t total)
    {
        // ✅ Safely copy payload into null-terminated buffer
        char safePayload[len + 1];
        memcpy(safePayload, payload, len);
        safePayload[len] = '\0';

        String message = String(safePayload);
        Serial.printf("📨 MQTT Message on [%s]: %s\n", topic, message.c_str());

        String topicStr = String(topic);

        // 🔍 Define all expected topics
        String expectedConfigTopic = String(serialNumber) + "/config";
        String expectedErrorsTopic = String(serialNumber) + "/errors";
        String expectedModuleTopic = String(serialNumber) + "/module";
        String expectedResetTopic = String(serialNumber) + "/reset";
        String expectedUpgradeTopic = String(serialNumber) + "/upgrade";
        String expectedUpdateTopic = String(serialNumber) + "/update";

        // 🧭 Route to appropriate handlers
        if (topicStr == expectedConfigTopic)
            handleConfigMessage(message);
        else if (topicStr == expectedErrorsTopic)
            handleErrorMessage(message);
        else if (topicStr == expectedModuleTopic)
            handleModuleMessage(message);
        else if (topicStr == expectedResetTopic)
            handleResetMessage(message);
        else if (topicStr == expectedUpgradeTopic)
            handleUpgradeMessage(message);
        else if (topicStr == expectedUpdateTopic)
            handleUpdateMessage(message);
        else
            Serial.printf("❓ Unknown subtopic on [%s]\n", topicStr.c_str());
    }

    void handleConfigMessage(const String &message)
    {
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, message);

        if (error)
        {
            Serial.printf("❌ Failed to parse config JSON: %s\n", error.c_str());
            return;
        }

        String type = doc["type"] | "";
        if (type.isEmpty())
        {
            Serial.println("⚠️ Config message missing 'type' field.");
            return;
        }

        if (type == "getConfig")
        {
            StaticJsonDocument<1024> response;
            response["serialNumber"] = serialNumber;
            response["moduleName"] = moduleName;
            response["location"] = location;
            response["mac"] = WiFi.macAddress();
            response["ip"] = WiFi.localIP().toString();
            response["firmwareVersion"] = FW_VER;
            response["updateAvailable"] = SmartCore_EEPROM::loadUpgradeFlag();

            String payload;
            serializeJson(response, payload);
            mqttClient->publish("module/config/update", 1, true, payload.c_str());
            Serial.println("✅ Published generic module config");

            // ✅ Forward object to module-specific handler too
            handleModuleSpecificConfig(doc.as<JsonObject>());
        }
        else if (type == "setConfig")
        {
            bool settingsChanged = false;

            if (doc.containsKey("moduleName"))
            {
                String name = doc["moduleName"].as<String>();
                if (name.length() <= EEPROM_STR_LEN)
                {
                    moduleName = name;
                    SmartCore_EEPROM::writeModuleNameToEEPROM(moduleName);
                    settingsChanged = true;
                }
            }

            if (doc.containsKey("location"))
            {
                String loc = doc["location"].as<String>();
                if (loc.length() <= EEPROM_STR_LEN)
                {
                    location = loc;
                    SmartCore_EEPROM::writeLocationToEEPROM(location);
                    settingsChanged = true;
                }
            }

            if (settingsChanged)
                Serial.println("💾 Generic config updated and saved.");

            // ✅ Re-publish the updated config
            handleConfigMessage("{\"type\":\"getConfig\"}");
        }
        else if (type == "setModuleConfig")
        {
            handleModuleSpecificConfig(doc.as<JsonObject>());
        }
        else
        {
            Serial.printf("⚠️ Unknown config message type: '%s'\n", type.c_str());
        }
    }

    void handleModuleMessage(const String &message)
    {
        Serial.println("message arrived for module");
        handleModuleSpecificModule(message);
    }

    void handleErrorMessage(const String &message)
    {
        Serial.println("message arrived for errors");
        handleModuleSpecificErrors(message);
    }

    void handleResetMessage(const String &message)
    {
        Serial.println("🔄 Reset message received");
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, message);

        if (error)
        {
            Serial.print("❌ Failed to parse reset JSON: ");
            Serial.println(error.c_str());
            return;
        }

        String action = doc["action"] | "";

        if (action == "reset")
        {
            bool confirm = doc["confirm"] | false;
            if (confirm)
            {
                xTaskCreatePinnedToCore(SmartCore_System::resetWorkerTask, "ResetWorker", 4096, NULL, 1, NULL, 0);
            }
            else
            {
                Serial.println("⚠️ Reset requested but not confirmed. Ignoring.");
            }
        }
        else
        {
            Serial.println("⚠️ Unsupported action in reset message.");
        }
    }

    void handleUpgradeMessage(const String &message)
    {
        Serial.println("⬆️ OTA upgrade message received");

        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, message);

        if (error)
        {
            Serial.print("❌ Failed to parse upgrade JSON: ");
            Serial.println(error.c_str());
            return;
        }

        String action = doc["action"] | "";
        bool confirm = doc["confirm"] | false;

        if (action == "upgrade")
        {
            if (confirm)
            {
                Serial.println("✅ OTA confirmed. Spawning OTA task...");
                SmartCore_OTA::shouldUpdateFirmware = true;
                if (SmartCore_OTA::otaTaskHandle == nullptr)
                {
                    xTaskCreatePinnedToCore(SmartCore_OTA::otaTask, "OTATask", 8192, NULL, 1, &SmartCore_OTA::otaTaskHandle, 1);
                }
            }
            else
            {
                Serial.println("⚠️ Upgrade requested but not confirmed. Ignoring.");
            }
        }
        else if (action == "checkUpdateAvailable")
        {
            Serial.println("🔍 Checking for OTA update availability...");
            updateInfo inf = OTADRIVE.updateFirmwareInfo();

            SmartCore_EEPROM::saveUpgradeFlag(inf.available);

            DynamicJsonDocument response(512);
            response["type"] = "upgradeStatus";
            response["serialNumber"] = serialNumber;
            response["available"] = inf.available;
            response["currentVersion"] = FW_VER;
            response["latestVersion"] = inf.available ? inf.version.c_str() : "N/A";
            response["message"] = inf.available ? "Upgrade available." : "System is up to date.";

            String responseJson;
            serializeJson(response, responseJson);

            if (mqttIsConnected && mqttClient)
            {
                mqttClient->publish("module/upgrade", 0, false, responseJson.c_str());
            }
            Serial.println("✅ OTA check response published.");
        }
        else
        {
            Serial.println("⚠️ Unsupported action in upgrade message.");
        }
    }

    void handleUpdateMessage(const String &message)
    {
        Serial.println("🛠️ Update message received");

        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, message);

        if (error)
        {
            Serial.print("❌ Failed to parse update JSON: ");
            Serial.println(error.c_str());
            return;
        }

        // ✅ Check for SmartBoat time sync
        if (doc.containsKey("update") &&
            doc["update"] == "time" &&
            doc.containsKey("epoch") &&
            awaitingSmartboatTimeSync)
        {

            smartBoatEpoch = doc["epoch"]; // assumed to be unsigned long / uint32_t
            smartBoatEpochSyncMillis = millis();
            awaitingSmartboatTimeSync = false;

            Serial.printf("🕒 Received SmartBoat time: %lu (syncMillis: %lu)\n",
                          smartBoatEpoch, smartBoatEpochSyncMillis);
        }
        else
        {
            Serial.println("⚠️ Unrecognized update message or already synced.");
        }
    }

    void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
    {
        mqttIsConnected = false; // Update connection state
        logMessage(LOG_WARN, "❌ Disconnected from MQTT (" + String((int)reason) + ", " + mqttDisconnectReasonToStr(reason) + ")");
        SmartCore_LED::currentLEDMode = LEDMODE_STATUS;

        if (metricsTaskHandle)
        {
            vTaskDelete(metricsTaskHandle);
            metricsTaskHandle = nullptr;
            Serial.println("🛑 Metrics task stopped");
        }

        if (timeSyncTaskHandle)
        {
            vTaskDelete(timeSyncTaskHandle);
            timeSyncTaskHandle = nullptr;
            Serial.println("🛑 Time sync task stopped");
        }
    }

    const char *mqttDisconnectReasonToStr(AsyncMqttClientDisconnectReason reason)
    {
        switch (reason)
        {
        case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:
            return "TCP_DISCONNECTED";
        case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
            return "UNACCEPTABLE_PROTOCOL_VERSION";
        case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED:
            return "IDENTIFIER_REJECTED";
        case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE:
            return "SERVER_UNAVAILABLE";
        case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:
            return "MALFORMED_CREDENTIALS";
        case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:
            return "NOT_AUTHORIZED";
        default:
            return "UNKNOWN";
        }
    }

    void metricsTask(void *parameter)
    {
        logMessage(LOG_INFO, "📊 Metrics task started.");

        // Simulate a crash during startup
        // int* ptr = nullptr;
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

        for (;;)
        {
            if (!mqttIsConnected)
            {
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
            if (temp_sensor_read_celsius(&tempC) == ESP_OK)
            {
                metrics["tempC"] = tempC;
            }
            else
            {
                metrics["tempC"] = nullptr;
            }

            metrics["heap"] = ESP.getFreeHeap();
            metrics["rssi"] = WiFi.RSSI();

            char buffer[512];
            size_t len = serializeJson(doc, buffer);

            mqttClient->publish("module/metrics", 0, false, buffer, len);
            logMessage(LOG_INFO, "📤 Metrics sent → " + String(buffer));

            vTaskDelay(10000 / portTICK_PERIOD_MS); // 10s loop
        }
    }

    void timeSyncTask(void *parameter)
    {
        const TickType_t delay = pdMS_TO_TICKS(3600000); // 1 hour = 3600000 ms

        for (;;)
        {
            if (mqttIsConnected)
            {
                Serial.println("🕓 Hourly SmartBoat time sync...");
                requestSmartBoatTime();
            }
            vTaskDelay(delay);
        }
    }

    void requestSmartBoatTime()
    {
        StaticJsonDocument<128> doc;
        doc["serialNumber"] = serialNumber;
        doc["action"] = "gettime";

        char payload[128];
        serializeJson(doc, payload, sizeof(payload));

        mqttClient->publish("module/update", 1, false, payload);
        awaitingSmartboatTimeSync = true;

        Serial.println("📡 Time sync requested via module/update");
    }

    void hardResetClient()
    {
        if (mqttClient)
        {
            mqttClient->onConnect(nullptr);
            mqttClient->onDisconnect(nullptr);
            mqttClient->onMessage(nullptr);
            mqttClient->disconnect();
            logMessage(LOG_INFO, "🔌 MQTT client: disconnect requested");
            vTaskDelay(pdMS_TO_TICKS(150)); // Let the TCP stack catch up

            delete mqttClient;
            mqttClient = nullptr;
            logMessage(LOG_INFO, "🗑️ MQTT client deleted.");
        }
    }
}
