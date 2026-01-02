#include "SmartCore_Wifi.h"
#include "SmartCore_MQTT.h"
#include "SmartCore_EEPROM.h"
#include "SmartCore_LED.h"
#include "SmartCore_Log.h"
#include "SmartCore_config.h"
#include "SmartCore_Network.h"
#include "esp_task_wdt.h"
#include "SmartCore_System.h"
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

uint16_t mqttPriorityCount = 0;
char mqttPriorityList[3][17] = {0};

namespace SmartCore_WiFi
{

    TaskHandle_t wifiProvisionTaskHandle = NULL;

    static char apName[32];
    static char apPassword[32];

    // Local state (TODO: move globals to a struct if needed)
    AsyncWebServer server(5000);

    // Public entry point
    void startWiFiProvisionTask()
    {
        xTaskCreatePinnedToCore(wifiProvisionTask, "WiFiManager Task", 8192, NULL, 1, &wifiProvisionTaskHandle, 1);
    }

// ======================================================================================
//  WiFi Provisioning / Connection Task
// ======================================================================================
//
//  PURPOSE
//  --------------------------------------------------------------------------------------
//  This task is responsible for bringing the module online over WiFi and handing off
//  control to the MQTT + network monitor logic. It also provides the *only* entry point
//  for user-driven provisioning and recovery networking.
//
//  This task runs once at boot and then self-terminates.
//
// --------------------------------------------------------------------------------------
//  HIGH-LEVEL BEHAVIOR
// --------------------------------------------------------------------------------------
//
//  The task operates in three mutually exclusive modes:
//
//  ┌────────────────────────────────────────────────────────────┐
//  │ 1) SAFE MODE (Crash / FS Failure Recovery)                  │
//  ├────────────────────────────────────────────────────────────┤
//  │ • Triggered when SmartCore_System::bootSafeMode == true     │
//  │ • Ignores provisioning flags and resetConfig                │
//  │ • Attempts WiFi STA connection using stored credentials     │
//  │ • Does NOT block or retry here                              │
//  │ • WiFi/MQTT monitor task handles reconnection + backoff     │
//  │ • MQTT publishes SAFE MODE + fault diagnostics              │
//  │ • If no SSID exists at all → fallback to AP recovery mode   │
//  │                                                            │
//  │ SAFE MODE NEVER:                                            │
//  │   - Writes EEPROM                                           │
//  │   - Starts provisioning portals                             │
//  │   - Clears configuration                                    │
//  │   - Runs module-specific logic                               │
//  └────────────────────────────────────────────────────────────┘
//
//  ┌────────────────────────────────────────────────────────────┐
//  │ 2) FORCED PROVISIONING (User Reset Button)                  │
//  ├────────────────────────────────────────────────────────────┤
//  │ • Triggered when resetConfig == true                        │
//  │ • Clears stored parameters                                  │
//  │ • Starts AP + HTTP provisioning portal                      │
//  │ • Accepts WiFi + MQTT + SmartBox configuration              │
//  │ • Writes configuration to EEPROM                            │
//  │ • Reboots after successful provisioning                     │
//  │                                                            │
//  │ This path is USER-INTENTIONAL only.                          │
//  └────────────────────────────────────────────────────────────┘
//
//  ┌────────────────────────────────────────────────────────────┐
//  │ 3) NORMAL BOOT                                              │
//  ├────────────────────────────────────────────────────────────┤
//  │ • Triggered when wifiSetupComplete == true                  │
//  │ • Attempts initial WiFi STA connection                      │
//  │ • Uses exponential backoff during initial connect           │
//  │ • On success → starts network services (MQTT, monitoring)   │
//  │ • WiFi/MQTT health is then handled elsewhere                │
//  │                                                            │
//  │ If not provisioned → forces resetConfig and reboots         │
//  └────────────────────────────────────────────────────────────┘
//
// --------------------------------------------------------------------------------------
//  DESIGN PRINCIPLES
// --------------------------------------------------------------------------------------
//
//  • SAFE MODE always prefers STA over AP.
//  • AP mode is a last-resort recovery mechanism only.
//  • Provisioning is NEVER automatic — only user-triggered.
//  • This task does not perform long-term retries or monitoring.
//  • Once WiFi is started, responsibility transfers to:
//        → wifiMqttCheckTask()
//        → MQTT failover engine
//
// --------------------------------------------------------------------------------------
//  TASK LIFETIME
// --------------------------------------------------------------------------------------
//
//  • The task exits (vTaskDelete) once its role is complete.
//  • It does NOT remain resident.
//  • All long-running connectivity logic is handled elsewhere.
//
// ======================================================================================

