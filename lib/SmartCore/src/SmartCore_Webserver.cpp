#include "SmartCore_Webserver.h"
#include <Arduino.h>
#include "SmartCore_Network.h"
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <AsyncWebSocket.h> 
#include "SmartCore_Log.h"
#include <ArduinoJson.h>
#include "SmartCore_OTA.h"
#include "websocket_funcs.h"
#include "SmartCore_EEPROM.h"


namespace SmartCore_Websocket{
    
    StaticJsonDocument<2048> doc;
    StaticJsonDocument<1024> doc_rx;

    void setupMDNS() {
        // Ensure webname is synced from webnamechar (which is loaded from EEPROM)
        webname = String(webnamechar);

        logMessage(LOG_INFO, "🔧 mDNS Webname: " + String(webname));

        if (webname.length() > 0) {
            if (MDNS.begin(webname.c_str())) {
            logMessage(LOG_INFO, "✅ mDNS started");
            } else {
                logMessage(LOG_ERROR, "❌ mDNS failed");
            }
            MDNS.addService("http", "tcp", 80);
        } else {
            logMessage(LOG_WARN, "⚠️ No valid webname. Skipping mDNS.");
        }
    }

    void setupWebServer() {
        logMessage(LOG_INFO, "🌐 Starting Async Web Server...");
    
        server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
        server.serveStatic("/images", LittleFS, "/images/");
    
        ws.onEvent(onWsEvent);
        server.addHandler(&ws);
    
        server.onNotFound([](AsyncWebServerRequest *request) {
        logMessage(LOG_WARN, "⚠️ Not found: " + request->url());
        request->send(404, "text/plain", "Not Found");
        });
    
        server.begin();
    }

    void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
        AwsEventType type, void *arg, uint8_t *data, size_t len) {
        
        switch (type) {
            case WS_EVT_CONNECT: {
                onClientConnect(client);
                String msg = "Client #" + String(client->id()) + " connected from " + client->remoteIP().toString();
                logMessage(LOG_INFO, msg);
                break;
            }

            case WS_EVT_DISCONNECT: {
                onClientDisconnect(client);
                String msg = "Client #" + String(client->id()) + " disconnected";
                logMessage(LOG_INFO, msg);
                break;
            }

            case WS_EVT_DATA:
                if (data) {
                    handleWsMessage(client, data, len);
                }
                break;

            default:
                break;
        }
    }

    void handleWsMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
        // Clear the document before use
        doc_rx.clear();

        // Deserialize the incoming JSON payload
        DeserializationError error = deserializeJson(doc_rx, data, len);
        if (error) {
            Serial.println("deserializationJson() failed");
            return;
        }

        // Debugging: Print the deserialized JSON
        String settingsData;
        serializeJsonPretty(doc_rx, settingsData);
        //logMessage(LOG_INFO, "settings data: " + settingsData);  // Uncomment for debugging
        bool settingsChanged = false;

        String msgType = doc_rx["type"] | "";

        if (msgType == "updateSettings") {
            String newModuleName = doc_rx["moduleName"] | "Smart Module";
            String newWebName = doc_rx["webname"] | "smartmodule";
            bool newSmartBoat = doc_rx["smartboat"] | false;
            bool newCustomMqtt = doc_rx["customMqtt"] | false;
            String newMqttServer = doc_rx["customMqttServer"] | "";
            int newMqttPort = doc_rx["customMqttPort"] | 1883;
        
            // 🧠 Update runtime variables
            moduleName = newModuleName;
            webname = newWebName;
            smartBoatEnabled = newSmartBoat;
            customMqtt = newCustomMqtt;
            newMqttServer.toCharArray(custom_mqtt_server, sizeof(custom_mqtt_server));
            custom_mqtt_port = newMqttPort;
        
            // 💾 Save to EEPROM using your SmartCore_EEPROM methods
            SmartCore_EEPROM::writeModuleNameToEEPROM(moduleName);
            SmartCore_EEPROM::writeCustomWebnameToEEPROM(); // assumes it uses `webname` internally
            SmartCore_EEPROM::writeSmartBoatToEEPROM(smartBoatEnabled);
        
            SmartCore_EEPROM::writeStringToEEPROM(CUSTOM_MQTT_SERVER_ADDR, newMqttServer);
            SmartCore_EEPROM::writeIntToEEPROM(newMqttPort, CUSTOM_MQTT_PORT_ADDR);
            SmartCore_EEPROM::writeBoolToEEPROM(CUSTOM_MQTT_ADDR , customMqtt);
        
            // Optional debug
            logMessage(LOG_INFO, "💾 Settings saved:");
            logMessage(LOG_INFO, "   Module Name: " + moduleName);
            logMessage(LOG_INFO, "   Web Name: " + webname);
            logMessage(LOG_INFO, "   SmartBoat: " + String(smartBoatEnabled));
            logMessage(LOG_INFO, "   MQTT Server: " + newMqttServer);
            logMessage(LOG_INFO, "   MQTT Port: " + String(custom_mqtt_port));
        
            sendAcknowledgment(client);
            return;
        }

        if (handleModuleWebSocketMessage(client, doc_rx)) {
            logMessage(LOG_INFO, "✅ Module-specific WebSocket message handled.");
            sendAcknowledgment(client);
        } else {
            logMessage(LOG_WARN, "⚠️ WebSocket message type not handled.");
        }
    }

    void onClientConnect(AsyncWebSocketClient* client) {
        if (!client) return;
        logMessage(LOG_INFO, "🟢 New client connected! ID: " + String(client->id()));
    
        int connectedCount = 0;
        for (AsyncWebSocketClient& c : ws.getClients()) {
            if (c.status() == WS_CONNECTED) {
                connectedCount++;
            }
        }
    
        logMessage(LOG_INFO, "🧠 Total Connected Clients: " + String(connectedCount));
    }
    
    void onClientDisconnect(AsyncWebSocketClient* client) {
        if (!client) return;
        logMessage(LOG_INFO, "🔴 Client disconnected. ID: " + String(client->id()));
    
        int connectedCount = 0;
        for (AsyncWebSocketClient& c : ws.getClients()) {
            if (c.status() == WS_CONNECTED) {
                connectedCount++;
            }
        }
    
        logMessage(LOG_INFO, "🧠 Total Connected Clients: " + String(connectedCount));
    }
    
    void sendAcknowledgment(AsyncWebSocketClient* client) {
        if (!client) return;
    
        DynamicJsonDocument ackDoc(1024);
        ackDoc["type"] = "ack";
        ackDoc["status"] = "ok";
    
        String ackJson;
        serializeJson(ackDoc, ackJson);
        client->text(ackJson);
    
        logMessage(LOG_INFO, "📨 Sent acknowledgment to client ID: " + String(client->id()));
    }

    void sendResponseWithRequestId(AsyncWebSocketClient* client, bool success, const char* message, const String& requestId) {
        DynamicJsonDocument response(1024);
        response["type"] = "response";
        response["success"] = success;
        response["message"] = message;
        response["requestId"] = requestId;
    
        String responseJson;
        serializeJson(response, responseJson);
        client->text(responseJson);
    }
    

    void send(const String& json) {
        ws.textAll(json);  // Broadcast to all connected clients
    }

    void send(JsonDocument& doc) {
        String jsonString;
        serializeJson(doc, jsonString);
        send(jsonString);
    }
}