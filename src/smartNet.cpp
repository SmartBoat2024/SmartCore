#include "SmartCore_SmartNet.h"
#include <arduino.h>
#include "SmartCore_Log.h"
#include "pgn.h"


bool handleModuleSpecificPGN(uint32_t pgn, const uint8_t* data, uint8_t len) {
    logMessage(LOG_INFO, "📦 Module PGN " + String(pgn) + " (" + String(getModulePGNName((ModulePGN)pgn)) + ")");
    switch (pgn) {
        //case PGN_TANK_LEVEL_UPDATE:
            // do something
           // return true;
        //case PGN_SET_SENSOR_INTERVAL:
            // update interval
            //return true;
        default:
            return false;
    }
}

void sendDataViaSmartNet(uint32_t id, const uint8_t* data, uint8_t len){
    SmartCore_SmartNet::sendCANMessage(id, data, len);
    logMessage(LOG_INFO, "📤 Sending module specific PGN " + String(id) + ", len: " + String(len));
}