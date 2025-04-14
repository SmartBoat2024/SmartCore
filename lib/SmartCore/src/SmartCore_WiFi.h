#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <SmartConnect_Async_WiFiManager.h>
//#include <AsyncDNSServer.h>

namespace SmartCore_WiFi {
    extern TaskHandle_t wifiManagerTaskHandle;

    void startWiFiManagerTask();       // To start the FreeRTOS WiFi task
    void wifiManagerTask(void *parameter); //wifimanager task
    void setupWiFiManager();           // Main setup logic (internal)
    void handleFailedConnection();     // Called if connection fails
    void saveConfigAndRestart();       // Save settings + restart
    void startNetworkServices(bool mqttRequired); //start mqtt, smartNet and webserver
    void startMinimalWifiSetup();
}