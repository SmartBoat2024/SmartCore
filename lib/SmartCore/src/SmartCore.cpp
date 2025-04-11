// In SmartCore/src/SmartCore.cpp
#include "SmartCore.h"
#include "SmartCoreVersion.h"

void SmartCore::test() {
    Serial.println("✅ SmartCore test function working!");
    Serial.printf("📦 SmartCore version: %s\n", SMARTCORE_VERSION);
}