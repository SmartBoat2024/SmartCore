#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

void handleModuleSpecificConfig(const JsonObject& doc);
void handleModuleSpecificModule(const JsonObject& doc);
void handleModuleSpecificErrors(const String& message);