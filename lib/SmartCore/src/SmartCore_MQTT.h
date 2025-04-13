#pragma once

#include <Arduino.h>
#include <AsyncMqttClient.h>

namespace SmartCore_MQTT {

extern TaskHandle_t mqttReconnectTaskHandle;

extern TaskHandle_t metricsTaskHandle;
extern TaskHandle_t timeSyncTaskHandle;
void setupMQTTClient();
void generateMqttPrefix();
void onMqttConnect(bool sessionPresent);
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void scheduleReconnect();
void mqttReconnectTask(void *param);
void metricsTask(void *parameter);
void handleErrorMessage(const String& message);
void handleConfigMessage(const String& message);
void handleUpdateMessage(const String& message);
void handleUpgradeMessage(const String& message);
void handleModuleMessage(const String& message);
void handleResetMessage(const String& message);
void requestSmartBoatTime();
void timeSyncTask(void* parameter);
}