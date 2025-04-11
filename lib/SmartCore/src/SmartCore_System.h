#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include "SmartCoreVersion.h"
#include "SmartCore_config.h"


namespace SmartCore_System {
    void init();
    void setModuleSettings(const SmartCoreSettings& settings);
    const SmartCoreSettings& getModuleSettings();
}

// LED pin and button constants (override externally if needed)
#ifndef RGB_r
#define RGB_r 3
#endif

#ifndef RGB_g
#define RGB_g 4
#endif

#ifndef RGB_b
#define RGB_b 5
#endif

#ifndef buttonPin
#define buttonPin 6
#endif

// LED PWM setup
#define LEDC_CHANNEL_0 0
#define LEDC_CHANNEL_1 1
#define LEDC_CHANNEL_2 2
#define LEDC_TIMER_BIT 8
#define LEDC_BASE_FREQ 5000

// Shared state
extern SemaphoreHandle_t ledMutex;
extern QueueHandle_t flashQueue;

// System init
void initSmartCoreSystem();
void setRGBColor(uint8_t red, uint8_t green, uint8_t blue);
void turnOffRGBLEDs();
