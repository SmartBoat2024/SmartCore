#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

void handleModuleSpecificConfig(const JsonObject& doc);
void handleModuleSpecificModule(const String& message);
void handleModuleSpecificErrors(const String& message);