#include "SmartCore_MQTT.h"
#include <arduino.h>
#include <AsyncMqttClient.h>
#include "SmartCore_Network.h"
#include "SmartCore_LED.h"
#include "SmartCore_EEPROM.h"
#include "SmartCore_Log.h"
#include <functional>

namespace SmartCore_MQTT {
    char mqttPrefix[6];  // 4 chars + null
    char mqttWillTopic[64];
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
    
        Serial.print("📏 mqttWillTopic length: ");
        Serial.println(strlen(mqttWillTopic));
        Serial.print("🧾 mqttWillTopic bytes: ");
        for (size_t i = 0; i < strlen(mqttWillTopic); ++i) {
            Serial.printf("[%02X]", mqttWillTopic[i]);
        }
        Serial.println();
    }

    void setupMQTTClient() {
        logMessage(LOG_INFO, "🔧 Configuring MQTT...");
        generateMqttPrefix();  // Also sets mqttWillTopic internally
    
        // Set up callbacks and keepalive
        mqttClient.onConnect(onMqttConnect);
        mqttClient.onDisconnect(onMqttDisconnect);
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
        mqttIsConnected = true;  // Set MQTT connection flag
        if (mqttReconnectTaskHandle) {
            vTaskDelete(mqttReconnectTaskHandle);
            mqttReconnectTaskHandle = NULL;
            Serial.println("🛑 Reconnect task stopped");
        }
        // Subscribe to topics
        mqttClient.subscribe((String(mqttPrefix) + "/#").c_str(), 1);   // Subscribe to all topics under prefix
        
        // Publish connected message
        mqttClient.publish((String(mqttPrefix) + "/connected").c_str(), 1, true, "connected");
        mqttClient.publish((String(mqttPrefix) + "/update").c_str(), 1, false, R"({"update":"time"})");
        awaitingSmartboatTimeSync = true;
      
        // Handle first WiFi connection scenario
        if (firstWiFiConnect) {
          char topic[50];
          snprintf(topic, sizeof(topic), (String(mqttPrefix) + "/newModule").c_str());
          mqttClient.publish(topic, 1, true, serialNumber);  // Publish retained message
          firstWiFiConnect = false;
          SmartCore_EEPROM::writeFirstWiFiConnectFlag(false);  // Save flag in EEPROM
        }
      }

      void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
        mqttIsConnected = false;  // Update connection state
        logMessage(LOG_WARN, "❌ Disconnected from MQTT (" + String((int)reason) + ")");
        currentLEDMode = LEDMODE_DEBUG_PATTERN;
    
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
    
}
