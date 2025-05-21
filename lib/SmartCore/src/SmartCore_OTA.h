#pragma once
#include <arduino.h>
#include <AsyncWebSocket.h> 

namespace SmartCore_OTA {

extern TaskHandle_t otaTaskHandle;

extern bool isUpgradeAvailable;
extern bool shouldUpdateFirmware;

void otaTask(void *parameter);
void onUpdateProgress(int progress, int total);
void ota();
}