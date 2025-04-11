#include "SmartCore_I2C.h"

void SmartCore_I2C::init() {
    Wire.begin(I2C_SDA, I2C_SCL);
    Serial.printf("🧩 I2C initialized on SDA: %d, SCL: %d\n", I2C_SDA, I2C_SCL);
}

void SmartCore_I2C::scan() {
    Serial.println("🔍 Scanning I2C bus...");

    uint8_t count = 0;
    for (uint8_t address = 1; address < 127; ++address) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.printf("📡 Found device at 0x%02X\n", address);
            count++;
        }
    }

    if (count == 0) {
        Serial.println("🚫 No I2C devices found.");
    } else {
        Serial.printf("✅ Total I2C devices found: %d\n", count);
    }
}