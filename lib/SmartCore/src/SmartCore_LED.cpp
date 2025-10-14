#include "SmartCore_LED.h"
#include "SmartCore_Network.h"
#include "SmartCore_System.h"
#include "SmartCore_MQTT.h"
#include "SmartCore_Log.h"
#include "SmartCore_EEPROM.h"
#include "SmartCore_Wifi.h"
#include <WiFi.h>
#include <cstdint>


QueueHandle_t flashQueue;
SemaphoreHandle_t ledMutex = nullptr;
TaskHandle_t flashLEDTaskHandle = nullptr;
TaskHandle_t provisioningBlinkTaskHandle = nullptr;
TaskHandle_t wifiMqttCheckTaskHandle = nullptr;
TaskHandle_t recoveryTaskHandle = nullptr;
volatile LEDMode currentLEDMode = LEDMODE_OFF;
DebugColor currentDebugColor = DEBUG_COLOR_YELLOW;
ProvisioningState currentProvisioningState = PROVISIONING_NONE;
String flashPattern = "";

void enterRecoveryMode(RecoveryType type);
void recoveryTask(void *parameter);
volatile bool inRecoveryMode = false;

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
    //xTaskCreatePinnedToCore(wifiMqttCheckTask,      "WiFi/MQTT Check", 4096, NULL, 1, &wifiMqttCheckTaskHandle,      0);
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

 void wifiMqttCheckTask(void *parameter) {
    static int wifiFailCount = 0;
    static int mqttFailCount = 0;
    static bool recoveryServerStarted = false;

    const int WIFI_FAIL_THRESHOLD = 8;
    const int MQTT_FAIL_THRESHOLD = 8;
    const unsigned long mqttRetryInterval = 10000;
    static unsigned long lastMqttAttempt = 0;
    static unsigned long firstWifiFailTime = 0;

    for (;;) {
        // 🚨 Skip if not in STATUS mode
        if (currentLEDMode != LEDMODE_STATUS) {
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }

        // ─── WiFi Check ─────────────────────────────
        if (!resetConfig && WiFi.status() != WL_CONNECTED) {
            if (wifiFailCount == 0) {
                firstWifiFailTime = millis();
            }

            wifiFailCount++;
            WiFi.setTxPower(WIFI_POWER_8_5dBm);

            logMessage(LOG_WARN, "[WiFiCheck] WiFi not connected. Fail count: " + String(wifiFailCount) + " / " + WIFI_FAIL_THRESHOLD);

            // ➤ If we've failed for over 15 minutes, start recovery
            if (!recoveryServerStarted && millis() - firstWifiFailTime >= 15 * 60 * 1000UL) {
                logMessage(LOG_WARN, "🚨 WiFi not connected for 15 mins → Starting Recovery Server (AP mode)");
                WiFi.disconnect(true, true);
                WiFi.mode(WIFI_AP);
                WiFi.softAP("SmartBoat_RECOVERY", nullptr);
                setLEDMode(LEDMODE_WAITING);

                SmartCore_WiFi::startRecoveryServer();
                recoveryServerStarted = true;

                // 💤 Wait 10 minutes then restart
                vTaskDelay(pdMS_TO_TICKS(10 * 60 * 1000UL));
                logMessage(LOG_WARN, "🕰️ 10 mins passed, rebooting to retry stored WiFi creds...");
                WiFi.disconnect(true, true);
                ESP.restart();
            }
        } else {
            if (wifiFailCount > 0) logMessage(LOG_INFO, "[WiFiCheck] WiFi connected. Resetting fail counter.");
            wifiFailCount = 0;
            firstWifiFailTime = 0;
        }

        // ─── MQTT Check ─────────────────────────────
        if (WiFi.status() == WL_CONNECTED && !mqttIsConnected) {
            mqttFailCount++;
            logMessage(LOG_WARN, "[MQTTCheck] MQTT not connected. Fail count: " + String(mqttFailCount) + " / " + MQTT_FAIL_THRESHOLD);

            unsigned long now = millis();
            if (now - lastMqttAttempt > mqttRetryInterval) {
                logMessage(LOG_INFO, "[MQTTCheck] Attempting regular MQTT reconnect...");
                SmartCore_MQTT::setupMQTTClient();
                lastMqttAttempt = now;
            }

            if (mqttFailCount >= MQTT_FAIL_THRESHOLD) {
                logMessage(LOG_WARN, "[MQTTCheck] MQTT fail threshold reached — ignoring further retries for now.");
                // Optionally trigger alert LED or retry after a longer interval
            }
        } else {
            if (mqttFailCount > 0) logMessage(LOG_INFO, "[MQTTCheck] MQTT connected. Resetting fail counter.");
            mqttFailCount = 0;
        }

        // ─── LED Status Update ──────────────────────
        if ((flashQueue && uxQueueMessagesWaiting(flashQueue) > 0) || currentLEDMode != LEDMODE_STATUS) {
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }

        if (xSemaphoreTake(ledMutex, portMAX_DELAY) == pdTRUE) {
            if (resetConfig) {
                setRGBColor(0, 255, 0);  // Green
            } else if (WiFi.status() != WL_CONNECTED) {
                setRGBColor(255, 0, 0);  // Red
            } else if (!mqttIsConnected) {
                setRGBColor(255, 60, 80);  // Hot Pink
            } else {
                setRGBColor(0, 0, 255);  // Blue
            }
            xSemaphoreGive(ledMutex);
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

    // ---- RECOVERY MODE ----
    void enterRecoveryMode(RecoveryType type) {
        logMessage(LOG_WARN, String("[Recovery] Entering recovery mode. Type: ") + (type == WIFI_RECOVERY ? "WiFi" : "MQTT"));
        inRecoveryMode = true;

        logMessage(LOG_INFO, "[Recovery] About to take LED mutex...");
        if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(3000)) == pdTRUE) {
            logMessage(LOG_INFO, "[Recovery] LED mutex acquired.");
                setRGBColor(200, 0, 180);      // Purple - recoverry mode
                xSemaphoreGive(ledMutex);
            logMessage(LOG_INFO, "[Recovery] LED mutex released.");
        } else {
            logMessage(LOG_ERROR, "[Recovery] LED mutex take TIMEOUT!!! Something is holding the mutex.");
        }

        logMessage(LOG_INFO, "[Recovery] About to suspend tasks...");
        if (!recoveryTaskHandle) {
            logMessage(LOG_INFO, "[Recovery] Creating recovery task...");
            xTaskCreatePinnedToCore(recoveryTask, "RecoveryTask", 4096, (void*)static_cast<uintptr_t>(type), 2, &recoveryTaskHandle, 1);
        }
        vTaskSuspend(flashLEDTaskHandle);
        vTaskSuspend(provisioningBlinkTaskHandle);
        if (xTaskGetCurrentTaskHandle() != wifiMqttCheckTaskHandle) {
            vTaskSuspend(wifiMqttCheckTaskHandle);
        } else {
            logMessage(LOG_INFO, "[Recovery] Not suspending self!");
        }

        logMessage(LOG_INFO, String("[Recovery] Current recoveryTaskHandle: ") + (uintptr_t)recoveryTaskHandle);

        // 2. Set LED to error code
        if (xSemaphoreTake(ledMutex, 1000 / portTICK_PERIOD_MS) == pdTRUE) {
            if (type == WIFI_RECOVERY) setRGBColor(255, 0, 0);      // Red
            else if (type == MQTT_RECOVERY) setRGBColor(255, 60, 80); // Hot Pink
            xSemaphoreGive(ledMutex);
        } else {
            logMessage(LOG_ERROR, "[Recovery] Could not take LED mutex! Mutex stuck or deadlocked.");
        }

        logMessage(LOG_WARN, String("[Recovery] Current recoveryTaskHandle: ") + (uintptr_t)recoveryTaskHandle);


        // 3. Start recovery task if not already running
        if (!recoveryTaskHandle) {
            logMessage(LOG_INFO, "[Recovery] Creating recovery task...");
            xTaskCreatePinnedToCore(recoveryTask, "RecoveryTask", 4096, (void*)static_cast<uintptr_t>(type), 2, &recoveryTaskHandle, 1);
        } else {
            logMessage(LOG_WARN, "[Recovery] Recovery task already running, skipping.");
        }
    }

    void recoveryTask(void *parameter) {
        RecoveryType type = static_cast<RecoveryType>(reinterpret_cast<uintptr_t>(parameter));
        int attempts = 0;
        const int maxAttempts = 10;
        bool recovered = false;

        logMessage(LOG_INFO, String("[RecoveryTask] Starting recovery. Type: ") + (type == WIFI_RECOVERY ? "WiFi" : "MQTT"));
        while (attempts < maxAttempts && !recovered) {
            attempts++;
            logMessage(LOG_INFO, String("[RecoveryTask] Attempt ") + attempts + " of " + maxAttempts);

            if (type == WIFI_RECOVERY) {
                logMessage(LOG_WARN, "[RecoveryTask] Resetting WiFi stack...");
                WiFi.disconnect(true);
                vTaskDelay(pdMS_TO_TICKS(1500));
                WiFi.mode(WIFI_OFF);
                vTaskDelay(pdMS_TO_TICKS(1500));
                WiFi.mode(WIFI_STA);
                vTaskDelay(pdMS_TO_TICKS(1500));

                // Read from EEPROM or config as needed
                String ssid = SmartCore_EEPROM::readStringFromEEPROM(WIFI_SSID_ADDR, 41);
                String pass = SmartCore_EEPROM::readStringFromEEPROM(WIFI_PASS_ADDR, 41);
                logMessage(LOG_INFO, String("[RecoveryTask] Trying to connect to SSID: ") + ssid);
                WiFi.begin(ssid.c_str(), pass.c_str());

                // Wait up to 15 seconds for connect
                for (int i = 0; i < 15; ++i) {
                    if (WiFi.status() == WL_CONNECTED) {
                        logMessage(LOG_INFO, "[RecoveryTask] WiFi connected!");
                        recovered = true;
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            } else if (type == MQTT_RECOVERY) {
                logMessage(LOG_WARN, "[RecoveryTask] Hard resetting MQTT client...");
                SmartCore_MQTT::hardResetClient(); // implement this!
                SmartCore_MQTT::setupMQTTClient();

                // Wait for connection
                for (int i = 0; i < 10; ++i) {
                    if (mqttIsConnected) {
                        logMessage(LOG_INFO, "[RecoveryTask] MQTT connected!");
                        recovered = true;
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            }

            // If not recovered, wait a minute before next attempt
            if (!recovered) {
                logMessage(LOG_WARN, "[RecoveryTask] Recovery attempt failed, waiting 1 min before retry...");
                vTaskDelay(pdMS_TO_TICKS(60000)); // 1 min
            }
        }

        if (recovered) {
            logMessage(LOG_INFO, "[RecoveryTask] Recovery successful, resuming normal operation.");
            // Resume tasks
            vTaskResume(flashLEDTaskHandle);
            vTaskResume(provisioningBlinkTaskHandle);
            vTaskResume(wifiMqttCheckTaskHandle);
            inRecoveryMode = false;
            recoveryTaskHandle = nullptr;
            vTaskDelete(NULL); // End recovery task
        } else {
            logMessage(LOG_WARN, "[RecoveryTask] Recovery failed. Entering safe mode.");
            SmartCore_System::enterSafeMode(); // Or call ESP.restart()
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

