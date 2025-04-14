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
#include <SmartConnect_Async_WiFiManager.h>
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


namespace SmartCore_System {

    TaskHandle_t resetButtonTaskHandle = NULL;

    const unsigned long holdTime = 5000;
    unsigned long buttonPressStart = 0;
    bool buttonPressed = false;
    volatile bool inOTAUpdate = false;

    AsyncEventSource events("/events");

    static SmartCoreSettings _settings = {
        .serialNumber = "UNKNOWN",
        .moduleName = "Generic SmartCore"
    };

    void setModuleSettings(const SmartCoreSettings& settings) {
        _settings = settings;
    
        logMessage(LOG_INFO, String("📦 Module settings updated → serial: ") + 
                   (settings.serialNumber ? settings.serialNumber : "nullptr") + 
                   ", name: " + 
                   (settings.moduleName ? settings.moduleName : "nullptr"));
    
        // 🧠 SYNC TO GLOBAL BUFFERS
        strncpy(serialNumber, settings.serialNumber, sizeof(serialNumber) - 1);
        serialNumber[sizeof(serialNumber) - 1] = '\0';
    
        // 💾 WRITE TO EEPROM
        SmartCore_EEPROM::writeStringToEEPROM(SN_ADDR, String(settings.serialNumber));
        SmartCore_EEPROM::writeModuleNameToEEPROM(settings.moduleName);
    }

    void preinit(){
        Serial.begin(115200);
        delay(4000);

        if (!EEPROM.begin(1024)) {
            logMessage(LOG_ERROR, "❌ EEPROM.begin failed!");
            while (true) yield();
        }
        
        // 🔍 Read and increment crash counter
        uint8_t crashCounter = EEPROM.read(CRASH_COUNTER_ADDR);
        crashCounter++;
        EEPROM.write(CRASH_COUNTER_ADDR, crashCounter);
        EEPROM.commit();

        uint8_t runtimeCrashCounter = EEPROM.read(RUNTIME_CRASH_COUNTER_ADDR);
        runtimeCrashCounter++;
        EEPROM.write(RUNTIME_CRASH_COUNTER_ADDR, runtimeCrashCounter);
        EEPROM.commit();

        OTADRIVE.setInfo(APIKEY, FW_VER);
        OTADRIVE.onUpdateFirmwareProgress(SmartCore_OTA::onUpdateProgress);

        logMessage(LOG_WARN, String("🧠 Boot crash counter: ") + crashCounter);
        logMessage(LOG_WARN, String("🧠 Boot Runtime crash counter: ") + runtimeCrashCounter);

        // 💥 Trigger logic based on counter
        if (crashCounter >= CRASH_LIMIT_SAFE || runtimeCrashCounter >= CRASH_LIMIT_SAFE) {
            logMessage(LOG_ERROR, "🚨 Crash limit exceeded! Entering Safe Mode...");
            enterSafeMode();  // You’ll implement this next
        }
        else if (crashCounter >= CRASH_LIMIT_RESET || runtimeCrashCounter >= CRASH_LIMIT_RESET) {
            logMessage(LOG_WARN, "⚠️ Crash count hit reset threshold. Resetting parameters...");
            xTaskCreatePinnedToCore(SmartCore_System::resetWorkerTask, "ResetWorker", 4096, NULL, 1, NULL, 0);
        }

        // Store resetConfig (might be set by resetWorkerTask)
        resetConfig = SmartCore_EEPROM::readResetConfigFlag();
}
     
    void init() {

        if (!LittleFS.begin()) {
           logMessage(LOG_ERROR, "❌ LittleFS mount failed!");
            while (true) yield();
        }
        
        xTaskCreatePinnedToCore(checkresetButtonTask,   "Check Reset Button",  4096, NULL, 1, &resetButtonTaskHandle,        0);
        initSmartCoreLED();
        SmartCore_I2C::init();
        SmartCore_MCP::init();
        //  SmartNet init + task
        if (SmartCore_SmartNet::initSmartNet()) {
            xTaskCreatePinnedToCore(SmartCore_SmartNet::smartNetTask, "SmartNet_RX_Task", 4096, NULL, 1, &SmartCore_SmartNet::smartNetTaskHandle, 1);
        } else {
            logMessage(LOG_ERROR, "❌ Failed to initialize SmartNet CAN bus");
        }
        SmartCore_WiFi::startWiFiManagerTask();
        
    }

