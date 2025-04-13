#include <Arduino.h>

enum ModulePGN : uint32_t {
    //Start at 131000
    //PGN_TANK_LEVEL_UPDATE    = 131000,
    //PGN_SET_SENSOR_INTERVAL  = 131001,
    //PGN_RELAY_TRIGGER        = 131002,
    // ... add as needed
};

inline const char* getModulePGNName(ModulePGN pgn) {
    switch (pgn) {
        //case PGN_TANK_LEVEL_UPDATE:   return "Tank Level Update";
        //case PGN_SET_SENSOR_INTERVAL:return "Set Sensor Interval";
        //case PGN_RELAY_TRIGGER:       return "Relay Trigger";
        default:                      return "Unknown Module PGN";
    }
}