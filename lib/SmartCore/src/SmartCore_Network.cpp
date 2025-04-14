#include "SmartCore_Network.h"

// --- Buffers ---
char serialNumber[40] = "UNKNOWN";
String moduleName = "Smart Module";
char webnamechar[40] = "SmartModule";
char apName[50] = "SmartConnectAP";
char apPassword[40] = "12345678";
char custom_ap_name[40] = "SmartConnectAP";
char custom_ap_pass[40] = "12345678";
String location = "location";
// --- Module Parameters ---
String webname = "smartmodule";

// --- ESPAsync_WM Parameters ---
ESPAsync_WMParameter* custom_serial = nullptr;
ESPAsync_WMParameter* custom_webname = nullptr;

// --- WiFi State ---
unsigned long wifiManagerStartTime = 0;
bool wifiManagerFinished = false;
bool wifiIsConnected = false;
bool wifiSetupComplete = false;
bool firstWiFiConnect = true;
volatile bool provisioningComplete = false;

// --- MQTT State ---
AsyncMqttClient mqttClient;
bool awaitingSmartboatTimeSync = true;
uint32_t smartBoatEpoch = 0;
uint32_t smartBoatEpochSyncMillis = 0;
unsigned long lastMqttReconnectAttempt = 0;
unsigned long mqttReconnectInterval = 5000;
char mqtt_server[16] = "";
int mqtt_port = 1883;
char custom_mqtt_server[16] = "";
int custom_mqtt_port = 1883;
bool customMqtt = false;
char mqtt_user[50] = "";
const char* mqtt_user_prefix = "rel";
bool mqttIsConnected = false;
char mqttPrefix[6] ="";  // 4 chars + nul

// --- Mode Flags ---
bool resetConfig = true;
bool smartBoatEnabled = true;
bool smartConnectEnabled = false;
bool standaloneMode = false;
bool standaloneFlag = false;
bool autoProvisioned = false;
bool serialNumberAssigned = false;

// --- Webserver ---
uint8_t clientIDs[5];
int numClients = 0;
int lastClientCheck = 0;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");