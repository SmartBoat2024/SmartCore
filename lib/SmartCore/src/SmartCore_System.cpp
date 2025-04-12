#include "SmartCore_config.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include "SmartCore_System.h"
#include "SmartCore_LED.h"
#include "SmartCore_I2C.h"

namespace SmartCore_System {

    static SmartCoreSettings _settings = {
        .serialNumber = "UNKNOWN",
        .moduleName = "Generic SmartCore"
    };

    void setModuleSettings(const SmartCoreSettings& settings) {
        _settings = settings;
    }

    const SmartCoreSettings& getModuleSettings() {
        return _settings;
    }

    void init() {
        Serial.begin(115200);
        delay(4000);

        if (!EEPROM.begin(1024)) {
            Serial.println("❌ EEPROM.begin failed!");
            while (true) yield();
        }

        if (!LittleFS.begin()) {
            Serial.println("❌ LittleFS mount failed!");
            while (true) yield();
        }

        initSmartCoreLED();
        SmartCore_I2C::init();
    }
}
