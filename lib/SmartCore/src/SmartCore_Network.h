#pragma once
#include <AsyncMqttClient.h>

#define METRICS_INTERVAL 30000

extern char serialNumber[40];
extern String moduleName;
extern String location;

extern char apName[50];
extern char apPassword[40];
extern char custom_ap_name[40];
extern char custom_ap_pass[40];

extern unsigned long wifiManagerStartTime;
extern bool wifiManagerFinished;

// --- WiFi Flags ---
extern bool wifiIsConnected;
extern bool wifiSetupComplete;
extern bool firstWiFiConnect;
extern volatile bool provisioningComplete;

// --- MQTT Parameters ---
extern AsyncMqttClient mqttClient;

extern unsigned long lastMqttReconnectAttempt;
extern unsigned long mqttReconnectInterval;
extern bool awaitingSmartboatTimeSync;
extern uint32_t smartBoatEpoch;
extern uint32_t smartBoatEpochSyncMillis;

extern char mqtt_server[16];
extern int mqtt_port;

extern char mqtt_user[50];
extern const char* mqtt_user_prefix;

extern bool mqttIsConnected;
extern char mqttPrefix[6];

// --- SmartCore Mode Flags ---
extern bool resetConfig;
extern bool autoProvisioned;
extern bool serialNumberAssigned;
