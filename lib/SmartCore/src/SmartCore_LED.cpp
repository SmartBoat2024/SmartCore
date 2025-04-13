#include "SmartCore_LED.h"
#include "SmartCore_Network.h"
#include "SmartCore_MQTT.h"
#include "SmartCore_Log.h"

QueueHandle_t flashQueue;
SemaphoreHandle_t ledMutex = nullptr;
TaskHandle_t flashLEDTaskHandle = nullptr;
TaskHandle_t provisioningBlinkTaskHandle = nullptr;
TaskHandle_t wifiMqttCheckTaskHandle = nullptr;
volatile LEDMode currentLEDMode = LEDMODE_OFF;
DebugColor currentDebugColor = DEBUG_COLOR_YELLOW;
ProvisioningState currentProvisioningState = PROVISIONING_NONE;
String flashPattern = "";

void initSmartCoreLED() {
    pinMode(RGB_r, OUTPUT);
    pinMode(RGB_g, OUTPUT);
    pinMode(RGB_b, OUTPUT);

    ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_BIT);
    ledcSetup(LEDC_CHANNEL_1, LEDC_BASE_FREQ, LEDC_TIMER_BIT);
    ledcSetup(LEDC_CHANNEL_2, LEDC_BASE_FREQ, LEDC_TIMER_BIT);

    ledcAttachPin(RGB_r, LEDC_CHANNEL_0);
    ledcAttachPin(RGB_g, LEDC_CHANNEL_1);
    ledcAttachPin(RGB_b, LEDC_CHANNEL_2);

    setRGBColor(0, 255, 0);  // Green on init

    ledMutex = xSemaphoreCreateMutex();
    if (!ledMutex)logMessage(LOG_INFO, "⚠️ Failed to create LED mutex");

    flashQueue = xQueueCreate(5, sizeof(String));
    if (!flashQueue) logMessage(LOG_INFO, "⚠️ Failed to create flash queue");

    xTaskCreatePinnedToCore(flashLEDTask, "Flash LED", 2048, NULL, 1, &flashLEDTaskHandle, 1);
    xTaskCreatePinnedToCore(provisioningBlinkTask, "Provision Blink", 2048, NULL, 1, &provisioningBlinkTaskHandle, 1);
    xTaskCreatePinnedToCore(wifiMqttCheckTask,      "WiFi/MQTT Check",     2048, NULL, 1, &wifiMqttCheckTaskHandle,      0);
}

void setRGBColor(uint8_t red, uint8_t green, uint8_t blue) {
    logMessage(LOG_INFO, "LED colour updated.");
    float rAdj = 0.8;
    float gAdj = 1.0;
    float bAdj = 1.0;

    uint32_t duty_r = map(255 - (uint8_t)(red * rAdj), 0, 255, 0, (1 << LEDC_TIMER_BIT) - 1);
    uint32_t duty_g = map(255 - (uint8_t)(green * gAdj), 0, 255, 0, (1 << LEDC_TIMER_BIT) - 1);
    uint32_t duty_b = map(255 - (uint8_t)(blue * bAdj), 0, 255, 0, (1 << LEDC_TIMER_BIT) - 1);

    ledcWrite(LEDC_CHANNEL_0, duty_r);
    ledcWrite(LEDC_CHANNEL_1, duty_g);
    ledcWrite(LEDC_CHANNEL_2, duty_b);
}

