#pragma once
#include <ArduinoJson.h>
#include <AsyncWebSocket.h>

bool handleModuleWebSocketMessage(AsyncWebSocketClient* client, JsonDocument& doc);