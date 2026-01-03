#include "SmartCore_SmartNet.h"
#include "SmartCore_MQTT.h"
#include <ArduinoJson.h>
#include <Arduino.h>
#include "SmartCore_Network.h"
#include "SmartCore_EEPROM.h"
#include <driver/twai.h>
#include "SmartCore_PGN.h"
#include "config.h"
#include "SmartCore_Log.h"
#include "SmartCore_System.h"

#ifdef SMARTBOX_BUILD

namespace SmartCore_SmartNet
{
    

    static uint8_t smartNetAddress = SMARTNET_ADDR_UNASSIGNED;
    TaskHandle_t smartNetTaskHandle = NULL;

    /*bool initSmartNet()
    {
        twai_general_config_t g_config =
            TWAI_GENERAL_CONFIG_DEFAULT(SMARTNET_CAN_TX, SMARTNET_CAN_RX, TWAI_MODE_NORMAL);

        twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
        twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

        if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK)
            return false;

        if (twai_start() != ESP_OK)
            return false;

        initializeAddress();

        logMessage(LOG_INFO, "✅ SmartNet CAN bus initialized.");
        return true;
    }*/
    bool initSmartNet()
    {
        twai_general_config_t g_config =
            TWAI_GENERAL_CONFIG_DEFAULT(
                SMARTNET_CAN_TX,
                SMARTNET_CAN_RX,
                TWAI_MODE_LISTEN_ONLY // 👂 silent observer
            );

        twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
        twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

        if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK)
        {
            logMessage(LOG_ERROR, "❌ SmartNet: Driver install failed");
            return false;
        }

        if (twai_start() != ESP_OK)
        {
            logMessage(LOG_ERROR, "❌ SmartNet: Start failed");
            return false;
        }

