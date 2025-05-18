#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

void handleModuleSpecificConfig(const String& message);
void handleModuleSpecificModule(const String& message);
void handleModuleSpecificErrors(const String& message);