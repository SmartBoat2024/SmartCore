#include "SmartCore_config.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include "SmartCore_System.h"
#include "SmartCore_LED.h"
#include "SmartCore_I2C.h"
#include "SmartCore_Network.h"
#include "SmartCore_EEPROM.h"
#include "SmartCore_OTA.h"
#include "SmartCore_Log.h"
#include "SmartCore_WiFi.h"
#include "SmartCore_MQTT.h"
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <otadrive_esp.h>
#include "SmartCore_SmartNet.h"
#include "SmartCore_OTA.h"
#include "SmartCore_MCP.h"
#include "SmartCore_OTA.h"
#include "config.h"
#include "module_reset.h"


namespace SmartCore_System
{

    TaskHandle_t resetButtonTaskHandle = NULL;

    const unsigned long holdTime = 5000;
    unsigned long buttonPressStart = 0;
    bool buttonPressed = false;
    volatile bool inOTAUpdate = false;

    static AsyncWebSocket safeWs("/ws");

    AsyncEventSource events("/events");

    volatile bool bootSafeMode = false;
    volatile BootFaultReason bootFault = BOOT_FAULT_NONE;

    volatile bool runtimeStable = false;
    volatile uint32_t runtimeStableSince = 0;

    static SmartCoreSettings _settings = {
        .serialNumber = "UNKNOWN",
        .moduleName = "Generic SmartCore",
        .apSSID = "SmartModule"};

    void markRuntimeStable()
    {
        if (!runtimeStable)
        {
            runtimeStable = true;
            runtimeStableSince = millis();

            logMessage(LOG_INFO,
                "🧠 Runtime marked STABLE — starting stability timer");
        }
    }

    void setModuleSettings(const SmartCoreSettings &settings)
    {
        _settings = settings;

        logMessage(LOG_INFO, String("📦 Module settings updated → serial: ") +
                                 (settings.serialNumber ? settings.serialNumber : "nullptr") +
                                 ", name: " +
                                 (settings.moduleName ? settings.moduleName : "nullptr"));

        // 🧠 SYNC TO GLOBAL BUFFERS
        strncpy(serialNumber, settings.serialNumber, sizeof(serialNumber) - 1);
        serialNumber[sizeof(serialNumber) - 1] = '\0';

        // 💾 WRITE TO EEPROM
        SmartCore_EEPROM::writeStringToEEPROM(SN_ADDR, String(settings.serialNumber), 41);
        SmartCore_EEPROM::writeModuleNameToEEPROM(settings.moduleName);
    }

    const SmartCoreSettings &getModuleSettings()
    {
        return _settings;
    }

    void preinit()
{
    Serial.begin(115200);
    delay(2000);

    if (!EEPROM.begin(1024))
    {
        logMessage(LOG_ERROR, "❌ EEPROM.begin failed!");
        // EEPROM failure is unrecoverable → halt
        while (true) { vTaskDelay(1000); }
    }

    // ─────────────────────────────────────────────
    // DEBUG: MQTT priority dump (safe)
    // ─────────────────────────────────────────────
    Serial.println("🐞 POST-BOOT: MQTT PRIORITY CHECK");
    int bootCount = SmartCore_EEPROM::readIntFromEEPROM(MQTT_PRIORITY_COUNT_ADDR);
    Serial.printf("Count from EEPROM: %d\n", bootCount);
    for (int i = 0; i < bootCount; i++)
    {
        String slot =
            SmartCore_EEPROM::readStringFromEEPROM(
                MQTT_PRIORITY_LIST_ADDR + (i * 17), 17);
        Serial.printf("   [%d] = '%s'\n", i, slot.c_str());
    }

    // ─────────────────────────────────────────────
    // Crash counters (OBSERVATION ONLY)
    // ─────────────────────────────────────────────
    uint8_t crashCounter = EEPROM.read(CRASH_COUNTER_ADDR) + 1;
    EEPROM.write(CRASH_COUNTER_ADDR, crashCounter);

    uint8_t runtimeCrashCounter =
        EEPROM.read(RUNTIME_CRASH_COUNTER_ADDR) + 1;
    EEPROM.write(RUNTIME_CRASH_COUNTER_ADDR, runtimeCrashCounter);

    EEPROM.commit();

    logMessage(LOG_WARN, "🧠 Boot crash counter: " + String(crashCounter));
    logMessage(LOG_WARN, "🧠 Runtime crash counter: " + String(runtimeCrashCounter));

    // ─────────────────────────────────────────────
    // Crash threshold handling (NO RESETS HERE)
    // ─────────────────────────────────────────────
    if (crashCounter >= CRASH_LIMIT_SAFE ||
        runtimeCrashCounter >= CRASH_LIMIT_SAFE)
    {
        bootSafeMode = true;
        bootFault = BOOT_FAULT_CRASH_LIMIT;

        logMessage(LOG_ERROR,
            "🚨 Crash limit exceeded → SAFE BOOT (network only)");
    }

    // ─────────────────────────────────────────────
    // OTA metadata (safe)
    // ─────────────────────────────────────────────
    Serial.printf("🐛 Firmware version: %s\n", FW_VER);
    OTADRIVE.setInfo(APIKEY, FW_VER);
    OTADRIVE.onUpdateFirmwareProgress(SmartCore_OTA::onUpdateProgress);

    // Read reset button intent (user-driven only)
    resetConfig = SmartCore_EEPROM::readResetConfigFlag();
}

