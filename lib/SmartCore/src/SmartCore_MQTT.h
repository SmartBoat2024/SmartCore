#pragma once

#include <Arduino.h>
#include <AsyncMqttClient.h>

namespace SmartCore_MQTT {

extern TaskHandle_t mqttReconnectTaskHandle;

//void setupMQTTClient(const char* server = nullptr, int port = 0, bool forceConnect = true);
void setupMQTTClient();
void generateMqttPrefix();
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void scheduleReconnect();
void mqttReconnectTask(void *param);  // ✅ Corrected with semicolon

}