    void getModuleConfig() {
        logMessage(LOG_INFO, "Inside getModuleConfig");
        // Read serial number
        strncpy(serialNumber, SmartCore_EEPROM::readSerialNumberFromEEPROM(), sizeof(serialNumber) - 1);
        serialNumber[sizeof(serialNumber) - 1] = '\0'; // Ensure null-termination
        logMessage(LOG_INFO, "get config --- serialnumber: " + String(serialNumber));
    
        // Read web name
        webname = SmartCore_EEPROM::readStringFromEEPROM(WEBNAME_ADDR, sizeof(webnamechar));
        webname.toCharArray(webnamechar, sizeof(webnamechar));
    
        // Read standalone mode flag
        standaloneMode = SmartCore_EEPROM::readStandaloneModeFromEEPROM();
    
        // Read AP Name and Password from EEPROM and copy them into apName and apPassword buffers
        strncpy(apName, SmartCore_EEPROM::readStringFromEEPROM(CUSTOM_AP_NAME_ADDR, 40).c_str(), sizeof(apName) - 1);
        apName[sizeof(apName) - 1] = '\0';  // Ensure null termination
        
        strncpy(apPassword, SmartCore_EEPROM::readStringFromEEPROM(CUSTOM_AP_PASS_ADDR, 40).c_str(), sizeof(apPassword) - 1);
        apPassword[sizeof(apPassword) - 1] = '\0';  // Ensure null termination

        location = SmartCore_EEPROM::readLocationFromEEPROM();
        
        // Read custom MQTT enabled flag
        customMqtt = SmartCore_EEPROM::readBoolFromEEPROM(CUSTOM_MQTT_ADDR);
        logMessage(LOG_INFO, "Custom MQTT enabled: " + String(customMqtt));
    
        // Check for conflicts between SmartBoat and Custom MQTT
        if (customMqtt && smartBoatEnabled) {
            logMessage(LOG_WARN, "Error: Both SmartBoat and Custom MQTT are enabled. Disabling Custom MQTT.");
            customMqtt = false;
            SmartCore_EEPROM::writeBoolToEEPROM(CUSTOM_MQTT_ADDR, customMqtt);
        }
    
        // Load custom MQTT settings if enabled
        if (customMqtt) {
            // Read custom MQTT server
            strncpy(custom_mqtt_server, SmartCore_EEPROM::readStringFromEEPROM(CUSTOM_MQTT_SERVER_ADDR, sizeof(custom_mqtt_server)).c_str(), sizeof(custom_mqtt_server) - 1);
            custom_mqtt_server[sizeof(custom_mqtt_server) - 1] = '\0';  // Ensure null termination
        
            // Read custom MQTT port (as int)
            custom_mqtt_port = SmartCore_EEPROM::readIntFromEEPROM(CUSTOM_MQTT_PORT_ADDR);
        
            logMessage(LOG_INFO, "Custom MQTT Server: "); Serial.println(custom_mqtt_server);
            logMessage(LOG_INFO, "Custom MQTT Port: "); Serial.println(custom_mqtt_port);
        }

        // Read first WiFi connect flag
        firstWiFiConnect = SmartCore_EEPROM::readFirstWiFiConnectFlag();
        logMessage(LOG_INFO, "📡 firstWiFiConnect: " + String(firstWiFiConnect));

        // Read serial number assigned flag
        serialNumberAssigned = SmartCore_EEPROM::readSerialNumberAssignedFlag();
        logMessage(LOG_INFO, "🧠 serialNumberAssigned: " + String(serialNumberAssigned));
    
        SmartCore_OTA::isUpgradeAvailable = SmartCore_EEPROM::loadUpgradeFlag();
    
        // Read smartBoatEnabled flag
        smartBoatEnabled = EEPROM.read(SB_BOOL_ADDR) == 1;
    
        // Read smartConnectEnabled flag
        smartConnectEnabled = EEPROM.read(SC_BOOL_ADDR) == 1;
        logMessage(LOG_INFO, "smartConnect: " + String(smartConnectEnabled));
    
        moduleName = SmartCore_EEPROM::readModuleNameFromEEPROM();

        getModuleSpecificConfig();  //call module specific config here
    }

