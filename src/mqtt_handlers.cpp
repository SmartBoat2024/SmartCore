#include "mqtt_handlers.h"
#include <Arduino.h>
#include "SmartCore_MQTT.h"

void handleModuleSpecificConfig(const JsonObject &doc)
{
    Serial.println("📦 [Template] handleModuleSpecificConfig() called.");
    // TODO: Add configuration-specific logic here
}

void handleModuleSpecificModule(const JsonObject &doc)
{
    Serial.println("📦 [Template] handleModuleSpecificModule() called.");

    const char* cmd = doc["cmd"] | "";
    if (!strcmp(cmd, "identify"))
    {
        // blink LEDs, buzzers, etc
    }
}

void handleModuleSpecificErrors(const String &message)
{
    Serial.println("📦 [Template] handleModuleSpecificErrors() called.");
    // TODO: Add error-specific logic here
}