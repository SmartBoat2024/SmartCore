#include "SmartCore_Time.h"
#include "SmartCore_Network.h"  // for smartBoatEnabled, smartBoatEpoch, smartBoatEpochSyncMillis

uint32_t getCurrentSmartBoatTime() {
    if (smartBoatEnabled && smartBoatEpoch > 0) {
        uint32_t deltaMs = millis() - smartBoatEpochSyncMillis;
        Serial.printf("[DEBUG] SmartBoatTime = %lu + (%lu - %lu)/1000 = %lu\n",
                      smartBoatEpoch, millis(), smartBoatEpochSyncMillis,
                      smartBoatEpoch + deltaMs / 1000);
        return smartBoatEpoch + (deltaMs / 1000);
    }
    return millis() / 1000;
}