    void createAppTasks() {
        //xTaskCreatePinnedToCore(otaTask, "OTA Task", 16384, NULL, 2, &otaTaskHandle, 0);
        //xTaskCreatePinnedToCore(smartNetTask, "SmartNet_RX_Task", 4096, NULL, 1, &smartNetTaskHandle, 1);
    }

    void __attribute__((weak)) getModuleSpecificConfig() {
        logMessage(LOG_WARN, "⚠️ Warning: getModuleSpecificConfig() not overridden in project.");
        // Default empty implementation
    }

    void checkresetButtonTask(void *parameter) {
        for (;;) {
            bool currentButtonState = digitalRead(buttonPin) == LOW;
    
            if (currentButtonState) {
                if (!buttonPressed) {
                    buttonPressStart = millis();
                    buttonPressed = true;
                } else {
                    if (millis() - buttonPressStart >= holdTime) {
                        logMessage(LOG_INFO, "🟢 Reset module triggered — launching reset worker...");
    
                        // Clear handle BEFORE deleting self
                        resetButtonTaskHandle = NULL;
    
                        // Spawn reset worker task
                        xTaskCreatePinnedToCore(resetWorkerTask, "ResetWorker", 4096, NULL, 1, NULL, 0);
    
                        // Kill this task
                        vTaskDelete(NULL);
                    }
                }
            } else {
                buttonPressed = false;
            }
    
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    void resetWorkerTask(void *param) {
        SmartCore_EEPROM::resetParameters();  // ⚙️ Clean config wipe
    
        Serial.println("🧹 Suspending other tasks...");
        TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    
        if (SmartCore_OTA::otaTaskHandle && SmartCore_OTA::otaTaskHandle != currentTask)   vTaskSuspend(SmartCore_OTA::otaTaskHandle);
        if (wifiMqttCheckTaskHandle && wifiMqttCheckTaskHandle != currentTask) vTaskSuspend(wifiMqttCheckTaskHandle);
        if (SmartCore_MQTT::metricsTaskHandle && SmartCore_MQTT::metricsTaskHandle != currentTask) vTaskSuspend(SmartCore_MQTT::metricsTaskHandle);
        if (SmartCore_MQTT::timeSyncTaskHandle && SmartCore_MQTT::timeSyncTaskHandle != currentTask) vTaskSuspend(SmartCore_MQTT::timeSyncTaskHandle);
        if (flashLEDTaskHandle && flashLEDTaskHandle != currentTask)           vTaskSuspend(flashLEDTaskHandle);
        if (provisioningBlinkTaskHandle && provisioningBlinkTaskHandle != currentTask) vTaskSuspend(provisioningBlinkTaskHandle);
        if (SmartCore_WiFi::wifiManagerTaskHandle && SmartCore_WiFi::wifiManagerTaskHandle != currentTask) vTaskSuspend(SmartCore_WiFi::wifiManagerTaskHandle);
        if (SmartCore_SmartNet::smartNetTaskHandle && SmartCore_SmartNet::smartNetTaskHandle != currentTask)           vTaskSuspend(SmartCore_SmartNet::smartNetTaskHandle);
        // TODO: re-enable SmartNet suspend if needed
    
        Serial.println("✅ All tasks suspended.");
        setRGBColor(0, 255, 0);  // 🟢 Visual feedback
    
        vTaskDelay(pdMS_TO_TICKS(250));  // Let things settle
        Serial.println("♻️ Restarting...");
        esp_restart();  // 💥 Full restart
    }

    void clearCrashCounter(CrashCounterType type) {
        bool changed = false;
    
        if (type == CRASH_COUNTER_BOOT || type == CRASH_COUNTER_ALL) {
            if (EEPROM.read(CRASH_COUNTER_ADDR) != 0) {
                EEPROM.write(CRASH_COUNTER_ADDR, 0);
                logMessage(LOG_INFO, "✅ Boot crash counter reset to 0.");
                changed = true;
            }
        }
    
        if (type == CRASH_COUNTER_RUNTIME || type == CRASH_COUNTER_ALL) {
            if (EEPROM.read(RUNTIME_CRASH_COUNTER_ADDR) != 0) {
                EEPROM.write(RUNTIME_CRASH_COUNTER_ADDR, 0);
                logMessage(LOG_INFO, "✅ Runtime crash counter reset to 0.");
                changed = true;
            }
        }
    
        if (changed) {
            EEPROM.commit();
        } else {
            logMessage(LOG_INFO, "ℹ️ Crash counters already at 0. No changes written.");
        }
    }
    

    void enterSafeMode() {
        logMessage(LOG_ERROR, "🧯 Entering SAFE MODE — minimal services only.");
        initSmartCoreLED();
        setRGBColor(255, 255, 0);
    
        SmartCore_WiFi::startMinimalWifiSetup();
    
        // Start Web Server
        static AsyncWebServer safeServer(81);

        SmartCore_OTA::safeWs.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
            AwsEventType type, void *arg, uint8_t *data, size_t len) {
        if (type == WS_EVT_CONNECT) {
        Serial.println("🌐 Recovery WS client connected");
        } else if (type == WS_EVT_DISCONNECT) {
        Serial.println("👋 Recovery WS client disconnected");
        } else if (type == WS_EVT_ERROR) {
        Serial.println("❌ WS error occurred");
        } else if (type == WS_EVT_DATA) {
        Serial.println("💬 WS data received (ignored in Safe Mode)");
        }
        });

        safeServer.addHandler(&SmartCore_OTA::safeWs);
    
        safeServer.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
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
                <button onclick="fetch('/wifi').then(()=>alert('📡 Starting WiFi Config Portal...'))">📡 Reconfigure WiFi</button><br><br>

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
            request->send(200, "text/html", html);
        });
    
        safeServer.on("/retry", HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(200, "text/plain", "Rebooting...");
            delay(1000);
            ESP.restart();
        });
    
        safeServer.on("/clear", HTTP_GET, [](AsyncWebServerRequest* request) {
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
            ESP.restart();
        });
    
        safeServer.on("/update", HTTP_GET, [](AsyncWebServerRequest* request) {
            logMessage(LOG_INFO, "📦 OTA update triggered via recovery UI");
            request->send(200, "text/plain", "Starting OTA task...");
        
            // Give time for WebSocket to connect before OTA starts
            inOTAUpdate = true;
            delay(2000);
        
            SmartCore_OTA::shouldUpdateFirmware = true;
            xTaskCreatePinnedToCore(SmartCore_OTA::otaTask, "OTAUpdate", 8192, NULL, 1, NULL, 0);
        });
    
        safeServer.on("/wifi", HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(200, "text/plain", "Starting WiFi config portal...");
            delay(1000);
            ESP.restart();
        });
    
        safeServer.begin();
        logMessage(LOG_INFO, "✅ Safe Mode web server started.");
        logMessage(LOG_INFO, "🌐 ESP local IP: " + WiFi.localIP().toString());
    
        // Blink LED while in Safe Mode
        unsigned long lastBlink = millis();
        bool ledOn = false;
    
        while (true) {
            if (!inOTAUpdate) {
                if (millis() - lastBlink > 500) {
                    ledOn = !ledOn;
                    if (ledOn) {
                        setRGBColor(255, 255, 0);
                    } else {
                        turnOffRGBLEDs();
                    }
                    lastBlink = millis();
                }
            }
        
            SmartCore_OTA::safeWs.cleanupClients();
            delay(10); // Allow tasks and OTA to breathe
        }
    }
    
    
}

