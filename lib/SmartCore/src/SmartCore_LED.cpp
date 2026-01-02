#include "SmartCore_LED.h"
#include "SmartCore_Network.h"
#include "SmartCore_System.h"
#include "SmartCore_MQTT.h"
#include "SmartCore_Log.h"
#include "SmartCore_EEPROM.h"
#include "SmartCore_Wifi.h"
#include <WiFi.h>
#include <cstdint>

TaskHandle_t flashLEDTaskHandle = nullptr;
TaskHandle_t provisioningBlinkTaskHandle = nullptr;
TaskHandle_t wifiMqttCheckTaskHandle = nullptr;

namespace SmartCore_LED
{
    QueueHandle_t flashQueue;
    SemaphoreHandle_t ledMutex = nullptr;
    TaskHandle_t recoveryTaskHandle = nullptr;
    volatile LEDMode currentLEDMode = LEDMODE_OFF;
    DebugColor currentDebugColor = DEBUG_COLOR_YELLOW;
    ProvisioningState currentProvisioningState = PROVISIONING_NONE;
    String flashPattern = "";

    void enterRecoveryMode(RecoveryType type);
    void recoveryTask(void *parameter);
    volatile bool inRecoveryMode = false;

    void initSmartCoreLED()
    {
        pinMode(RGB_r, OUTPUT);
        pinMode(RGB_g, OUTPUT);
        pinMode(RGB_b, OUTPUT);

        ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_BIT);
        ledcSetup(LEDC_CHANNEL_1, LEDC_BASE_FREQ, LEDC_TIMER_BIT);
        ledcSetup(LEDC_CHANNEL_2, LEDC_BASE_FREQ, LEDC_TIMER_BIT);

        ledcAttachPin(RGB_r, LEDC_CHANNEL_0);
        ledcAttachPin(RGB_g, LEDC_CHANNEL_1);
        ledcAttachPin(RGB_b, LEDC_CHANNEL_2);

        setRGBColor(0, 255, 0); // Green on init

        ledMutex = xSemaphoreCreateMutex();
        if (!ledMutex)
            logMessage(LOG_INFO, "⚠️ Failed to create LED mutex");

        flashQueue = xQueueCreate(5, sizeof(String));
        if (!flashQueue)
            logMessage(LOG_INFO, "⚠️ Failed to create flash queue");

