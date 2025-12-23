#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include "SmartCoreVersion.h"
#include "SmartCore_config.h"

#define CRASH_LIMIT_RESET 3
#define CRASH_LIMIT_SAFE 6

enum BootFaultReason {
    BOOT_FAULT_NONE,
    BOOT_FAULT_CRASH_LIMIT,
    BOOT_FAULT_LITTLEFS
};

enum CrashCounterType
{
    CRASH_COUNTER_BOOT,
    CRASH_COUNTER_RUNTIME,
    CRASH_COUNTER_ALL // for full wipes if ever needed
};

enum ModuleErrorCode {
    ERR_BOOT_CRASH_LIMIT   = 1001,
    ERR_LITTLEFS_MOUNT     = 1002,
    ERR_SAFE_BOOT_ACTIVE   = 1003
};


namespace SmartCore_System
{
    extern volatile bool runtimeStable;
    extern volatile uint32_t runtimeStableSince;

    void markRuntimeStable();

    extern const unsigned long holdTime;
    extern unsigned long buttonPressStart;
    extern bool buttonPressed;

    extern TaskHandle_t otaTaskHandle;
    extern TaskHandle_t resetButtonTaskHandle;
    extern volatile bool bootSafeMode;
    extern volatile BootFaultReason bootFault;

    void preinit();
    void init();
    void setModuleSettings(const SmartCoreSettings &settings);
    const SmartCoreSettings &getModuleSettings();
    void getModuleConfig();
    void createAppTasks();
    void checkresetButtonTask(void *parameter);
    void resetWorkerTask(void *param);
    void clearCrashCounter(CrashCounterType type);
    void enterSafeMode();
    void loadMqttPriorityList();

}

extern "C" void SmartBox_notifyBecomePrimary();
extern "C" void SmartBox_notifySetBroker(const String &newIp);
extern "C" void SmartBox_notifyProvisionReceived(
    const String &ssid,
    const String &password,
    const String &serial,
    bool mqttStatic,
    const String &staticIp,
    const String &subnetMask,
    const String ipList[], // ordered SmartBox list
    int ipCount,           // number of SmartBoxes
    int priority,          // my priority (1 = primary)
    int mqttPort);

// LED pin and button constants (override externally if needed)

#ifndef buttonPin
#define buttonPin 0
#endif