    void wifiProvisionTask(void *parameter)
    {
        logMessage(LOG_INFO, "📡 Starting WiFi Provisioning Task...");

       if (SmartCore_System::bootSafeMode)
        {
            logMessage(LOG_WARN,
                "🧯 SAFE MODE active — attempting WiFi recovery");

            String ssid = SmartCore_EEPROM::readStringFromEEPROM(WIFI_SSID_ADDR, 41);
            String pass = SmartCore_EEPROM::readStringFromEEPROM(WIFI_PASS_ADDR, 41);

            // ─────────────────────────────────────────────
            // CASE 1: No WiFi credentials → AP recovery
            // ─────────────────────────────────────────────
            if (ssid.length() == 0)
            {
                uint8_t mac[6];
                WiFi.softAPmacAddress(mac);
                char macSuffix[7];
                sprintf(macSuffix, "%02X%02X%02X", mac[3], mac[4], mac[5]);

                String apSsid =
                    String(SmartCore_System::getModuleSettings().apSSID) +
                    "_" + String(macSuffix);

                WiFi.mode(WIFI_AP);
                WiFi.softAP(apSsid.c_str(), nullptr);

                SmartCore_LED::setLEDMode(LEDMODE_WAITING);
                SmartCore_LED::currentProvisioningState = PROVISIONING_PORTAL;

                // Stay in AP mode indefinitely
                while (true)
                    vTaskDelay(pdMS_TO_TICKS(500));
            }

            // ─────────────────────────────────────────────
            // CASE 2: Credentials exist → STA recovery
            // ─────────────────────────────────────────────
            bool connected = false;
            unsigned long backoff = WIFI_RETRY_START_MS;

            while (!connected)
            {
                WiFi.mode(WIFI_STA);
                WiFi.softAPdisconnect(true);
                WiFi.begin(ssid.c_str(), pass.c_str());

                unsigned long start = millis();
                while (WiFi.status() != WL_CONNECTED &&
                    millis() - start < backoff)
                {
                    vTaskDelay(pdMS_TO_TICKS(200));
                }

                if (WiFi.status() == WL_CONNECTED)
                {
                    connected = true;
                    startNetworkServices(true);
                    break;
                }

                backoff = min(backoff * 2, 60000UL);
            }

            SmartCore_LED::setLEDMode(LEDMODE_STATUS);
            vTaskDelete(NULL);
            return;
        }

        resetConfig = SmartCore_EEPROM::readResetConfigFlag();
        // resetConfig = true;
        wifiSetupComplete = SmartCore_EEPROM::readBoolFromEEPROM(WIFI_SETUP_COMPLETE_ADDR);

        Serial.printf("wifiSetupComplete flag = %s\n", wifiSetupComplete ? "true" : "false");
        Serial.printf("resetConfig: %d\n", resetConfig);

        // ─────────────────────────────────────────────
        // STEP 1: FORCED PROVISIONING (Reset button held)
        if (resetConfig)
        {
            SmartCore_EEPROM::resetParameters();
            logMessage(LOG_INFO, "🔁 ResetConfig = true → Starting provisioning AP...");

            uint8_t mac[6];
            WiFi.softAPmacAddress(mac);
            char macSuffix[7];
            sprintf(macSuffix, "%02X%02X%02X", mac[3], mac[4], mac[5]);
            String apSsid = String(SmartCore_System::getModuleSettings().apSSID) + "_" + String(macSuffix);

            WiFi.mode(WIFI_AP);
            WiFi.softAP(apSsid.c_str(), nullptr);

            SmartCore_LED::setLEDMode(LEDMODE_WAITING);
            SmartCore_LED::currentProvisioningState = PROVISIONING_PORTAL;

            server.on(
                "/setup",
                HTTP_POST,
                [](AsyncWebServerRequest *request) {},
                nullptr,
                [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                {
                    static String jsonBuffer;

                    if (index == 0)
                        jsonBuffer = "";
                    jsonBuffer.concat((char *)data, len);

                    if (index + len != total)
                        return;

                    DynamicJsonDocument doc(1024);
                    DeserializationError err = deserializeJson(doc, jsonBuffer);

                    if (err)
                    {
                        request->send(400, "application/json",
                                      "{\"status\":\"fail\",\"msg\":\"Bad JSON\"}");
                        return;
                    }
                    // ===== COMMON FIELDS =====
                    const char *ssid = doc["ssid"] | "";
                    const char *password = doc["password"] | "";
                    const char *serial = doc["serialNumber"] | "";
                    int mqttPort = doc["mqttPort"] | 1883;

                    if (strlen(ssid) > 32 || strlen(password) > 64)
                    {
                        request->send(400, "application/json",
                                      "{\"status\":\"fail\",\"msg\":\"Invalid SSID/password length\"}");
                        return;
                    }

                    // ===== Write common values =====
                    SmartCore_EEPROM::writeStringToEEPROM(WIFI_SSID_ADDR, ssid, 41);
                    SmartCore_EEPROM::writeStringToEEPROM(WIFI_PASS_ADDR, password, 41);
                    SmartCore_EEPROM::writeStringToEEPROM(SN_ADDR, serial, 41);
                    SmartCore_EEPROM::writeIntToEEPROM(mqttPort, MQTT_PORT_ADDR);

                    // ===== Detect SmartBox vs SmartModule =====
                    bool isSmartBox = doc.containsKey("mqttStatic") ||
                                      doc.containsKey("backupSystemBoxes");

                    bool isSmartModule = doc.containsKey("mqttPriorityList");

                    // ===== SMARTBOX PROVISIONING =====
                    if (isSmartBox)
                    {
                        bool mqttStatic = doc["mqttStatic"] | false;
                        const char *staticIp = doc["staticIp"] | "0.0.0.0";
                        const char *mask = doc["subnetMask"] | "0.0.0.0";
                        int priority = doc["smartboxPriority"] | 1;

                        Serial.print("Priority received from APP:"); Serial.println(priority);

                        SmartCore_EEPROM::writeBoolToEEPROM(MQTT_STATIC_FLAG_ADDR, mqttStatic);
                        SmartCore_EEPROM::writeStringToEEPROM(MQTT_STATIC_IP_ADDR, staticIp, 17);
                        SmartCore_EEPROM::writeStringToEEPROM(MQTT_STATIC_MASK_ADDR, mask, 17);
                        Serial.print("saving priority to eeprom :"); Serial.println(priority);
                        SmartCore_EEPROM::writeIntToEEPROM(priority, SMARTBOX_PRIORITY_ADDR);

                        // MAX 3 SmartBoxes
                        const int MAX_SB = 3;

                        // Clear old entries FIRST
                        for (int i = 0; i < MAX_SB; i++)
                            SmartCore_EEPROM::writeStringToEEPROM(
                                MQTT_BACKUP_LIST_ADDR + i * 17, "", 17);

                        String backups[MAX_SB];
                        int backupCount = 0;

                        if (doc["smartboxIpList"].is<JsonArray>())
                        {
                            JsonArray arr = doc["smartboxIpList"].as<JsonArray>();

                            // Enforce limit
                            int count = min((int)arr.size(), MAX_SB);
                            SmartCore_EEPROM::writeIntToEEPROM(count, MQTT_BACKUP_COUNT_ADDR);

                            int idx = 0;

                            for (JsonVariant v : arr)
                            {
                                if (idx >= MAX_SB)
                                    break; // HARD LIMIT

                                const char *ip = v.as<const char *>();
                                if (!ip)
                                    continue;

                                // Basic IPv4 sanity check
                                if (strlen(ip) < 7 || strlen(ip) > 15)
                                    continue;

                                // EEPROM write
                                SmartCore_EEPROM::writeStringToEEPROM(
                                    MQTT_BACKUP_LIST_ADDR + idx * 17, ip, 17);

                                // Notify SmartBox override (same idx limit)
                                backups[backupCount++] = String(ip);

                                idx++;
                            }
                        }

                        // Final SmartBox notification callback
                        SmartBox_notifyProvisionReceived(
                            String(ssid),
                            String(password),
                            String(serial),
                            mqttStatic,
                            String(staticIp),
                            String(mask),
                            backups,     // only the valid ones (0–3)
                            backupCount, // 0–3
                            priority,
                            mqttPort);
                    }

                    // ===== SMARTMODULE PROVISIONING =====
                    if (isSmartModule)
                    {
                        JsonArray list = doc["mqttPriorityList"].as<JsonArray>();
                        int count = min((int)list.size(), 8);

                        SmartCore_EEPROM::writeIntToEEPROM(count, MQTT_PRIORITY_COUNT_ADDR);

                        // always clear storage first
                        for (int i = 0; i < 8; i++)
                            SmartCore_EEPROM::writeStringToEEPROM(MQTT_PRIORITY_LIST_ADDR + i * 17, "", 17);

                        // now write actual IPs
                        int idx = 0;
                        for (JsonVariant v : list)
                        {
                            if (idx >= 8)
                                break;

                            const char *ip = v.as<const char *>();
                            if (!ip)
                                continue;

                            // basic validation
                            if (strlen(ip) < 7 || strlen(ip) > 15)
                                continue;

                            SmartCore_EEPROM::writeStringToEEPROM(
                                MQTT_PRIORITY_LIST_ADDR + idx * 17, ip, 17);

                            idx++;
                        }
                    }

                    // ===== Finalize provisioning =====
                    SmartCore_EEPROM::writeBoolToEEPROM(WIFI_SETUP_COMPLETE_ADDR, true);
                    EEPROM.write(WIFI_FAIL_COUNTER_ADDR, 0);
                    EEPROM.commit();

                    SmartCore_EEPROM::writeResetConfigFlag(false);
                    resetConfig = false;

                    SmartCore_LED::setLEDMode(LEDMODE_STATUS);

                    request->send(200, "application/json",
                                  "{\"status\":\"success\",\"msg\":\"Provisioned. Rebooting.\"}");

                    request->onDisconnect([]()
                                          {
                        vTaskDelay(pdMS_TO_TICKS(2000));
                        ESP.restart(); });
                });

            server.begin();
            while (true)
            {
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }

        // ─────────────────────────────────────────────
        // STEP 2: NORMAL BOOT — TRY INITIAL CONNECTION
        if (wifiSetupComplete)
        {
            SmartCore_System::getModuleConfig();
            String ssid = SmartCore_EEPROM::readStringFromEEPROM(WIFI_SSID_ADDR, 41);
            String pass = SmartCore_EEPROM::readStringFromEEPROM(WIFI_PASS_ADDR, 41);

            logMessage(LOG_INFO, "🧪 [WiFi DEBUG] Attempting initial connection");
            logMessage(LOG_INFO, "📶 SSID=[" + ssid + "] (" + String(ssid.length()) + ")");
            logMessage(LOG_INFO, "🔐 PASS=[" + pass + "] (" + String(pass.length()) + ")");

            WiFi.mode(WIFI_STA);
            WiFi.softAPdisconnect(true);
            bool connected = false;

            unsigned long backoff = WIFI_RETRY_START_MS;

            while (!connected)
            {
                logMessage(LOG_INFO, "🔄 WiFi attempt with backoff: " + String(backoff) + " ms");
                logMessage(LOG_INFO, "🧪 [WiFi DEBUG] Attempting initial connection");
                logMessage(LOG_INFO, "📶 SSID=[" + ssid + "] (" + String(ssid.length()) + ")");
                logMessage(LOG_INFO, "🔐 PASS=[" + pass + "] (" + String(pass.length()) + ")");
                WiFi.disconnect(true, false);
                WiFi.begin(ssid.c_str(), pass.c_str());

                unsigned long start = millis();
                while (WiFi.status() != WL_CONNECTED && millis() - start < backoff)
                {
                    vTaskDelay(pdMS_TO_TICKS(200));
                }

                if (WiFi.status() == WL_CONNECTED)
                {
                    connected = true;
                    logMessage(LOG_INFO, "✅ Initial WiFi connection successful");
                    EEPROM.write(WIFI_FAIL_COUNTER_ADDR, 0);
                    EEPROM.commit();
                    startNetworkServices(true);
                    break;
                }

                logMessage(LOG_WARN, "❌ Initial WiFi attempt failed. Retrying...");
                backoff = min(backoff * 2, (unsigned long)60000); // cap at 60s
            }

            // 💡 Once WiFi is up → the monitor task takes over
            SmartCore_LED::setLEDMode(LEDMODE_STATUS);
        }
        else
        {
            // 💣 No provisioning + not in reset = force reset mode
            logMessage(LOG_WARN, "⚠️ Not provisioned and not in reset state — forcing AP provisioning");
            SmartCore_EEPROM::writeResetConfigFlag(true);
            delay(500);
            ESP.restart();
        }

        vTaskDelete(NULL);
    }

    // ======================================================================================
    //  SmartBoat MQTT Prioritised Failover Boot Logic
    // ======================================================================================
    //
    //  SmartModules and SmartBox units use a prioritised MQTT broker list.
    //  Index 0 is always the PRIMARY SmartBox broker.
    //  Index 1..N are backup SmartBoxes in descending priority.
    //
    //  This function handles *boot-time broker selection*, including:
    //    • Normal operation
    //    • Failover recovery
    //    • Reverse-failover (returning to the real primary)
    //
    // --------------------------------------------------------------------------------------
    //  WHY WE NEED THIS
    // --------------------------------------------------------------------------------------
    //
    //  When the primary SmartBox goes offline, modules fail over to the next broker.
    //  However, if a module reboots while the primary is still offline,
    //  it should *not* waste 10–15 minutes retrying the primary before connecting.
    //
    //  Similarly, when the primary SmartBox comes back online,
    //  modules should automatically return to it (reverse-failover),
    //  but not by constantly disconnecting and reconnecting.
    //
    // --------------------------------------------------------------------------------------
    //  HOW THIS SYSTEM WORKS
    // --------------------------------------------------------------------------------------
    //
    //  1) We store the last successfully-used broker priority index in EEPROM:
    //
    //         MQTT_LAST_PRIORITY_ADDR → 0 = primary, 1+ = backup
    //
    //  2) On boot:
    //       - We load the last used priority index.
    //       - If it was 0 → normal boot; connect to primary.
    //       - If it was >0 → we TEST PRIORITY 0 *3 times only*.
    //
    //       This gives:
    //         • Fast reconnect to backup if primary is still down
    //         • Fast automatic revert to primary if it has come back
    //         • No endless switching or prolonged delays
    //
    //  3) If priority 0 responds → we switch back to it and save index 0 to EEPROM.
    //     If priority 0 does NOT respond → we silently continue using the backup.
    //
    //  4) Normal failover still occurs later inside wifiMqttCheckTask:
    //         - MQTT reconnection attempts
    //         - Hard-reset retry
    //         - Full failover sequence (including SmartBox-aware logic)
    //
    // --------------------------------------------------------------------------------------
    //  BENEFITS
    // --------------------------------------------------------------------------------------
    //
    //   ✔ Zero long delay when booting during an outage
    //   ✔ Automatic return to the primary broker when it’s healthy
    //   ✔ No periodic probing or unnecessary reconnects
    //   ✔ EEPROM write minimization (only written after successful connect)
    //   ✔ Clean and deterministic behaviour for all module types
    //
    // --------------------------------------------------------------------------------------
    //  SUMMARY
    // --------------------------------------------------------------------------------------
    //
    //   This boot logic ensures the module always uses the best available MQTT broker,
    //   instantly recovers from outages, and participates cleanly in SmartBox cluster
    //   failover without adding instability or network noise.
    //
    // ======================================================================================

    void startNetworkServices(bool mqttRequired = true)
    {
        logMessage(LOG_INFO, "🌐 Starting Network Services...");
        waitForValidIP();
        vTaskDelay(pdMS_TO_TICKS(200));

        if (!mqttRequired)
        {
            Serial.println("🚫 MQTT skipped (not required)");
            return;
        }

        // -------------------------------------------------------------
        // Load last used broker index
        // -------------------------------------------------------------
        uint8_t savedIndex =
            SmartCore_EEPROM::readByteFromEEPROM(MQTT_LAST_PRIORITY_ADDR);

        if (savedIndex >= mqttPriorityCount)
            savedIndex = 0;

        SmartCore_MQTT::currentPriorityIndex = savedIndex;

        // -------------------------------------------------------------
        // Try PRIORITY 0 first if we were previously using a backup
        // -------------------------------------------------------------
        const int TRY_PRIMARY_ON_BOOT = 3;

        if (savedIndex != 0)
        {
            logMessage(LOG_INFO,
                       "🔍 Previously using backup priority " + String(savedIndex) +
                           ". Testing priority 0...");

            String primaryIP = mqttPriorityList[0];
            uint16_t primaryPort = mqtt_port;

            bool primaryConnected = false;

            for (int attempt = 1; attempt <= TRY_PRIMARY_ON_BOOT; attempt++)
            {
                SmartCore_MQTT::hardResetClient();

                logMessage(LOG_INFO,
                           "🧪 Testing PRIMARY broker attempt " + String(attempt) +
                               " → " + primaryIP + ":" + String(primaryPort));

                SmartCore_MQTT::setupMQTTClient(primaryIP, primaryPort);

                vTaskDelay(pdMS_TO_TICKS(2000)); // allow connection attempt

                if (mqttIsConnected)
                {
                    logMessage(LOG_INFO,
                               "🎉 PRIMARY is back online! Using priority 0.");
                    SmartCore_MQTT::currentPriorityIndex = 0;

                    // Persist the change immediately
                    SmartCore_EEPROM::writeByteToEEPROM(
                        MQTT_LAST_PRIORITY_ADDR, 0);
                    EEPROM.commit();

                    primaryConnected = true;
                    break;
                }
            }

            if (!primaryConnected)
            {
                logMessage(LOG_WARN,
                           "⚠️ PRIMARY still offline → using backup priority " +
                               String(savedIndex));

                SmartCore_MQTT::setupMQTTClient(
                    mqttPriorityList[savedIndex], mqtt_port);
            }
        }
        else
        {
            // -------------------------------------------------------------
            // Normal boot using priority 0
            // -------------------------------------------------------------
            String ip = mqttPriorityList[0];
            if (ip.length() == 0 || ip == "0.0.0.0" || ip == "127.0.0.1" || mqttPriorityCount == 0)
            {
                Serial.println("⚠️ MQTT not started: No valid broker IP found");
                return;
            }
            logMessage(LOG_INFO, "🚀 Starting MQTT using primary → " + ip);
            SmartCore_MQTT::setupMQTTClient(ip, mqtt_port);
        }

        // -------------------------------------------------------------
        // Start WiFi/MQTT health monitor
        // -------------------------------------------------------------
        if (wifiMqttCheckTaskHandle == nullptr)
        {
            xTaskCreatePinnedToCore(
                SmartCore_LED::wifiMqttCheckTask,
                "WiFi/MQTT Check",
                4096, NULL, 1,
                &wifiMqttCheckTaskHandle,
                0);
        }

        SmartCore_System::createAppTasks();

        SmartCore_MQTT::checkForUpgrade(false);
    }

    void waitForValidIP()
    {
        int tries = 0;
        while ((WiFi.localIP() == IPAddress(0, 0, 0, 0)) && (tries++ < 20))
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    bool hasStoredCreds()
    {
        String ssid = SmartCore_EEPROM::readStringFromEEPROM(WIFI_SSID_ADDR, 41);
        String pass = SmartCore_EEPROM::readStringFromEEPROM(WIFI_PASS_ADDR, 41);

        // Trim accidental whitespace
        ssid.trim();
        pass.trim();

        // SSID must exist, must not be blank, must not be 0xFF garbage
        if (ssid.length() == 0 || ssid == "FFFFFFFF" || ssid == "ffffffffff")
            return false;

        // Password is optional — some networks have none.
        // BUT it must not be garbage (all 0xFF).
        if (pass == "FFFFFFFF" || pass == "ffffffffff")
            return false;

        return true;
    }

}