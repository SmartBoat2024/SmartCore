#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include "SmartCoreVersion.h"
#include "SmartCore_config.h"


namespace SmartCore_System {
    extern const unsigned long holdTime;
    extern unsigned long buttonPressStart;
    extern bool buttonPressed;

    void preinit();
    void init();
    void setModuleSettings(const SmartCoreSettings& settings);
    void getModuleConfig();
    void getModuleSpecificConfig();
    void createAppTasks();
    void checkresetButtonTask(void *parameter);
    void resetWorkerTask(void *param);

}

// LED pin and button constants (override externally if needed)

#ifndef buttonPin
#define buttonPin 0
#endif



