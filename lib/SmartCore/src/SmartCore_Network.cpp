#include "SmartCore_Network.h"
#include <ESPAsyncWebServer.h>

// --- Buffers ---
char serialNumber[40] = "UNKNOWN";
String moduleName = "Smart Module";
char apName[50] = "SmartConnectAP";
char apPassword[40] = "12345678";
char custom_ap_name[40] = "SmartConnectAP";
char custom_ap_pass[40] = "12345678";
String location = "location";
// --- Module Parameters ---

// --- WiFi State ---
unsigned long wifiManagerStartTime = 0;
bool wifiManagerFinished = false;
bool wifiIsConnected = false;
bool wifiSetupComplete = false;
bool firstWiFiConnect = true;
volatile bool provisioningComplete = false;

// --- MQTT State ---
AsyncMqttClient* mqttClient = nullptr;
bool awaitingSmartboatTimeSync = true;
uint32_t smartBoatEpoch = 0;
uint32_t smartBoatEpochSyncMillis = 0;
unsigned long lastMqttReconnectAttempt = 0;
unsigned long mqttReconnectInterval = 5000;
char mqtt_server[16] = "";
int mqtt_port = 1883;
char mqtt_user[50] = "";
const char* mqtt_user_prefix = "rel";
bool mqttIsConnected = false;
char mqttPrefix[6] ="";  // 4 chars + nul

// --- Mode Flags ---
bool resetConfig = true;
bool autoProvisioned = false;
bool serialNumberAssigned = false;