  void init()
{
    bool fsOk = LittleFS.begin();

    if (!fsOk)
    {
        logMessage(LOG_ERROR, "❌ LittleFS mount failed → SAFE BOOT");
        bootSafeMode = true;
        bootFault = BOOT_FAULT_LITTLEFS;
    }
    else
    {
        logMessage(LOG_WARN, "✅ LittleFS mounted successfully");
    }

    // ─────────────────────────────────────────────
    // Core hardware init (always safe)
    // ─────────────────────────────────────────────
    SmartCore_LED::initSmartCoreLED();
    SmartCore_I2C::init();
    SmartCore_MCP::init();

    xTaskCreatePinnedToCore(
        checkresetButtonTask,
        "Check Reset Button",
        4096, NULL, 1,
        &resetButtonTaskHandle,
        0
    );

    // ─────────────────────────────────────────────
    // SmartNet (safe even in recovery)
    // ─────────────────────────────────────────────
    xTaskCreatePinnedToCore(
        SmartCore_SmartNet::smartNetTask,
        "SmartNet_RX_Task",
        4096, NULL, 1,
        &SmartCore_SmartNet::smartNetTaskHandle,
        1
    );

    // ─────────────────────────────────────────────
    // Load MQTT priority and serialNumber if in bootsafemode
    // ─────────────────────────────────────────────
    // EEPROM is SAFE in recovery
    loadMqttPriorityList();

    if (bootSafeMode)
    {
        String sn = SmartCore_EEPROM::readStringFromEEPROM(SN_ADDR, 41);

        if (sn.length() > 0)
        {
            strncpy(serialNumber, sn.c_str(), sizeof(serialNumber) - 1);
            serialNumber[sizeof(serialNumber) - 1] = '\0';
        }
        else
        {
            strncpy(serialNumber, "UNKNOWN", sizeof(serialNumber) - 1);
            serialNumber[sizeof(serialNumber) - 1] = '\0';
        }

        logMessage(LOG_INFO,
            "🆔 SAFE MODE serial loaded: " + String(serialNumber));

        logMessage(LOG_WARN,
            "⚠️ SAFE MODE — module tasks skipped, network allowed");
    }

    // ─────────────────────────────────────────────
    // WiFi + MQTT are ALWAYS allowed
    // ─────────────────────────────────────────────
    SmartCore_WiFi::startWiFiProvisionTask();
}


    void getModuleConfig()
    {
        logMessage(LOG_INFO, "Inside getModuleConfig");
        // Read serial number
        strncpy(serialNumber, SmartCore_EEPROM::readSerialNumberFromEEPROM(), sizeof(serialNumber) - 1);
        serialNumber[sizeof(serialNumber) - 1] = '\0'; // Ensure null-termination
        logMessage(LOG_INFO, "get config --- serialnumber: " + String(serialNumber));

        location = SmartCore_EEPROM::readLocationFromEEPROM();

        // Read first WiFi connect flag
        firstWiFiConnect = SmartCore_EEPROM::readFirstWiFiConnectFlag();
        logMessage(LOG_INFO, "📡 firstWiFiConnect: " + String(firstWiFiConnect));

        // Read serial number assigned flag
        serialNumberAssigned = SmartCore_EEPROM::readSerialNumberAssignedFlag();
        logMessage(LOG_INFO, "🧠 serialNumberAssigned: " + String(serialNumberAssigned));

        SmartCore_OTA::isUpgradeAvailable = SmartCore_EEPROM::loadUpgradeFlag();

        moduleName = SmartCore_EEPROM::readModuleNameFromEEPROM();

        getModuleSpecificConfig(); // call module specific config here
    }

