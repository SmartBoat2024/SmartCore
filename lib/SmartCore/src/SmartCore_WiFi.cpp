#include "SmartCore_Wifi.h"
#include "SmartCore_MQTT.h"
#include "SmartCore_EEPROM.h"
#include "SmartCore_LED.h"
#include "SmartCore_Log.h"
#include "SmartCore_config.h"
#include "SmartCore_Network.h"
#include "esp_task_wdt.h"
#include "SmartCore_System.h"
#include "SmartCore_Webserver.h"
#include <SmartConnect_Async_WiFiManager.h>
#include "SmartCore_Webserver.h"
#include <ESPmDNS.h>

namespace SmartCore_WiFi {

    TaskHandle_t wifiManagerTaskHandle = NULL;

    static char apName[32];
    static char apPassword[32];

    // Local state (TODO: move globals to a struct if needed)
    AsyncWebServer server(80);
    static AsyncDNSServer* dns = nullptr;
    static ESPAsync_WiFiManager* wifiManager = nullptr;

    // Public entry point
    void startWiFiManagerTask() {
        xTaskCreatePinnedToCore(wifiManagerTask,        "WiFiManager Task",    8192, NULL, 1, &wifiManagerTaskHandle,        1);
    }

    void wifiManagerTask(void *parameter) {
        wifiManagerStartTime = millis();
        logMessage(LOG_INFO, "📡 Starting WiFiManager Task...");
    
        //resetConfig = true; //for testing only
        resetConfig = SmartCore_EEPROM::readResetConfigFlag();                          
        smartConnectEnabled = SmartCore_EEPROM::readBoolFromEEPROM(SC_BOOL_ADDR);      
        standaloneMode =  SmartCore_EEPROM::readBoolFromEEPROM(STANDALONE_MODE_ADDR);    
        wifiSetupComplete =  SmartCore_EEPROM::readBoolFromEEPROM(WIFI_SETUP_COMPLETE_ADDR);
        currentLEDMode = LEDMODE_STATUS;
    
        // 🔍 Diagnostic Summary
        logMessage(LOG_INFO, "🔍 WiFi Setup Summary:");
        logMessage(LOG_INFO, "  📂 resetConfig         : " + String(resetConfig ? "true" : "false"));
        logMessage(LOG_INFO, "  📡 smartConnectEnabled : " + String(smartConnectEnabled ? "true" : "false"));
        logMessage(LOG_INFO, "  🛠️  standaloneMode      : " + String(standaloneMode ? "true" : "false"));
        logMessage(LOG_INFO, "  🔐 wifiSetupComplete   : " + String(wifiSetupComplete ? "true" : "false"));
    
        if (resetConfig) {
            //SmartCore_EEPROM::resetParameters();
            logMessage(LOG_INFO, "🔁 resetConfig = true → Forcing WiFiManager portal...");
            currentLEDMode = LEDMODE_WAITING;
            currentProvisioningState = PROVISIONING_PORTAL;
            setupWiFiManager();  // Will restart on completion
        } else {
            SmartCore_System::getModuleConfig();  // Load flags: resetConfig, smartConnectEnabled, etc.
    
            if (smartConnectEnabled && !wifiSetupComplete) {
                logMessage(LOG_INFO, "🚀 SmartConnect awaiting credentials...");
                currentLEDMode = LEDMODE_WAITING;
                currentProvisioningState = PROVISIONING_SMARTCONNECT;
    
                String apSSID = "SB_Internal_" + String(serialNumber);
                WiFi.mode(WIFI_AP);
                WiFi.softAP(apSSID.c_str(), "12345678");
    
                unsigned long apWaitStart = millis();
                IPAddress apIP;
    
                do {
                    apIP = WiFi.softAPIP();
                    vTaskDelay(pdMS_TO_TICKS(50));
                } while ((apIP.toString() == "0.0.0.0") && millis() - apWaitStart < 3000);
    
                logMessage(LOG_INFO, "🔗 SoftAP IP: " + String(apIP));

    
                server.on("/provision", HTTP_POST, [](AsyncWebServerRequest *request) {
                    String ssid = request->arg("ssid");
                    String pass = request->arg("pass");
                
                    logMessage(LOG_INFO, "📥 Received creds:");
                    logMessage(LOG_INFO, "  📶 SSID : " + String(ssid));
                    logMessage(LOG_INFO, "  🔐 PASS : " + String(pass));
                    SmartCore_EEPROM::writeStringToEEPROM(WIFI_SSID_ADDR, ssid);
                    SmartCore_EEPROM::writeStringToEEPROM(WIFI_PASS_ADDR, pass);
                    wifiSetupComplete = true;
                    SmartCore_EEPROM::writeBoolToEEPROM(WIFI_SETUP_COMPLETE_ADDR, true);
                    provisioningComplete = true;
                
                    logMessage(LOG_INFO, "💾 EEPROM Values After Write:");
                    logMessage(LOG_INFO, "  ✅ WIFI_SSID_ADDR       : " + SmartCore_EEPROM::readStringFromEEPROM(WIFI_SSID_ADDR, 40));
                    logMessage(LOG_INFO, "  ✅ WIFI_PASS_ADDR       : " + SmartCore_EEPROM::readStringFromEEPROM(WIFI_PASS_ADDR, 40));
                    logMessage(LOG_INFO, "  ✅ WIFI_SETUP_COMPLETE  : " + String(SmartCore_EEPROM::readBoolFromEEPROM(WIFI_SETUP_COMPLETE_ADDR) ? "true" : "false"));
                
                    // ✅ Send response now
                    request->send(200, "text/plain", "✅ Credentials received. Rebooting...");
                
                    // ✅ Reboot only *after* client disconnects (SmartBox got response)
                    request->onDisconnect([]() {
                        logMessage(LOG_INFO, "🔌 SmartBox disconnected after provisioning. Rebooting...");
                        vTaskDelay(pdMS_TO_TICKS(100)); // small safety delay
                        ESP.restart();
                    });
                });
    
                server.begin();
    
                unsigned long start = millis();
                while (!provisioningComplete && millis() - start < 50000) {
                    vTaskDelay(pdMS_TO_TICKS(100));  // ✅ yields during wait
                }
    
                if (provisioningComplete) {
                    logMessage(LOG_INFO, "⏳ Provisioning completed, skipping timeout fallback...");
                
                    server.end();                 // 🔒 Cleanly shut down the server
                    WiFi.softAPdisconnect(true);  // 📡 Kill SoftAP before reboot
                
                    vTaskDelete(NULL);            // ✅ End task cleanly (or use return;)
                } else {
                    logMessage(LOG_INFO, "⌛ SmartConnect timeout. Falling back...");
                    smartConnectEnabled = false;
                    resetConfig = true;
                    SmartCore_EEPROM::writeBoolToEEPROM(SC_BOOL_ADDR, false);
                    SmartCore_EEPROM::writeResetConfigFlag(true);
    
                    WiFi.softAPdisconnect(true);  // Disconnect SoftAP mode
                    delay(100);                   // Let SoftAP disconnect
                    WiFi.mode(WIFI_OFF);          // Turn off Wi-Fi mode completely
                    delay(100);  
                    ESP.restart();
                }
    
                
            }
            else if (smartConnectEnabled && wifiSetupComplete) {
                logMessage(LOG_INFO, "📶 Connecting with SmartBox-provided creds...");
    
                String ssid = SmartCore_EEPROM::readStringFromEEPROM(WIFI_SSID_ADDR, 40);
                String pass = SmartCore_EEPROM::readStringFromEEPROM(WIFI_PASS_ADDR, 40);
                WiFi.begin(ssid.c_str(), pass.c_str());
    
                unsigned long start = millis();
                while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
                    vTaskDelay(pdMS_TO_TICKS(100));  // ✅ fix: was checking provisioningComplete
                }
    
                if (WiFi.status() == WL_CONNECTED) {
                    logMessage(LOG_INFO, "✅ Connected with SmartConnect creds");
                    currentLEDMode = LEDMODE_STATUS;
                    startNetworkServices(true);
                } else {
                    logMessage(LOG_INFO, "❌ Failed SmartConnect. Fallback to WiFiManager...");
                    smartConnectEnabled = false;
                    resetConfig = true;
                    SmartCore_EEPROM::writeBoolToEEPROM(SC_BOOL_ADDR, false);
                    SmartCore_EEPROM::writeResetConfigFlag(true);
                    ESP.restart();
                }
            }
            else if (standaloneMode) {
                logMessage(LOG_INFO, "📡 Standalone mode active → Starting SoftAP");
                currentLEDMode = LEDMODE_STATUS;
    
                WiFi.mode(WIFI_AP);
                WiFi.softAP(apName, apPassword);
                startNetworkServices(false);  // No MQTT in standalone
            }
            else if (wifiSetupComplete) {
                logMessage(LOG_INFO, "🔍 Attempting WiFi autoConnect...");

                if (dns) {
                    logMessage(LOG_WARN, "♻️ Deleting old dns instance before reallocation...");
                    delete dns;
                    dns = nullptr;
                }
                
                if (wifiManager) {
                    logMessage(LOG_WARN, "♻️ Deleting old wifiManager instance...");
                    delete wifiManager;
                    wifiManager = nullptr;
                }
                
                // Create new DNS and WiFiManager cleanly
                logMessage(LOG_INFO, "🛠️ Creating new AsyncDNSServer...");
                dns = new AsyncDNSServer();
                
                logMessage(LOG_INFO, "🛠️ Creating new WiFiManager...");
                wifiManager = new ESPAsync_WiFiManager(&server, dns);
                
                bool res = wifiManager->autoConnect("SmartController");
                vTaskDelay(pdMS_TO_TICKS(100));  // Let system breathe
    
                if (!res) {
                    setupWiFiManager();  // fallback portal
                    return;
                } else {
                    logMessage(LOG_INFO, "✅ WiFiManager autoConnect success.");
                    currentLEDMode = LEDMODE_STATUS;
                    startNetworkServices(true);
                }
            }
            else {
                logMessage(LOG_INFO, "🔍 Default: WiFiManager autoConnect attempt...");
                currentLEDMode = LEDMODE_WAITING;
                currentProvisioningState = PROVISIONING_PORTAL;
                setupWiFiManager();
                return;
            }
        }
    
