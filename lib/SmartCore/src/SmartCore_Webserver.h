#pragma once

#include <Arduino.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>

namespace SmartCore_Websocket {

    extern StaticJsonDocument<2048> doc;
    extern StaticJsonDocument<1024> doc_rx;

    void setupMDNS();
    void setupWebServer();

    void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                   AwsEventType type, void *arg, uint8_t *data, size_t len);
    void sendAcknowledgment(AsyncWebSocketClient* client);
    void onClientDisconnect(AsyncWebSocketClient* client);
    void onClientConnect(AsyncWebSocketClient* client);
    void handleWsMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len);
    void sendResponseWithRequestId(AsyncWebSocketClient* client, bool success, const char* message, const String& requestId);
    void send(const String& json);
    void send(JsonDocument& doc);

}