    void createAppTasks()
    {
        // xTaskCreatePinnedToCore(otaTask, "OTA Task", 16384, NULL, 2, &otaTaskHandle, 0);
        // xTaskCreatePinnedToCore(smartNetTask, "SmartNet_RX_Task", 4096, NULL, 1, &smartNetTaskHandle, 1);
    }

    void checkresetButtonTask(void *parameter)
    {
        for (;;)
        {
            bool currentButtonState = digitalRead(buttonPin) == LOW;

            if (currentButtonState)
            {
                if (!buttonPressed)
                {
                    buttonPressStart = millis();
                    buttonPressed = true;
                }
                else
                {
                    if (millis() - buttonPressStart >= holdTime)
                    {
                        logMessage(LOG_INFO, "🟢 Reset module triggered — launching reset worker...");

                        // Clear handle BEFORE deleting self
                        resetButtonTaskHandle = NULL;

                        // Spawn reset worker task
                        xTaskCreatePinnedToCore(resetWorkerTask, "ResetWorker", 4096, NULL, 1, NULL, 0);

                        // Kill this task
                        vTaskDelete(NULL);
                    }
                }
            }
            else
            {
                buttonPressed = false;
            }

            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    void resetWorkerTask(void *param)
    {
        Serial.println("🧹 Suspending other tasks...");
        TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();

        struct TaskEntry
        {
            const char *name;
            TaskHandle_t handle;
        };

        TaskEntry tasks[] = {
            {"metricsTaskHandle", SmartCore_MQTT::metricsTaskHandle},
            {"timeSyncTaskHandle", SmartCore_MQTT::timeSyncTaskHandle},
            {"flashLEDTaskHandle", flashLEDTaskHandle},
            //{ "provisioningBlinkTaskHandle", provisioningBlinkTaskHandle }
            // Exclude OTA / WiFi tasks that could crash during suspend
        };

        for (const auto &task : tasks)
        {
            Serial.printf("[DEBUG] Suspending: %s (%p)...\n", task.name, task.handle);
            if (task.handle && task.handle != currentTask)
            {
                vTaskSuspend(task.handle);
                Serial.printf("   ✅ %s suspended\n", task.name);
            }
        }

        // ✅ Module-specific suspends (if any)
        suspendModuleTasksDuringReset();

        Serial.println("✅ All safe tasks suspended.");

        // 🔧 Now it's safe to reset parameters
        SmartCore_EEPROM::resetParameters();

        SmartCore_LED::setRGBColor(0, 255, 0); // 🟢 Feedback
        vTaskDelay(pdMS_TO_TICKS(200));

        Serial.println("♻️ Restarting...");
        esp_restart();
    }

    void clearCrashCounter(CrashCounterType type)
    {
        bool changed = false;

        if (type == CRASH_COUNTER_BOOT || type == CRASH_COUNTER_ALL)
        {
            if (EEPROM.read(CRASH_COUNTER_ADDR) != 0)
            {
                EEPROM.write(CRASH_COUNTER_ADDR, 0);
                logMessage(LOG_INFO, "✅ Boot crash counter reset to 0.");
                changed = true;
            }
        }

        if (type == CRASH_COUNTER_RUNTIME || type == CRASH_COUNTER_ALL)
        {
            if (EEPROM.read(RUNTIME_CRASH_COUNTER_ADDR) != 0)
            {
                EEPROM.write(RUNTIME_CRASH_COUNTER_ADDR, 0);
                logMessage(LOG_INFO, "✅ Runtime crash counter reset to 0.");
                changed = true;
            }
        }

        if (changed)
        {
            EEPROM.commit();
        }
        else
        {
            logMessage(LOG_INFO, "ℹ️ Crash counters already at 0. No changes written.");
        }
    }

    void enterSafeMode()
    {
        logMessage(LOG_ERROR, "🧯 Entering SAFE MODE — minimal services only.");
        SmartCore_LED::initSmartCoreLED();
        SmartCore_LED::setRGBColor(255, 255, 0);

        // WiFi AP setup for recovery
        WiFi.disconnect(true, false);
        delay(100);
        WiFi.mode(WIFI_AP);
        String apSsid = String(SmartCore_System::getModuleSettings().apSSID) + "_RECOVERY";
        WiFi.softAP(apSsid.c_str(), nullptr);
        logMessage(LOG_INFO, "🛟 Safe Mode AP SSID: " + apSsid);

        // Start Web Server
        static AsyncWebServer safeServer(81);

        safeWs.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                          AwsEventType type, void *arg, uint8_t *data, size_t len)
                       {
        if (type == WS_EVT_CONNECT) {
        Serial.println("🌐 Recovery WS client connected");
        } else if (type == WS_EVT_DISCONNECT) {
        Serial.println("👋 Recovery WS client disconnected");
        } else if (type == WS_EVT_ERROR) {
        Serial.println("❌ WS error occurred");
        } else if (type == WS_EVT_DATA) {
        Serial.println("💬 WS data received (ignored in Safe Mode)");
        } });

        safeServer.addHandler(&safeWs);

        safeServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                      {
            Serial.println("on recovery portal homepage");
            String html = R"rawliteral(
            <!DOCTYPE html>
            <html>
            <head>
                <title>SmartCore Recovery Portal</title>
                <meta charset="UTF-8">
                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                <style>
                    body { font-family: sans-serif; padding: 2em; background: #f5f5f5; text-align: center; }
                    h2 { color: #c0392b; }
                    button {
                        font-size: 1.2em;
                        margin: 1em;
                        padding: 1em 2em;
                        border: none;
                        border-radius: 10px;
                        background-color: #2980b9;
                        color: white;
                        cursor: pointer;
                    }
                    button:hover { background-color: #3498db; }
                    progress {
                        width: 80%; height: 30px;
                        margin-top: 20px;
                    }
                </style>
            </head>
            <body>
                <h2>🛠️ Safe Mode</h2>
                <p>The module entered Safe Mode after repeated crashes.</p>
                <button onclick="fetch('/retry').then(()=>alert('🔁 Retrying boot...'))">🔁 Try Normal Boot</button><br>
                <button onclick="fetch('/clear').then(()=>alert('🧹 EEPROM Cleared. Rebooting...'))">🧹 Clear EEPROM</button><br>
                <button onclick="fetch('/update')">📦 Firmware Update</button><br>   
                <button onclick="fetch('/wifi').then(()=>alert('📡 Restart SmartCOnnect...'))">📡 Reconfigure WiFi</button><br><br>

                <progress id="otaProgress" max="100" value="0"></progress>
                <p id="progressText">Waiting for OTA update...</p>

                <script>
                    const socket = new WebSocket("ws://" + location.hostname + ":81/ws");

                    socket.onopen = () => {
                        console.log("✅ WebSocket connected to OTA");
                    };

                    socket.onerror = (e) => {
                        console.error("❌ WebSocket error", e);
                    };

                    socket.onmessage = function (event) {
                        console.log("📨 WS Message Raw:", event.data);
                        try {
                            const data = JSON.parse(event.data);
                            console.log("📨 WS Message Parsed:", data);
                            if (typeof data.progress !== 'undefined') {
                                const progressEl = document.getElementById("otaProgress");
                                const textEl = document.getElementById("progressText");

                                progressEl.value = data.progress;
                                textEl.innerText = `Progress: ${data.progress}%`;

                                if (data.progress >= 100) {
                                    textEl.innerText = "✅ Update complete. Rebooting...";
                                }

                                console.log("📨 WS Message:", data);
                            }
                        } catch (err) {
                            console.error("❌ Failed to parse WS message:", event.data, err);
                        }
                    };
                    socket.onclose = () => console.warn("⚠️ WebSocket closed");
                </script>
            </body>
            </html>
            )rawliteral";
            request->send(200, "text/html", html); });

        safeServer.on("/retry", HTTP_GET, [](AsyncWebServerRequest *request)
                      {
            logMessage(LOG_WARN, "🧹 Safe Mode: Clearing crash counters...");
            EEPROM.write(CRASH_COUNTER_ADDR, 0);
            EEPROM.write(RUNTIME_CRASH_COUNTER_ADDR, 0);
            EEPROM.commit();
            request->send(200, "text/plain", "Rebooting...");
            delay(1000);
            ESP.restart(); });

        safeServer.on("/clear", HTTP_GET, [](AsyncWebServerRequest *request)
                      {
            logMessage(LOG_WARN, "🧹 Safe Mode: Clearing EEPROM and reset counters...");
            SmartCore_EEPROM::resetParameters();
            EEPROM.write(CRASH_COUNTER_ADDR, 0);
            EEPROM.write(RUNTIME_CRASH_COUNTER_ADDR, 0);
            if (!EEPROM.commit()) {
                logMessage(LOG_ERROR, "❌ EEPROM commit failed!");
                request->send(500, "text/plain", "EEPROM commit failed!");
                return;
            }
            request->send(200, "text/plain", "EEPROM cleared. Rebooting...");
            delay(1000);
            ESP.restart(); });

        safeServer.on("/update", HTTP_GET, [](AsyncWebServerRequest *request)
                      {
            logMessage(LOG_INFO, "📦 OTA update triggered via recovery UI");
            request->send(200, "text/plain", "Starting OTA task...");
        
            // Give time for WebSocket to connect before OTA starts
            inOTAUpdate = true;
            delay(2000);
        
            SmartCore_OTA::shouldUpdateFirmware = true;
            xTaskCreatePinnedToCore(SmartCore_OTA::otaTask, "OTAUpdate", 8192, NULL, 1, NULL, 0); });

        safeServer.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request)
                      {
            request->send(200, "text/plain", "Reset Wifi details and restart SmartConnect...");
            WiFi.disconnect(true, true);
            delay(1000);
            ESP.restart(); });

