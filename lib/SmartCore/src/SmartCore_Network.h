#pragma once
#include <SmartConnect_Async_WiFiManager.h>
#include <AsyncMqttClient.h>

#define METRICS_INTERVAL 30000

// --- WiFiManager Parameters ---
extern ESPAsync_WMParameter* custom_serial;
extern ESPAsync_WMParameter* custom_webname;

extern char serialNumber[40];
extern String moduleName;
extern char webnamechar[40];
extern String webname;
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

extern char custom_mqtt_server[16];
extern int custom_mqtt_port;
extern bool customMqtt;

extern char mqtt_user[50];
extern const char* mqtt_user_prefix;

extern bool mqttIsConnected;
extern char mqttPrefix[6];

// --- SmartCore Mode Flags ---
extern bool resetConfig;
extern bool smartBoatEnabled;
extern bool smartConnectEnabled;
extern bool standaloneMode;
extern bool standaloneFlag;
extern bool autoProvisioned;
extern bool shouldUpdateFirmware;
extern bool isUpgradeAvailable;
extern bool serialNumberAssigned;

// --- Webserver ---
extern uint8_t clientIDs[5];
extern int numClients;
extern int lastClientCheck;
extern AsyncWebSocket ws;
extern AsyncWebServer server;