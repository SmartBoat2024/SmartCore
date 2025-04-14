#include "websocket_funcs.h"
#include "SmartCore_Log.h"
#include "SmartCore_Webserver.h"
#include "SmartCore_Network.h"

bool handleModuleWebSocketMessage(AsyncWebSocketClient* client, JsonDocument& doc_rx) {
    String type = doc_rx["type"] | "";

    /*if (doc_rx.containsKey("command") && doc_rx["command"] == "setBlockConfiguration") {
        JsonObject blockData = doc_rx["blockData"];
        String requestId = doc_rx["requestId"] | "";

        String blockName = blockData["blockName"].as<String>();
        String location = blockData["location"].as<String>();
        uint8_t i2cAddress = blockData["i2cAddress"].as<uint8_t>();
        bool isBlock8 = blockData["isBlock8"].as<bool>();

        JsonArray fuseArray = blockData["fuseNames"].as<JsonArray>();
        JsonArray alertArray = blockData["alertsEnabled"].as<JsonArray>();
        const uint8_t numFuses = isBlock8 ? 8 : 4;

        if (fuseArray.size() != numFuses || alertArray.size() != numFuses) {
            Serial.printf("❌ Invalid array sizes. Expected %d entries.\n", numFuses);
            SmartCore_Websocket::sendResponseWithRequestId(client, false, "Invalid fuse/alert array length", requestId);
            return;
        }

        String fuseNames[8];
        bool alertsEnabled[8];

        for (uint8_t i = 0; i < numFuses; i++) {
            fuseNames[i] = fuseArray[i].as<String>();
            alertsEnabled[i] = alertArray[i].as<bool>();
        }

        // 🔍 Look up existing block (do NOT delete or reorder)
        Block* existing = nullptr;
        for (Block* block : blocksManager.getBlocks()) {
            if (block->getI2CAddress() == i2cAddress) {
                existing = block;
                break;
            }
        }

        if (existing) {
            Serial.printf("♻️ Updating existing block at 0x%02X\n", i2cAddress);
            existing->setName(blockName);
            existing->setLocation(location);
            existing->setIsBlock8(isBlock8);
            existing->setFuseNames(fuseNames, numFuses);
            existing->setAlertsEnabled(alertsEnabled, numFuses);
            existing->saveBlockToLittleFS();

            sendFuseStateUpdateToWebpageAndMQTT();

            SmartCore_Websocket::sendResponseWithRequestId(client, true, "Block updated", requestId);
        } else {
            Serial.printf("➕ No existing block at 0x%02X. Adding new.\n", i2cAddress);
            bool success = blocksManager.addBlock(
                blockName, serialNumber, location, i2cAddress, isBlock8, fuseNames, alertsEnabled
            );

            if (success) {
                SmartCore_Websocket::sendResponseWithRequestId(client, true, "Block added", requestId);
                blocksManager.saveAllBlocksToLittleFS();
                return true;
            } else {
                SmartCore_Websocket::sendResponseWithRequestId(client, false, "Failed to add block", requestId);
            }
        }

        return;
    }*/

    return false;  // Not handled by this module
}