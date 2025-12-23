#pragma once

#include <Arduino.h>
#include <AsyncMqttClient.h>

namespace SmartCore_MQTT
{

    extern String mqttConfigMessageBuffer;
    extern String mqttErrorsMessageBuffer;
    extern String mqttModuleMessageBuffer;
    extern String mqttResetMessageBuffer;
    extern String mqttUpgradeMessageBuffer;
    extern String mqttUpdateMessageBuffer;

    extern String currentBrokerIP;
    extern uint16_t currentBrokerPort;
    extern int currentPriorityIndex;
    extern bool failoverInProgress;
    extern bool mqttFailoverAckReceived;
    extern String pendingBrokerIP;
    extern uint16_t pendingBrokerPort;

    extern TaskHandle_t metricsTaskHandle;
    extern TaskHandle_t timeSyncTaskHandle;
    void setupMQTTClient(const String &ip, uint16_t port);
    void handleMQTTFailover();
    void commitPendingBroker();
    void generateMqttPrefix();
    void onMqttConnect(bool sessionPresent);
    void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
    void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
    const char *mqttDisconnectReasonToStr(AsyncMqttClientDisconnectReason reason);
    void metricsTask(void *parameter);
    void handleErrorMessage(const String &message);
    void handleConfigMessage(const String &message);
    void handleUpdateMessage(const String &message);
    void handleUpgradeMessage(const String &message);
    void handleModuleMessage(const String &message);
    void handleResetMessage(const String &message);
    void requestSmartBoatTime();
    void timeSyncTask(void *parameter);
    void hardResetClient();
    bool fetchMQTTConfig(String &mqttIp, uint16_t &mqttPort);
    void publishModuleError(
        const String &message,
        uint16_t errorNumber,
        const String &category,
        const String &type,
        const String &status = "active"
    );
     bool mqttSafePublish(
        const char* topic,
        uint8_t qos,
        bool retain,
        const char* payload,
        size_t len = 0
    );
}