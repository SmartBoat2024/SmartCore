#include "SmartCore_MCP.h"
#include <Wire.h>

namespace SmartCore_MCP {
    static MCP23008 mcp(MCP23008_ADDRESS, &Wire);

    void init() {
        mcp.begin();  // No Wire.begin() here — let SmartCore_I2C handle it
        Serial.println("🔌 MCP23008 initialized");
    }

    void setPinMode(uint8_t pin, uint8_t mode) {
        mcp.pinMode1(pin, mode);
    }

    void digitalWrite(uint8_t pin, bool value) {
        mcp.write1(pin, value ? HIGH : LOW);
    }

    bool digitalRead(uint8_t pin) {
        return mcp.read1(pin);
    }

    void writePort(uint8_t value) {
        mcp.write8(value);
    }

    uint8_t readPort() {
        return mcp.read8();
    }

    void printPortState() {
        uint8_t portValue = mcp.read8();
        Serial.print("📊 MCP Port State: 0b");
        for (int i = 7; i >= 0; i--) {
            Serial.print(bitRead(portValue, i));
        }
        Serial.println();
    }
}
