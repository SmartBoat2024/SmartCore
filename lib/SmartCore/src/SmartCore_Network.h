#pragma once
#include <SmartConnect_Async_WiFiManager.h>
#include <AsyncMqttClient.h>

// --- WiFiManager Parameters ---
extern ESPAsync_WMParameter custom_serial;
extern ESPAsync_WMParameter custom_webname;

extern char serialNumber[40];
extern char moduleName[40];
extern char webnamechar[40];
extern String webname;

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

extern char mqtt_server[16];
extern int mqtt_port;

extern char custom_mqtt_server[16];
extern int custom_mqtt_port;
extern bool customMqtt;

extern char mqtt_user[50];
extern const char* mqtt_user_prefix;

extern bool mqttIsConnected;

// --- SmartCore Mode Flags ---
extern bool resetConfig;
extern bool smartBoatEnabled;
extern bool smartConnectEnabled;
extern bool standaloneMode;
extern bool standaloneFlag;
extern bool autoProvisioned;