void triggerFlashPattern(const String& pattern, DebugColor color = DEBUG_COLOR_YELLOW) {
    if (!flashQueue) {
        logMessage(LOG_WARN, "⚠️ Flash queue not initialized.");
        return;
    }

    int available = uxQueueSpacesAvailable(flashQueue);
    logMessage(LOG_INFO, "🟨 Flash pattern trigger → pattern: [" + pattern + "], length: " + String(pattern.length()));
    logMessage(LOG_INFO, "🧮 Flash queue space: " + String(available));
    logMessage(LOG_INFO, "💡 Current LED mode: " + String(currentLEDMode));

    if (available > 0) {
        currentDebugColor = color;
        currentLEDMode = LEDMODE_DEBUG_PATTERN;

        // Create new pattern instance to avoid shared reference issues
        String* dynamicPattern = new String(pattern);
        BaseType_t result = xQueueSend(flashQueue, &dynamicPattern, 0);  // Non-blocking

        if (result == pdTRUE) {
            logMessage(LOG_INFO, "🟡 Pattern queued successfully.");
        } else {
            logMessage(LOG_WARN, "⚠️ Pattern queue send failed.");
            delete dynamicPattern;
        }
    } else {
        logMessage(LOG_WARN, "⚠️ Flash queue full. Pattern not queued.");
    }
}

void flashLEDTask(void *parameter) {
    String* patternPtr = nullptr;

    for (;;) {
        if (xQueueReceive(flashQueue, &patternPtr, portMAX_DELAY) == pdTRUE) {
            String pattern = *patternPtr;
            delete patternPtr;  // Clean up
            logMessage(LOG_INFO, "⚡ FlashLEDTask running pattern: [" + pattern + "]");

            if (xSemaphoreTake(ledMutex, portMAX_DELAY) == pdTRUE) {
                setRGBColor(0, 255, 255);  // Cyan blip
                vTaskDelay(pdMS_TO_TICKS(150));
                turnOffRGBLEDs();
                vTaskDelay(pdMS_TO_TICKS(150));

                for (char symbol : pattern) {
                    Serial.printf("  → Flash symbol: %c\n", symbol);

                    switch (symbol) {
                        case '.':
                        case '-':
                            switch (currentDebugColor) {
                                case DEBUG_COLOR_RED:     setRGBColor(255, 0, 0); break;
                                case DEBUG_COLOR_CYAN:    setRGBColor(0, 255, 255); break;
                                case DEBUG_COLOR_MAGENTA: setRGBColor(255, 0, 255); break;
                                default:                  setRGBColor(255, 255, 0); break;
                            }

                            vTaskDelay(pdMS_TO_TICKS(symbol == '.' ? 200 : 600));
                            turnOffRGBLEDs();
                            vTaskDelay(pdMS_TO_TICKS(200));
                            break;

                        case '~':
                            setRGBColor(0, 255, 255);
                            vTaskDelay(pdMS_TO_TICKS(500));
                            turnOffRGBLEDs();
                            break;

                        default:
                            logMessage(LOG_WARN, String("❓ Unknown flash symbol: ") + symbol);
                            break;
                    }
                }

                vTaskDelay(pdMS_TO_TICKS(2000));
                xSemaphoreGive(ledMutex);
            } else {
                logMessage(LOG_WARN, "❌ Could not take LED mutex during flash.");
            }

            logMessage(LOG_INFO, "✅ Flash pattern complete. Returning to STATUS mode.");
            currentLEDMode = LEDMODE_STATUS;
        } else {
            logMessage(LOG_WARN, "⛔ FlashLEDTask failed to receive pattern from queue.");
        }
    }
}

