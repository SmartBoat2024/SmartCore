#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include "SmartCoreVersion.h"
#include "SmartCore_config.h"

#define CRASH_LIMIT_RESET  3
#define CRASH_LIMIT_SAFE   6

enum CrashCounterType {
    CRASH_COUNTER_BOOT,
    CRASH_COUNTER_RUNTIME,
    CRASH_COUNTER_ALL  // for full wipes if ever needed
};

namespace SmartCore_System {

    extern const unsigned long holdTime;
    extern unsigned long buttonPressStart;
    extern bool buttonPressed;

    extern TaskHandle_t otaTaskHandle;
    extern TaskHandle_t resetButtonTaskHandle;

    void preinit();
    void init();
    void setModuleSettings(const SmartCoreSettings& settings);
    void getModuleConfig();
    void createAppTasks();
    void checkresetButtonTask(void *parameter);
    void resetWorkerTask(void *param);
    void clearCrashCounter(CrashCounterType type);
    void enterSafeMode();

}

// LED pin and button constants (override externally if needed)

#ifndef buttonPin
#define buttonPin 0
#endif