        safeServer.begin();
        logMessage(LOG_INFO, "✅ Safe Mode web server started.");
        logMessage(LOG_INFO, "🌐 ESP local IP: " + WiFi.localIP().toString());

        // Blink LED while in Safe Mode
        unsigned long lastBlink = millis();
        bool ledOn = false;

        while (true)
        {
            if (!inOTAUpdate)
            {
                if (millis() - lastBlink > 500)
                {
                    ledOn = !ledOn;
                    if (ledOn)
                    {
                        SmartCore_LED::setRGBColor(255, 255, 0);
                    }
                    else
                    {
                        SmartCore_LED::turnOffRGBLEDs();
                    }
                    lastBlink = millis();
                }
            }

            safeWs.cleanupClients();
            delay(10); // Allow tasks and OTA to breathe
        }
    }

    void loadMqttPriorityList()
    {
        int rawHigh = EEPROM.read(MQTT_PRIORITY_COUNT_ADDR);
        int rawLow = EEPROM.read(MQTT_PRIORITY_COUNT_ADDR + 1);

        Serial.printf("🔍 DEBUG: Raw count bytes: [%d][%d]\n", rawHigh, rawLow);

        mqttPriorityCount = SmartCore_EEPROM::readIntFromEEPROM(MQTT_PRIORITY_COUNT_ADDR);

        Serial.printf("🔍 DEBUG: mqttPriorityCount read = %d\n", mqttPriorityCount);

        if (mqttPriorityCount < 0 || mqttPriorityCount > 3)
        {
            Serial.println("⚠️ DEBUG: Invalid count. Resetting to 0");
            mqttPriorityCount = 0;
        }

        for (int i = 0; i < mqttPriorityCount; i++)
        {
            String stored = SmartCore_EEPROM::readStringFromEEPROM(
                MQTT_PRIORITY_LIST_ADDR + (i * 17),
                17);

            Serial.printf("🔍 DEBUG: Loaded slot %d raw = '%s'\n", i, stored.c_str());

            strncpy(mqttPriorityList[i], stored.c_str(), 16);
            mqttPriorityList[i][16] = '\0';
        }

        Serial.printf("🔄 Loaded %d MQTT priority servers:\n", mqttPriorityCount);
        for (int i = 0; i < mqttPriorityCount; i++)
        {
            Serial.printf("   [%d] %s\n", i, mqttPriorityList[i]);
        }
    }

}

// =============================================================
//  WEAK CALLBACK: SmartBox provisioning (overridden on SmartBox)
// =============================================================
extern "C" __attribute__((weak)) void SmartBox_notifyProvisionReceived(
    const String &ssid,
    const String &password,
    const String &serial,
    bool mqttStatic,
    const String &staticIp,
    const String &subnetMask,
    const String ipList[], // ordered SmartBox list
    int ipCount,           // number of SmartBoxes
    int priority,          // my priority (1 = primary)
    int mqttPort)
{
    // non-SmartBox modules do nothing
}

// =============================================================
//  WEAK CALLBACKS (SmartBox will override)
// =============================================================
extern "C" __attribute__((weak)) void SmartBox_notifyBecomePrimary()
{
    // non-SmartBox modules do nothing
}

extern "C" __attribute__((weak)) void SmartBox_notifySetBroker(const String &newIp)
{
    // non-SmartBox modules do nothing
}