// Function to check WiFi and MQTT connection
void wifiMqttCheckTask(void *parameter) {
    for (;;) {
        // 🚨 1. If we're not in STATUS mode, skip
        if (currentLEDMode != LEDMODE_STATUS) {
            logMessage(LOG_INFO, "⏳ LED mode is not STATUS (mode = " + String(currentLEDMode) + "), skipping LED update.");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }

        // 🧯 2. Watchdog for WiFiManager lockup
        if (!wifiManagerFinished && millis() - wifiManagerStartTime > 300000) {
            logMessage(LOG_WARN, "🛑 WiFiManager hang detected! Triggering emergency reset...");
            String emergencyPattern = "..--";
            xQueueSend(flashQueue, (void *)&emergencyPattern, 0);
            delay(500);
            ESP.restart();
        }

        // ⚠️ 3. If a flash pattern is running, don't mess with LEDs
        if ((flashQueue && uxQueueMessagesWaiting(flashQueue) > 0) || currentLEDMode != LEDMODE_STATUS) {
            logMessage(LOG_INFO, "⏳ LED update skipped — pattern running or mode not STATUS");
            logMessage(LOG_INFO, "🧮 Queue count: " + String(uxQueueMessagesWaiting(flashQueue)) + ", LED mode: " + String(currentLEDMode));
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }

        // ✅ 4. Proceed to update LED based on current state
        if (xSemaphoreTake(ledMutex, portMAX_DELAY) == pdTRUE) {
            if (resetConfig) {
                logMessage(LOG_INFO, "🟢 LED: Reset Config Mode");
                setRGBColor(0, 255, 0);  // Green
            } else if (standaloneMode) {
                logMessage(LOG_INFO, "🟣 LED: Standalone Mode");
                setRGBColor(130, 0, 230);  // Magenta
            } else if (WiFi.status() != WL_CONNECTED) {
                logMessage(LOG_INFO, "🔴 LED: No WiFi");
                setRGBColor(255, 0, 0);  // Red
            } else if (!mqttIsConnected) {
                logMessage(LOG_INFO, "💗 LED: WiFi OK, MQTT not connected");
                setRGBColor(255, 60, 80);  // Hot Pink
            } else {
                logMessage(LOG_INFO, "🔵 LED: WiFi + MQTT connected");
                setRGBColor(0, 0, 255);  // Blue
            }

            xSemaphoreGive(ledMutex);
        } else {
            logMessage(LOG_WARN, "⚠️ Failed to take LED mutex in wifiMqttCheckTask");
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}


  void provisioningBlinkTask(void *parameter) {
    for (;;) {
        if (currentLEDMode == LEDMODE_WAITING) {
            if (xSemaphoreTake(ledMutex, portMAX_DELAY) == pdTRUE) {
                if (currentProvisioningState == PROVISIONING_PORTAL) {
                    logMessage(LOG_INFO, "💡 Blinking Cyan: WiFiManager portal open");
                    setRGBColor(0, 255, 255);  // Cyan
                } else if (currentProvisioningState == PROVISIONING_SMARTCONNECT) {
                    logMessage(LOG_INFO, "💡 Blinking Red: Waiting for SmartConnect");
                    setRGBColor(255, 0, 0);  // Red
                }

                vTaskDelay(pdMS_TO_TICKS(400));  // LED on
                setRGBColor(0, 0, 0);            // LED off
                vTaskDelay(pdMS_TO_TICKS(400));  // LED off delay

                xSemaphoreGive(ledMutex);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(500));  // Sleep quietly when not in WAITING mode
        }
    }
}

void setLEDMode(LEDMode mode) {
    if (mode != currentLEDMode) {
        currentLEDMode = mode;
       logMessage(LOG_INFO, "🔁 LED Mode changed to: " + String(mode));
    }
}

void setFailedWiFiCredsTask(void *parameter) {
    int blinkDuration = 5000;  // Total blink time in milliseconds
    int blinkInterval = 500;   // Time between blinks in milliseconds (0.5 seconds on/off)
    unsigned long startTime = millis();

    // Lock the LED mutex before starting the LED task
    if (xSemaphoreTake(ledMutex, portMAX_DELAY) == pdTRUE) {
        while (millis() - startTime < blinkDuration) {
            // Turn on the red LED
            setRGBColor(255, 0, 0);  // Red color
            vTaskDelay(blinkInterval / 2 / portTICK_PERIOD_MS);  // LED on for half the interval

            // Turn off the LED
            setRGBColor(0, 0, 0);  // LED off
            vTaskDelay(blinkInterval / 2 / portTICK_PERIOD_MS);  // LED off for half the interval
        }
        // Release the LED mutex
        xSemaphoreGive(ledMutex);
    }

    vTaskDelete(NULL);  // End the task when done
}



void turnOffRGBLEDs() {
    logMessage(LOG_INFO, "Turning off LEDs via PWM");
    setRGBColor(0, 0, 0);  // Set to black/off via PWM
}