        logMessage(LOG_INFO, "👂 SmartNet sniffing NMEA2000 bus (listen-only)");
        return true;
    }

    /* uint8_t suggestedAddress()
     {
         if (strncmp(mqttPrefix, "swb", 3) == 0)
             return SMARTNET_ADDR_SMARTWIRING;
         if (strncmp(mqttPrefix, "rel", 3) == 0)
             return SMARTNET_ADDR_RELAY_MODULE;
         if (strncmp(mqttPrefix, "stc", 3) == 0 || strncmp(mqttPrefix, "ui", 2) == 0)
             return SMARTNET_ADDR_UI_MODULE;
         if (strncmp(mqttPrefix, "sns", 3) == 0 || strncmp(mqttPrefix, "sen", 3) == 0)
             return SMARTNET_ADDR_SENSOR_MODULE;
         if (strncmp(mqttPrefix, "dev", 3) == 0)
             return SMARTNET_ADDR_DEV_TEST;

         return SMARTNET_ADDR_DEV_TEST;
     }*/

    /*void initializeAddress()
    {
        uint8_t storedAddress = SmartCore_EEPROM::readSmartNetAddress();

        sendIdentityRequest();
        waitForAssignment(); // ← implement logic to block for PGN_ASSIGN_ADDRESS
        if (storedAddress != SMARTNET_ADDR_UNASSIGNED && !testAddressConflict(storedAddress))
        {
            smartNetAddress = storedAddress;
        }
        else
        {
            uint8_t base = suggestedAddress();
            smartNetAddress = findFreeAddressNear(base); // ← implement scan function
            SmartCore_EEPROM::writeSmartNetAddress(smartNetAddress);
        }

        logMessage(LOG_INFO, "✅ SmartNet address assigned:" + String(smartNetAddress));
    }*/

    /*bool sendMessage(uint32_t id, const uint8_t *data, uint8_t len)
    {
        if (len > 8)
            return false;

        twai_message_t message;
        message.identifier = id;
        message.data_length_code = len;
        message.flags = TWAI_MSG_FLAG_EXTD;
        memcpy(message.data, data, len);

        return twai_transmit(&message, pdMS_TO_TICKS(100)) == ESP_OK;
    }*/

    /*void handleIncoming()
    {
        twai_message_t message;
        if (twai_receive(&message, pdMS_TO_TICKS(10)) == ESP_OK)
        {
            logMessage(LOG_INFO, "📥 SmartNet RX: ID=0x" + String(message.identifier, HEX) + " LEN=" + String(message.data_length_code));
            parseMessage(message.identifier, message.data, message.data_length_code);
        }
    }*/

    uint32_t extractPGN(uint32_t canId)
    {
        uint8_t dp = (canId >> 24) & 0x01;
        uint8_t pf = (canId >> 16) & 0xFF;
        uint8_t ps = (canId >> 8) & 0xFF;

        if (pf < 240)
            return (dp << 16) | (pf << 8);
        else
            return (dp << 16) | (pf << 8) | ps;
    }

    /*void parseMessage(uint32_t id, const uint8_t *data, uint8_t len)
    {
        uint32_t pgn = extractPGN(id);

        logMessage(LOG_INFO, "📥 Received PGN: " + String(pgn));
        handlePGN(pgn, data, len);
    }*/

    /*void handlePGN(uint32_t pgn, const uint8_t *data, uint8_t len)
    {
        PGN pgnEnum = static_cast<PGN>(pgn);
        logMessage(LOG_INFO, "📦 PGN " + String(pgn) + " (" + String(getPGNName(pgnEnum)) + ")");

        switch (pgnEnum)
        {
        case PGN_HEARTBEAT:
            handleHeartbeat(data, len);
            break;
        case PGN_PRODUCT_INFO:
            handleProductInfo(data, len);
            break;
        case PGN_MODULE_IDENTITY:
            handleModuleIdentity(data, len);
            break;
        case PGN_SYSTEM_STATUS:
            handleSystemStatus(data, len);
            break;
        case PGN_CUSTOM_MESSAGE:
            handleCustomMessage(data, len);
            break;
        default:
            Serial.printf("❓ Unknown PGN: %lu\n", pgn);
            break;
        }
    }*/

    /*void sendHeartbeat()
    {
        uint8_t data[8] = {smartNetAddress, 0x00, 0x00, 0x00, 0, 0, 0, 0};
        sendCANMessage(PGN_HEARTBEAT, data, 8);
    }*/

    /*void sendModuleIdentity()
    {
        uint8_t data[8] = {0x01, 0x02, 0x03, 'S', 'M', 'N', 0x00, 0x00};
        sendCANMessage(PGN_MODULE_IDENTITY, data, 8);
    }*/

    /*void sendCANMessage(uint32_t id, const uint8_t *data, uint8_t len)
    {
        logMessage(LOG_INFO, "📤 Sending PGN " + String(id) + ", len: " + String(len));
        sendMessage(id, data, len);
    }*/

    // === PGN Handlers ===
    /*void handleHeartbeat(const uint8_t *data, uint8_t len)
    {
        if (len < 5)
            return;

        uint8_t sender = data[0];
        uint32_t uptime = data[1] | (data[2] << 8) | (data[3] << 16);
        uint8_t flags = data[4];

        logMessage(LOG_INFO, "❤️ Heartbeat from ID " + String(sender) + " | Uptime: " + String(uptime) + "s | Flags: 0x" + String(flags, HEX));
    }*/

    /*void handleProductInfo(const uint8_t *data, uint8_t len)
    {
        logMessage(LOG_INFO, "ℹ️ Received Product Info");
        // TODO: parse model, firmware, etc.
    }

    void handleModuleIdentity(const uint8_t *data, uint8_t len)
    {
        logMessage(LOG_INFO, "🆔 Received Module Identity");
        // TODO: parse identity details
    }

    void handleSystemStatus(const uint8_t *data, uint8_t len)
    {
        logMessage(LOG_INFO, "📊 Received System Status");
    }

    void handleCustomMessage(const uint8_t *data, uint8_t len)
    {
        logMessage(LOG_INFO, "📬 Received Custom Message");
    }

    void sendProductInfo()
    {
        uint8_t data[8];
        data[0] = SMARTBOAT_MANUFACTURER_ID & 0xFF;
        data[1] = (SMARTBOAT_MANUFACTURER_ID >> 8) & 0xFF;
        data[2] = SMARTBOAT_HW_VERSION;

        uint8_t fwMajor = 0, fwMinor = 0;
        getFirmwareVersion(fwMajor, fwMinor);

        data[3] = fwMajor;
        data[4] = fwMinor;

        for (int i = 0; i < 3; ++i)
        {
            data[5 + i] = serialNumber[i];
        }

        sendCANMessage(PGN_PRODUCT_INFO, data, 8);
        logMessage(LOG_INFO, "📤 Sent Product Info - MFG: " + String(SMARTBOAT_MANUFACTURER_ID) +
                                 ", HW: " + String(SMARTBOAT_HW_VERSION) +
                                 ", FW: " + String(fwMajor) + "." + String(fwMinor));
    }*/

    /*void getFirmwareVersion(uint8_t &major, uint8_t &minor)
    {
        const char *fw = FW_VER;
        while (*fw && !isdigit(*fw))
            ++fw;
        if (*fw)
        {
            major = atoi(fw);
            const char *dot = strchr(fw, '.');
            if (dot && isdigit(*(dot + 1)))
            {
                minor = atoi(dot + 1);
            }
        }
    }*/

    // === TODO: Provide the following ===

    /*void sendIdentityRequest()
    {
        // Broadcast PGN_MODULE_IDENTITY with our serialNumber
        // Node-RED/SmartBox will assign an address
    }

    void waitForAssignment()
    {
        // Wait for PGN_ASSIGN_ADDRESS → update smartNetAddress
    }

    bool testAddressConflict(uint8_t addressToTest)
    {
        // Broadcast something like a Heartbeat with your test address
        // Then listen briefly to see if another heartbeat from the same ID is seen
        // Simulate with a short window of listening

        sendTestPing(addressToTest);

        unsigned long start = millis();
        while (millis() - start < 200)
        {
            twai_message_t msg;
            if (twai_receive(&msg, pdMS_TO_TICKS(10)) == ESP_OK)
            {
                if (msg.identifier == addressToTest)
                {
                    logMessage(LOG_WARN, "⚠️ Address conflict detected.");
                    return true;
                }
            }
        }

        return false; // no conflict
    }

    void sendTestPing(uint8_t testAddr)
    {
        uint8_t data[1] = {0x01}; // Basic byte to simulate ping
        sendCANMessage((testAddr << 8) | (PGN_HEARTBEAT & 0xFF), data, 1);
    }

    uint8_t findFreeAddressNear(uint8_t baseAddr)
    {
        for (uint8_t offset = 0; offset < 5; ++offset)
        {
            uint8_t candidate = baseAddr + offset;
            if (!testAddressConflict(candidate))
            {
                return candidate;
            }
        }

        logMessage(LOG_INFO, "⚠️ No free SmartNet address found nearby, falling back.");
        return SMARTNET_ADDR_DEV_TEST;
    }*/

    void smartNetTask(void *pvParameters)
    {
        uint32_t lastHeartbeat = millis(); // ⏱️ Start from now to avoid instant send

        while (true)
        {
            // handleIncoming();     // 📥 Process incoming CAN messages

            if (millis() - lastHeartbeat >= 1000)
            {
                // sendHeartbeat();  // ❤️ Send heartbeat every second

                lastHeartbeat = millis();
            }

            sniffBus();

            vTaskDelay(pdMS_TO_TICKS(10)); // ⚖️ Yield to scheduler
        }
    }

    void sniffBus()
    {
        twai_message_t msg;

        if (twai_receive(&msg, pdMS_TO_TICKS(10)) != ESP_OK)
            return;

        uint32_t id = msg.identifier;
        uint8_t src = id & 0xFF;
        uint32_t pgn = extractPGN(id);

        dispatchPGN(pgn, src, msg.data, msg.data_length_code);
    }

    void decodeWaterDepth(uint8_t src, const uint8_t *data, uint8_t len)
    {
        if (len < 4)
            return;

        uint16_t rawDepth = data[1] | (data[2] << 8);
        if (rawDepth == 0xFFFF)
            return;

        float depthMeters = rawDepth * 0.01f;

        publishField(
            128267,
            "Water Depth",
            src,
            "depth",
            depthMeters,
            "m");
    }

    void decodeVesselHeading(uint8_t src, const uint8_t *data, uint8_t len)
    {
        if (len < 4)
            return;

        uint16_t rawHeading = data[1] | (data[2] << 8);
        if (rawHeading == 0xFFFF)
            return;

        float headingRad = rawHeading * 0.0001f;

        publishField(
            127250,
            "Vessel Heading",
            src,
            "heading",
            headingRad,
            "rad");
    }

    void decodeAttitude(uint8_t src, const uint8_t *data, uint8_t len)
    {
        if (len < 5)
            return;

        int16_t rawPitch = data[1] | (data[2] << 8);
        int16_t rawRoll = data[3] | (data[4] << 8);

        if (rawPitch != 0x7FFF)
        {
            float pitchRad = rawPitch * 0.0001f;
            publishField(127237, "Attitude", src, "pitch", pitchRad, "rad");
        }

        if (rawRoll != 0x7FFF)
        {
            float rollRad = rawRoll * 0.0001f;
            publishField(127237, "Attitude", src, "roll", rollRad, "rad");
        }
    }

    void decodeRudder(uint8_t src, const uint8_t *data, uint8_t len)
    {
        if (len < 3)
            return;

        int16_t rawAngle = data[1] | (data[2] << 8);
        if (rawAngle == 0x7FFF)
            return;

        float rudderRad = rawAngle * 0.0001f;

        publishField(
            127245,
            "Rudder",
            src,
            "rudderAngle",
            rudderRad,
            "rad");
    }

    void decodeRateOfTurn(uint8_t src, const uint8_t *data, uint8_t len)
    {
        if (len < 3)
            return;

        int16_t rawRot = data[1] | (data[2] << 8);
        if (rawRot == 0x7FFF)
            return;

        float rotRadPerSec = rawRot * 0.0001f;

        publishField(
            127251,
            "Rate of Turn",
            src,
            "rateOfTurn",
            rotRadPerSec,
            "rad/s");
    }

    void decodeHeave(uint8_t src, const uint8_t *data, uint8_t len)
    {
        if (len < 3)
            return;

        int16_t rawHeave = data[1] | (data[2] << 8);
        if (rawHeave == 0x7FFF)
            return;

        float heaveMeters = rawHeave * 0.01f;

        publishField(
            127252,
            "Heave",
            src,
            "heave",
            heaveMeters,
            "m");
    }

    void decodeSpeed(uint8_t src, const uint8_t *data, uint8_t len)
    {
        if (len < 5)
            return;

        uint16_t rawSTW = data[1] | (data[2] << 8);
        uint16_t rawSOG = data[3] | (data[4] << 8);

        if (rawSTW != 0xFFFF)
        {
            float stw = rawSTW * 0.01f;
            publishField(128259, "Speed", src, "speedThroughWater", stw, "m/s");
        }

        if (rawSOG != 0xFFFF)
        {
            float sog = rawSOG * 0.01f;
            publishField(128259, "Speed", src, "speedOverGround", sog, "m/s");
        }
    }

    void decodeEnvironmental(uint8_t src, const uint8_t *data, uint8_t len)
    {
        if (len < 8)
            return;

        int16_t rawTemp = data[1] | (data[2] << 8);
        uint16_t rawPressure = data[3] | (data[4] << 8);
        uint16_t rawHumidity = data[5] | (data[6] << 8);

        if (rawTemp != 0x7FFF)
        {
            float tempC = rawTemp * 0.01f;
            publishField(130311, "Environmental", src, "temperature", tempC, "C");
        }

        if (rawPressure != 0xFFFF)
        {
            float pressurePa = rawPressure * 1.0f;
            publishField(130311, "Environmental", src, "pressure", pressurePa, "Pa");
        }

        if (rawHumidity != 0xFFFF)
        {
            float humidityPct = rawHumidity * 0.004f;
            publishField(130311, "Environmental", src, "humidity", humidityPct, "%");
        }
    }

    void dispatchPGN(uint32_t pgn, uint8_t src, const uint8_t *data, uint8_t len)
    {
        switch (pgn)
        {
        // --------------------------------------------------
        // NAVIGATION / MOTION
        // --------------------------------------------------
        case 127237: // Attitude
            decodeAttitude(src, data, len);
            break;

        case 127245: // Rudder
            decodeRudder(src, data, len);
            break;

        case 127250: // Vessel Heading
            decodeVesselHeading(src, data, len);
            break;

        case 127251: // Rate of Turn
            decodeRateOfTurn(src, data, len);
            break;

        case 127252: // Heave
            decodeHeave(src, data, len);
            break;

        /*case 127257: // Attitude (extended motion)
            decodeExtendedAttitude(src, data, len);
            break;*/

        // --------------------------------------------------
        // ENVIRONMENT
        // --------------------------------------------------
        case 128259: // Speed (STW / SOG)
            decodeSpeed(src, data, len);
            break;

        case 130311: // Environmental parameters
            decodeEnvironmental(src, data, len);
            break;

        case 128267: // Water Depth
            decodeWaterDepth(src, data, len);
            break;

        // --------------------------------------------------
        // STATE / DIGITAL
        // --------------------------------------------------
        /*case 127501: // Binary Switch Bank Status
            decodeSwitchBank(src, data, len);
            break;*/

        // --------------------------------------------------
        // PROPRIETARY / FAST PACKET (STUBS)
        // --------------------------------------------------
        case 130820: // Proprietary / fast-packet
        case 65341:
        case 65305:
            logMessage(LOG_INFO,
                       "📦 Fast-packet PGN seen: " + String(pgn) + " src=" + String(src));
            // TODO: fast-packet or manufacturer specific
            // Explicitly acknowledged but not decoded yet
            break;

        // --------------------------------------------------
        // EVERYTHING ELSE
        // --------------------------------------------------
        default:
            // Known unknowns get ignored cleanly
            break;
        }
    }

    void publishField(
        uint32_t pgn,
        const char *pgnName,
        uint8_t src,
        const char *field,
        float value,
        const char *units)
    {
        DynamicJsonDocument doc(256);

        doc["bus"] = "nmea2000";
        doc["pgn"] = pgn;
        doc["pgnName"] = pgnName;
        doc["source"] = src;
        doc["field"] = field;
        doc["value"] = value;
        doc["units"] = units;
        doc["timestamp"] = millis(); // monotonic, safe

        String payload;
        serializeJson(doc, payload);

        const char *topic = "smartnet/data";

        if (mqttClient && mqttClient->connected())
        {
            mqttClient->publish(topic, 1, true, payload.c_str());
            logMessage(LOG_INFO, "📤 smartnet/data → " + payload);
        }
        else
        {
            logMessage(LOG_WARN, "❌ MQTT not connected — dropping smartnet/data");
        }
    }

}


#endif // SMARTBOX_BUILD
   