        xTaskCreatePinnedToCore(flashLEDTask, "Flash LED", 2048, NULL, 1, &flashLEDTaskHandle, 1);
        xTaskCreatePinnedToCore(provisioningBlinkTask, "Provision Blink", 2048, NULL, 1, &provisioningBlinkTaskHandle, 1);
        xTaskCreatePinnedToCore(statusLEDTask, "Status LED Task", 2048, NULL, 1, NULL, 1);
        // xTaskCreatePinnedToCore(wifiMqttCheckTask,      "WiFi/MQTT Check", 4096, NULL, 1, &wifiMqttCheckTaskHandle,      0);
    }

    void setRGBColor(uint8_t red, uint8_t green, uint8_t blue)
    {
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

    void triggerFlashPattern(const String &pattern, DebugColor color = DEBUG_COLOR_YELLOW)
    {
        if (!flashQueue)
        {
            logMessage(LOG_WARN, "⚠️ Flash queue not initialized.");
            return;
        }

        int available = uxQueueSpacesAvailable(flashQueue);
        logMessage(LOG_INFO, "🟨 Flash pattern trigger → pattern: [" + pattern + "], length: " + String(pattern.length()));
        logMessage(LOG_INFO, "🧮 Flash queue space: " + String(available));
        logMessage(LOG_INFO, "💡 Current LED mode: " + String(currentLEDMode));

        if (available > 0)
        {
            currentDebugColor = color;
            currentLEDMode = LEDMODE_DEBUG_PATTERN;

            // Create new pattern instance to avoid shared reference issues
            String *dynamicPattern = new String(pattern);
            BaseType_t result = xQueueSend(flashQueue, &dynamicPattern, 0); // Non-blocking

            if (result == pdTRUE)
            {
                logMessage(LOG_INFO, "🟡 Pattern queued successfully.");
            }
            else
            {
                logMessage(LOG_WARN, "⚠️ Pattern queue send failed.");
                delete dynamicPattern;
            }
        }
        else
        {
            logMessage(LOG_WARN, "⚠️ Flash queue full. Pattern not queued.");
        }
    }

    void flashLEDTask(void *parameter)
    {
        String *patternPtr = nullptr;

        for (;;)
        {
            if (xQueueReceive(flashQueue, &patternPtr, portMAX_DELAY) == pdTRUE)
            {
                String pattern = *patternPtr;
                delete patternPtr; // Clean up
                logMessage(LOG_INFO, "⚡ FlashLEDTask running pattern: [" + pattern + "]");

                if (xSemaphoreTake(ledMutex, portMAX_DELAY) == pdTRUE)
                {
                    setRGBColor(0, 255, 255); // Cyan blip
                    vTaskDelay(pdMS_TO_TICKS(150));
                    turnOffRGBLEDs();
                    vTaskDelay(pdMS_TO_TICKS(150));

                    for (char symbol : pattern)
                    {
                        Serial.printf("  → Flash symbol: %c\n", symbol);

                        switch (symbol)
                        {
                        case '.':
                        case '-':
                            switch (currentDebugColor)
                            {
                            case DEBUG_COLOR_RED:
                                setRGBColor(255, 0, 0);
                                break;
                            case DEBUG_COLOR_CYAN:
                                setRGBColor(0, 255, 255);
                                break;
                            case DEBUG_COLOR_MAGENTA:
                                setRGBColor(255, 0, 255);
                                break;
                            default:
                                setRGBColor(255, 255, 0);
                                break;
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
                }
                else
                {
                    logMessage(LOG_WARN, "❌ Could not take LED mutex during flash.");
                }

                logMessage(LOG_INFO, "✅ Flash pattern complete. Returning to STATUS mode.");
                currentLEDMode = LEDMODE_STATUS;
            }
            else
            {
                logMessage(LOG_WARN, "⛔ FlashLEDTask failed to receive pattern from queue.");
            }
        }
    }

    void statusLEDTask(void *parameter)
    {
        static bool toggle = false;

        for (;;)
        {
            // Only run when in STATUS mode
            if (currentLEDMode != LEDMODE_STATUS)
            {
                vTaskDelay(pdMS_TO_TICKS(200));
                continue;
            }

            // ---- Non-blocking LED mutex check ----
            if (xSemaphoreTake(ledMutex, 0) != pdTRUE)
            {
                // Another LED task is running (flash pattern, provisioning, recovery)
                vTaskDelay(pdMS_TO_TICKS(200));
                continue;
            }

            // ========== LED STATUS LOGIC ==========
            if (resetConfig)
            {
                setRGBColor(0, 255, 0); // GREEN
            }
            else if (!SmartCore_WiFi::hasStoredCreds())
            {
                setRGBColor(255, 0, 0); // SOLID RED
            }
            else if (WiFi.status() != WL_CONNECTED)
            {
                toggle ? setRGBColor(255, 0, 0) : setRGBColor(0, 0, 0);
                toggle = !toggle;
            }
            else if (mqttPriorityCount == 0 ||
                     SmartCore_MQTT::currentBrokerIP.length() == 0 ||
                     SmartCore_MQTT::currentBrokerIP == "0.0.0.0" ||
                     SmartCore_MQTT::currentBrokerIP == "127.0.0.1")
            {
                setRGBColor(255, 50, 0); // ORANGE/AMBER = Not Provisioned
            }
            else if (!mqttIsConnected)
            {
                setRGBColor(255, 0, 90); // HOT PINK
            }
            else if (SmartCore_System::bootSafeMode)
            {
                // 🧯 SAFE MODE — online but degraded
                setRGBColor(160, 0, 100);// PURPLE (matches app)
            }
            else
            {
                setRGBColor(0, 0, 255); // BLUE
            }

            xSemaphoreGive(ledMutex);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }

    // ======================================================================================
    //  WIFI + MQTT HEALTH MONITOR TASK
    // ======================================================================================
    //
    //  This FreeRTOS task runs continuously on its own core and is responsible for:
    //      • Detecting WiFi loss and performing exponential-backoff reconnects.
    //      • Detecting MQTT loss and attempting reconnects.
    //      • Performing MQTT hard resets after extended failures.
    //      • Triggering the cluster-aware failover engine after repeated failures.
    //      • Respecting SmartBox failover locks (failoverInProgress).
    //      • Updating LED behaviour indirectly via currentLEDMode (status gating).
    //
    // --------------------------------------------------------------------------------------
    //  EXECUTION RULES
    // --------------------------------------------------------------------------------------
    //  • Runs once per second.
    //
    //  • If LEDMODE is NOT STATUS
    //        → task is “soft paused” (used during provisioning/failover).
    //
    //  • WiFi recovery uses strict exponential backoff but NEVER enters AP mode.
    //        NO auto-AP. NO fallback webserver. This is intentional.
    //
    //  • MQTT recovery happens only when WiFi is connected.
    //
    //  • If failoverInProgress == true
    //        → MQTT reconnect attempts are temporarily ignored.
    //          (failover engine owns the state)
    //
    // --------------------------------------------------------------------------------------
    //  WIFI LOGIC
    // --------------------------------------------------------------------------------------
    //  - Upon WiFi disconnect:
    //       wifiFailCount++
    //       Attempt reconnect only when the exponential timer fires.
    //       wifiBackoff doubles each time, capped at 5 minutes.
    //       NO AP MODE, NO RESETTING NETWORK STACK.
    //
    //  - Upon WiFi recovery:
    //       wifiFailCount resets.
    //       backoff returns to 2 seconds.
    //
    // --------------------------------------------------------------------------------------
    //  MQTT LOGIC
    // --------------------------------------------------------------------------------------
    //
    // 1) mqttIsConnected == false AND WiFi == connected:
    //        mqttFailCount++
    //
    //        Every mqttBackoff interval:
    //            → Attempt reconnect to *currentBrokerIP/currentBrokerPort*.
    //
    //        mqttBackoff doubles each time (max 5 minutes).
    //
    // 2) mqttFailCount == 8
    //        → Hard-reset MQTT client (destroy client, recreate from scratch).
    //
    // 3) mqttFailCount > 10
    //        → Trigger SmartCore_MQTT::handleMQTTFailover()
    //        → mqttFailCount is reset to 0 after calling failover handler.
    //
    // --------------------------------------------------------------------------------------
    //  FAILOVER SAFETY
    // --------------------------------------------------------------------------------------
    //  The task checks:
    //
    //        if (SmartCore_MQTT::failoverInProgress)
    //            → skip all MQTT reconnect attempts
    //
    //  This prevents the reconnect thread from fighting with the failover engine.
    //
    // --------------------------------------------------------------------------------------
    //  LED STATUS GATING
    // --------------------------------------------------------------------------------------
    //  The task only runs when:
    //
    //        currentLEDMode == LEDMODE_STATUS
    //
    //  Any other LED mode (WAITING, DEBUG, OFF) pauses monitoring.
    //  This allows provisioning, failover waits, or device overrides to run cleanly.
    //
    // --------------------------------------------------------------------------------------
    //  SUMMARY OF BEHAVIOR
    // --------------------------------------------------------------------------------------
    //
    //  • WiFi DOWN  → exponential reconnect (no AP).
    //  • MQTT DOWN → reconnect attempts → hard reset → failover.
    //  • Failover pauses reconnect attempts until Pi ACKs.
    //  • On successful connection (handled inside onMqttConnect()):
    //        → Priority index is persisted
    //        → Normal monitoring resumes.
    //
    //  This task forms the "self-healing" backbone of SmartBoat networking.
    // ======================================================================================

    void wifiMqttCheckTask(void *parameter)
    {
        static int wifiFailCount = 0;
        static int mqttFailCount = 0;

        static uint32_t wifiBackoff = 2000; // 2 sec
        static uint32_t mqttBackoff = 5000; // 5 sec

        const uint32_t WIFI_BACKOFF_MAX = 5 * 60 * 1000UL; // 5 min
        const uint32_t MQTT_BACKOFF_MAX = 5 * 60 * 1000UL; // 5 min

        uint32_t lastWiFiAttempt = 0;
        uint32_t lastMQTTAttempt = 0;

        for (;;)
        {
            if (currentLEDMode != LEDMODE_STATUS)
            {
                vTaskDelay(pdMS_TO_TICKS(200));
                continue;
            }

            uint32_t now = millis();

            // ------------------------------------------------------
            // Runtime stability hint (NON-AUTHORITATIVE)
            // ------------------------------------------------------
            static bool stabilityHintSent = false;

            if (!stabilityHintSent &&
                millis() > RUNTIME_STABILITY_WINDOW_MS &&
                WiFi.status() == WL_CONNECTED)
            {
                SmartCore_System::runtimeStableConfirmed = true;
                stabilityHintSent = true;

                logMessage(LOG_INFO,
                    "🟢 Runtime stability hint raised (network alive)");
            }

            // ======================================================
            //  WIFI: Exponential Backoff Only
            // ======================================================
            if (!resetConfig && WiFi.status() != WL_CONNECTED)
            {
                wifiFailCount++;

                logMessage(LOG_WARN,
                           "[WiFiCheck] WiFi DOWN. Count=" + String(wifiFailCount) +
                               " | Backoff=" + String(wifiBackoff / 1000) + "s");

                if (now - lastWiFiAttempt >= wifiBackoff)
                {
                    lastWiFiAttempt = now;
                    logMessage(LOG_INFO, "[WiFiCheck] Reconnecting WiFi…");
                    WiFi.reconnect();

                    wifiBackoff = min<uint32_t>(wifiBackoff * 2, WIFI_BACKOFF_MAX);
                }
            }
            else
            {
                if (wifiFailCount > 0)
                    logMessage(LOG_INFO, "[WiFiCheck] WiFi recovered.");

                wifiFailCount = 0;
                wifiBackoff = 2000;
            }
            // ======================================================
            //  MQTT: Reconnect + Hard Reset + Failover Callback
            // ======================================================
            if (WiFi.status() == WL_CONNECTED && !mqttIsConnected)
            {
                // Before attempting MQTT connect/reset/failover check details are present
                if (mqttPriorityCount == 0 ||
                    SmartCore_MQTT::currentBrokerIP.length() == 0 ||
                    SmartCore_MQTT::currentBrokerIP == "0.0.0.0" ||
                    SmartCore_MQTT::currentBrokerIP == "127.0.0.1")
                {
                    logMessage(LOG_WARN, "[MQTTCheck] MQTT not started: No valid broker provisioned");
                    // Optional: set a “not provisioned” LED state here if you want instant feedback
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    continue;
                }
                // If failover is running → DO NOT reconnect
                if (SmartCore_MQTT::failoverInProgress)
                {
                    vTaskDelay(pdMS_TO_TICKS(200));
                    continue;
                }

                mqttFailCount++;

                logMessage(LOG_WARN,
                           "[MQTTCheck] MQTT DOWN. Count=" + String(mqttFailCount) +
                               " | Backoff=" + String(mqttBackoff / 1000) + "s");

                if (now - lastMQTTAttempt >= mqttBackoff)
                {
                    lastMQTTAttempt = now;
                    logMessage(LOG_INFO, "[MQTTCheck] Attempting MQTT reconnect…");

                    SmartCore_MQTT::setupMQTTClient(
                        SmartCore_MQTT::currentBrokerIP,
                        SmartCore_MQTT::currentBrokerPort);

                    mqttBackoff = min<uint32_t>(mqttBackoff * 2, MQTT_BACKOFF_MAX);
                }

                // Hard reset MQTT client
                if (mqttFailCount == 8)
                {
                    logMessage(LOG_WARN, "[MQTTCheck] Hard-resetting MQTT client");
                    SmartCore_MQTT::hardResetClient();
                    vTaskDelay(pdMS_TO_TICKS(300));

                    SmartCore_MQTT::setupMQTTClient(
                        SmartCore_MQTT::currentBrokerIP,
                        SmartCore_MQTT::currentBrokerPort);
                }

                // FAILOVER CALLBACK — only after repeated failures
                if (mqttFailCount > 10)
                {
                    logMessage(LOG_WARN, "[MQTTCheck] Triggering FAILOVER HANDLER");
                    SmartCore_MQTT::handleMQTTFailover();
                    mqttFailCount = 0;
                }
            }
            else
            {
                if (mqttFailCount > 0)
                    logMessage(LOG_INFO, "[MQTTCheck] MQTT recovered.");

                mqttFailCount = 0;
                mqttBackoff = 5000;
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // ---- RECOVERY MODE ----
    void enterRecoveryMode(RecoveryType type)
    {
        logMessage(LOG_WARN, String("[Recovery] Entering recovery mode. Type: ") + (type == WIFI_RECOVERY ? "WiFi" : "MQTT"));
        inRecoveryMode = true;

        logMessage(LOG_INFO, "[Recovery] About to take LED mutex...");
        if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(3000)) == pdTRUE)
        {
            logMessage(LOG_INFO, "[Recovery] LED mutex acquired.");
            setRGBColor(200, 0, 180); // Purple - recoverry mode
            xSemaphoreGive(ledMutex);
            logMessage(LOG_INFO, "[Recovery] LED mutex released.");
        }
        else
        {
            logMessage(LOG_ERROR, "[Recovery] LED mutex take TIMEOUT!!! Something is holding the mutex.");
        }

        logMessage(LOG_INFO, "[Recovery] About to suspend tasks...");
        if (!recoveryTaskHandle)
        {
            logMessage(LOG_INFO, "[Recovery] Creating recovery task...");
            xTaskCreatePinnedToCore(recoveryTask, "RecoveryTask", 4096, (void *)static_cast<uintptr_t>(type), 2, &recoveryTaskHandle, 1);
        }
        vTaskSuspend(flashLEDTaskHandle);
        vTaskSuspend(provisioningBlinkTaskHandle);
        if (xTaskGetCurrentTaskHandle() != wifiMqttCheckTaskHandle)
        {
            vTaskSuspend(wifiMqttCheckTaskHandle);
        }
        else
        {
            logMessage(LOG_INFO, "[Recovery] Not suspending self!");
        }

        logMessage(LOG_INFO, String("[Recovery] Current recoveryTaskHandle: ") + (uintptr_t)recoveryTaskHandle);

        // 2. Set LED to error code
        if (xSemaphoreTake(ledMutex, 1000 / portTICK_PERIOD_MS) == pdTRUE)
        {
            if (type == WIFI_RECOVERY)
                setRGBColor(255, 0, 0); // Red
            else if (type == MQTT_RECOVERY)
                setRGBColor(255, 60, 80); // Hot Pink
            xSemaphoreGive(ledMutex);
        }
        else
        {
            logMessage(LOG_ERROR, "[Recovery] Could not take LED mutex! Mutex stuck or deadlocked.");
        }

        logMessage(LOG_WARN, String("[Recovery] Current recoveryTaskHandle: ") + (uintptr_t)recoveryTaskHandle);

        // 3. Start recovery task if not already running
        if (!recoveryTaskHandle)
        {
            logMessage(LOG_INFO, "[Recovery] Creating recovery task...");
            xTaskCreatePinnedToCore(recoveryTask, "RecoveryTask", 4096, (void *)static_cast<uintptr_t>(type), 2, &recoveryTaskHandle, 1);
        }
        else
        {
            logMessage(LOG_WARN, "[Recovery] Recovery task already running, skipping.");
        }
    }

    void provisioningBlinkTask(void *parameter)
    {
        for (;;)
        {
            if (currentLEDMode == LEDMODE_WAITING)
            {
                if (xSemaphoreTake(ledMutex, portMAX_DELAY) == pdTRUE)
                {
                    if (currentProvisioningState == PROVISIONING_PORTAL)
                    {
                        logMessage(LOG_INFO, "💡 Blinking Cyan: WiFiManager portal open");
                        setRGBColor(0, 255, 255); // Cyan
                    }
                    else if (currentProvisioningState == PROVISIONING_SMARTCONNECT)
                    {
                        logMessage(LOG_INFO, "💡 Blinking Red: Waiting for SmartConnect");
                        setRGBColor(255, 0, 0); // Red
                    }

                    vTaskDelay(pdMS_TO_TICKS(400)); // LED on
                    setRGBColor(0, 0, 0);           // LED off
                    vTaskDelay(pdMS_TO_TICKS(400)); // LED off delay

                    xSemaphoreGive(ledMutex);
                }
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(500)); // Sleep quietly when not in WAITING mode
            }
        }
    }

    void setLEDMode(LEDMode mode)
    {
        if (mode != currentLEDMode)
        {
            currentLEDMode = mode;
            logMessage(LOG_INFO, "🔁 LED Mode changed to: " + String(mode));
        }
    }

    void setFailedWiFiCredsTask(void *parameter)
    {
        int blinkDuration = 5000; // Total blink time in milliseconds
        int blinkInterval = 500;  // Time between blinks in milliseconds (0.5 seconds on/off)
        unsigned long startTime = millis();

        // Lock the LED mutex before starting the LED task
        if (xSemaphoreTake(ledMutex, portMAX_DELAY) == pdTRUE)
        {
            while (millis() - startTime < blinkDuration)
            {
                // Turn on the red LED
                setRGBColor(255, 0, 0);                             // Red color
                vTaskDelay(blinkInterval / 2 / portTICK_PERIOD_MS); // LED on for half the interval

                // Turn off the LED
                setRGBColor(0, 0, 0);                               // LED off
                vTaskDelay(blinkInterval / 2 / portTICK_PERIOD_MS); // LED off for half the interval
            }
            // Release the LED mutex
            xSemaphoreGive(ledMutex);
        }

        vTaskDelete(NULL); // End the task when done
    }

    void turnOffRGBLEDs()
    {
        logMessage(LOG_INFO, "Turning off LEDs via PWM");
        setRGBColor(0, 0, 0); // Set to black/off via PWM
    }
}