        wifiManagerFinished = true;
        logMessage(LOG_INFO, "✅ WiFiManager task complete.");
        currentLEDMode = LEDMODE_STATUS;
        vTaskDelay(pdMS_TO_TICKS(100));
        vTaskDelete(NULL);

        // ✅ At the end:
        vTaskDelete(NULL);
    }

    void setupWiFiManager() {
        logMessage(LOG_INFO, "📶 setupWiFiManager() called");
        static bool wifiManagerInitialized = false;
    
        currentLEDMode = LEDMODE_WAITING;
        currentProvisioningState = PROVISIONING_PORTAL;
    
        // Use safer constructor without DNS
        dns = new AsyncDNSServer();
        wifiManager = new ESPAsync_WiFiManager(&server, dns, "SmartConnect");
    
        logMessage(LOG_INFO, "🔁 [DEBUG] resetConfig: " + String(resetConfig));
    
        if (resetConfig) {
            // Generate random 4-digit number as suffix
            uint16_t randSuffix = random(1000, 9999);  // Make sure randomSeed() was called earlier
    
            // Generate AP name with suffix
            snprintf(apName, sizeof(apName), "SmartConnect_%s_%04d", serialNumber, randSuffix);
            strncpy(apPassword, "12345678", sizeof(apPassword) - 1);
            apPassword[sizeof(apPassword) - 1] = '\0';
    
            SmartCore_EEPROM::resetParameters();
    
            logMessage(LOG_INFO, "⛔ Erasing WiFi stack and credentials...");
            WiFi.disconnect(true, true);
            delay(100);
            WiFi.mode(WIFI_OFF);
            delay(100);
    
            logMessage(LOG_INFO, "🧹 Resetting WiFiManager settings...");
            wifiManager->resetSettings();
            delay(100);
    
            // Recreate WiFiManager (DNS not used anymore)
            delete wifiManager;
            dns = new AsyncDNSServer();
            wifiManager = new ESPAsync_WiFiManager(&server, dns, "SmartCtrl");
        } else {
            SmartCore_System::getModuleConfig();
        }
    
        // Standard dual-mode
        WiFi.softAPdisconnect(true);
        delay(100);
        WiFi.mode(WIFI_AP_STA);
        delay(100);
    
        wifiManager->setConnectTimeout(20);
        wifiManager->setConfigPortalTimeout(180);
        wifiManager->setDebugOutput(true);

        //Update custom_serial to the generic module serial number before starting portal
        custom_serial = new ESPAsync_WMParameter("serialNumber", "Please Enter Serial Number", serialNumber, 40);
        custom_webname = new ESPAsync_WMParameter("CustomWebName", "Please Enter a Custom Webname", webnamechar, 40);

    
        if (!wifiManagerInitialized) {
            wifiManager->addParameter(custom_serial);
            wifiManager->addParameter(custom_webname);
            wifiManagerInitialized = true;
        }
    
        bool res = false;
    
        // 🔥 Disable WDT for this task
        esp_task_wdt_delete(NULL);
    
        if (resetConfig) {
            logMessage(LOG_INFO, "🚪 Forcing config portal (resetConfig = true)...");
            res = wifiManager->startConfigPortal(apName);
        } else {
            logMessage(LOG_INFO, "🔍 Attempting WiFi autoConnect...");
            res = wifiManager->autoConnect("SmartCtroller");
    
            if (!res) {
                logMessage(LOG_INFO, "⚠️ autoConnect failed. Launching config portal...");
                res = wifiManager->startConfigPortal(apName);
            } else {
                logMessage(LOG_INFO, "✅ WiFiManager autoConnect success.");
                setLEDMode(LEDMODE_STATUS);
                startNetworkServices(true);
            }
        }
    
        // ✅ Re-enable WDT now that blocking operation is done
        esp_task_wdt_add(NULL);
    
        // Final connection check
        if (WiFi.status() == WL_CONNECTED) {
            logMessage(LOG_INFO, "✅ WiFi connected via WiFiManager.");
            setLEDMode(LEDMODE_STATUS);
    
            if (resetConfig && !smartConnectEnabled) {
                
                saveConfigAndRestart();
            }
        } else {
            logMessage(LOG_INFO, "❌ WiFi failed. Calling handleFailedConnection()...");
            setLEDMode(LEDMODE_STATUS);
            handleFailedConnection();
        }
    }
    
    void handleFailedConnection() {
        logMessage(LOG_INFO, "🚨 handleFailedConnection() triggered.");
    
        if (standaloneFlag || standaloneMode) {
            SmartCore_EEPROM::writeStandaloneModeToEEPROM(true);
            saveConfigAndRestart();  // This resets flags and restarts
        } else {
            logMessage(LOG_INFO, "⚠️ WiFi credentials likely invalid — flashing error pattern...");
            triggerFlashPattern(".....", DEBUG_COLOR_RED);  // Five short red blinks
    
            vTaskDelay(pdMS_TO_TICKS(3000));  // Allow partial flashing before restart
            ESP.restart();  // Or use resetModule() if cleanup is needed
        }
    }

    void saveConfigAndRestart() {
        logMessage(LOG_INFO, "Saving configuration and restarting...");
        logMessage(LOG_INFO, String("🧠 autoProvisioned = ") + (autoProvisioned ? "true" : "false"));
        if (!autoProvisioned){
                // Serial Number
            const char* serialFromForm = custom_serial->getValue();
            if (serialFromForm && strlen(serialFromForm) > 0) {
                strncpy(serialNumber, serialFromForm, sizeof(serialNumber) - 1);
                serialNumber[sizeof(serialNumber) - 1] = '\0';
                SmartCore_EEPROM::writeSerialNumberToEEPROM();
                serialNumberAssigned = true;
                SmartCore_EEPROM::writeSerialNumberAssignedFlag(true);
            } else {
                logMessage(LOG_WARN, "⚠️ custom_serial was null or empty.");
                serialNumberAssigned = false;
                SmartCore_EEPROM::writeSerialNumberAssignedFlag(false);
            }

            // Webname
            if(!smartConnectEnabled){
                const char* webnameFromForm = custom_webname->getValue();
                if (webnameFromForm && strlen(webnameFromForm) > 0) {
                    strncpy(webnamechar, webnameFromForm, sizeof(webnamechar) - 1);
                    webnamechar[sizeof(webnamechar) - 1] = '\0';
                    webname = String(webnamechar);
                    SmartCore_EEPROM::writeCustomWebnameToEEPROM();
    
                    logMessage(LOG_INFO, "✅ [WiFiManager] Saved Custom Webname: " + webname);
                } else {
                    logMessage(LOG_WARN, "⚠️ custom_webname was null or empty.");
                }
            }
           
        } else {
            logMessage(LOG_INFO, "📦 Skipping custom_serial and webname — auto SmartConnect mode.");
        }
    
        // Save AP Name and Password
       /* strncpy(apName, custom_ap_name.getValue(), sizeof(apName) - 1);
        apName[sizeof(apName) - 1] = '\0';
        writeStringToEEPROM(CUSTOM_AP_NAME_ADDR, apName);
    
        strncpy(apPassword, custom_ap_pass.getValue(), sizeof(apPassword) - 1);
        apPassword[sizeof(apPassword) - 1] = '\0';
        writeStringToEEPROM(CUSTOM_AP_PASS_ADDR, apPassword);*/
    
        wifiSetupComplete = true;
        SmartCore_EEPROM::writeBoolToEEPROM(WIFI_SETUP_COMPLETE_ADDR, true);
    
        // Save webname to EEPROM
        webname = String(webnamechar);
        SmartCore_EEPROM::writeStringToEEPROM(WEBNAME_ADDR, webname);
    
        // Reset Config and Flag if needed
        logMessage(LOG_INFO, "Set Reset Config to false");
        resetConfig = false;
        SmartCore_EEPROM::writeResetConfigFlag(false);  // Ensure resetConfig is saved as false
    
        logMessage(LOG_INFO, "Configuration saved. Restarting now...");
        delay(1000);  // Give some time to finish serial output
        ESP.restart();  // Restart the ESP32 to apply new settings
    }

    void startNetworkServices(bool mqttRequired = true) {
        logMessage(LOG_INFO, "🌐 Starting Network Services...");
        
        #ifdef WEBSERVER_ENABLED
        logMessage(LOG_INFO, "🖥️  Web server enabled — starting...");
        // Always sync webname from char
        webname = String(webnamechar);
    
        SmartCore_Websocket::setupMDNS();
        SmartCore_Websocket::setupWebServer();
        #endif
        
        smartBoatEnabled = true;  //for testing

        if (mqttRequired) {
            if (smartBoatEnabled || customMqtt) {
                Serial.println("📡 MQTT enabled — configuring...");
                SmartCore_MQTT::setupMQTTClient();
            } else {
                Serial.println("ℹ️ MQTT not enabled (SmartBoat and Custom MQTT both false)");
            }
        } else {
            Serial.println("🚫 MQTT skipped (standalone mode or not required)");
        }
    
        SmartCore_System::createAppTasks();
    
    }

    //SAFE CONNECT FOR RECOVERY
    void startMinimalWifiSetup() {

        logMessage(LOG_INFO, "⛔ Erasing WiFi stack and credentials...");
        WiFi.disconnect(true, true);
        delay(100);
        WiFi.mode(WIFI_OFF);
        delay(100);

        logMessage(LOG_INFO, "🧹 Resetting WiFiManager settings...");
        if (wifiManager) {
            wifiManager->resetSettings();
            delete wifiManager;
            wifiManager = nullptr;
        }

        if (dns) {
            delete dns;
            dns = nullptr;
        }

        dns = new AsyncDNSServer();
        wifiManager = new ESPAsync_WiFiManager(&server, dns, "SmartCtrl");

        // Dual-mode setup
        WiFi.softAPdisconnect(true);
        delay(100);
        WiFi.mode(WIFI_AP_STA);
        delay(100);

        wifiManager->setConnectTimeout(20);
        wifiManager->setConfigPortalTimeout(180);
        wifiManager->setDebugOutput(true);

        logMessage(LOG_INFO, "🔍 Attempting WiFi autoConnect...");
        wifiManager->startConfigPortal("SmartSafeConnect");
        
        if (WiFi.status() == WL_CONNECTED) {
            if (MDNS.begin("recovery")) {
                logMessage(LOG_INFO, "✅ mDNS started");
                MDNS.addService("http", "tcp", 81);
            } else {
                logMessage(LOG_WARN, "⚠️ Failed to start mDNS");
            }
        
            logMessage(LOG_INFO, "✅ WiFi connected via WiFiManager.");
        }

        if (WiFi.status() == WL_CONNECTED) {
            logMessage(LOG_INFO, "✅ WiFi connected via WiFiManager.");
        }
    }

}