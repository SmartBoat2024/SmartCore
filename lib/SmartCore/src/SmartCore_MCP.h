#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "MCP23008.h"

#define MCP23008_ADDRESS 0x20

namespace SmartCore_MCP {
    void init();
    void setPinMode(uint8_t pin, uint8_t mode);
    void digitalWrite(uint8_t pin, bool value);
    bool digitalRead(uint8_t pin);
    void writePort(uint8_t value);
    uint8_t readPort();
    void printPortState();
}