#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// LED channels and pins (can be adjusted as needed)
#define LEDC_CHANNEL_0 0
#define LEDC_CHANNEL_1 1
#define LEDC_CHANNEL_2 2
#define LEDC_BASE_FREQ 5000
#define LEDC_TIMER_BIT 8

// Pin definitions (overrideable)
#ifndef RGB_r
#define RGB_r 1
#endif

#ifndef RGB_g
#define RGB_g 2
#endif

#ifndef RGB_b
#define RGB_b 42
#endif

// Flash pattern queue and LED control
extern QueueHandle_t flashQueue;
extern SemaphoreHandle_t ledMutex;

// Task handle
extern TaskHandle_t flashLEDTaskHandle;
extern TaskHandle_t provisioningBlinkTaskHandle;
extern TaskHandle_t wifiMqttCheckTaskHandle;

// LED control mode enum
enum LEDMode {
    LEDMODE_STATUS,
    LEDMODE_WAITING,
    LEDMODE_DEBUG_PATTERN,
    LEDMODE_OFF
};

enum DebugColor {
    DEBUG_COLOR_YELLOW,  // MQTT
    DEBUG_COLOR_RED,     // WiFi
    DEBUG_COLOR_CYAN,    // Reserved or future use
    DEBUG_COLOR_MAGENTA, // Optional future use
};

enum ProvisioningState {
    PROVISIONING_NONE,
    PROVISIONING_PORTAL,
    PROVISIONING_SMARTCONNECT
};

extern volatile LEDMode currentLEDMode;
extern DebugColor currentDebugColor;
extern ProvisioningState currentProvisioningState;
extern String flashPattern;

// Function declarations
void initSmartCoreLED();
void setRGBColor(uint8_t r, uint8_t g, uint8_t b);
void triggerFlashPattern(const String& pattern, DebugColor color);
void flashLEDTask(void* parameter);
void provisioningBlinkTask(void* parameter);
void turnOffRGBLEDs();
void setLEDMode(LEDMode mode);
void wifiMqttCheckTask(void *parameter);
void setFailedWiFiCredsTask(void *parameter);

