#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
//#include <AsyncDNSServer.h>

namespace SmartCore_WiFi {
    extern TaskHandle_t wifiProvisionTaskHandle;

    void startWiFiProvisionTask();       // To start the FreeRTOS WiFi task
    void wifiProvisionTask(void *parameter); //wifimanager task
    void handleFailedConnection();     // Called if connection fails
    void waitForValidIP();    // Save settings + restart
    void startNetworkServices(bool mqttRequired); //start mqtt, smartNet and webserver
    void startMinimalWifiSetup();
}