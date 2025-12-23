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
extern AsyncMqttClient *mqttClient;

extern unsigned long lastMqttReconnectAttempt;
extern unsigned long mqttReconnectInterval;
extern bool awaitingSmartboatTimeSync;
extern uint32_t smartBoatEpoch;
extern uint32_t smartBoatEpochSyncMillis;

extern char mqtt_server[16];
extern int mqtt_port;

// --- SmartModule Priority MQTT Servers (max 3) ---
extern uint16_t mqttPriorityCount;   // 0–3
extern char mqttPriorityList[3][17]; // 3 priority servers

// --- SmartBox Static IP Configuration ---
extern bool mqttStaticEnabled; // TRUE if using static IP
extern char staticIp[17];      // "xxx.xxx.xxx.xxx"
extern char staticMask[17];    // "255.255.255.0"

// --- Backup SmartBoxes (max 2) ---
extern uint16_t backupBoxCount;  // 0–2 (2 bytes because writeIntToEEPROM)
extern char backupBoxIPs[2][17]; // up to 2 SmartBoxes

extern char mqtt_user[50];
extern const char *mqtt_user_prefix;

extern bool mqttIsConnected;
extern char mqttPrefix[6];

// --- SmartCore Mode Flags ---
extern bool resetConfig;
extern bool autoProvisioned;
extern bool serialNumberAssigned;