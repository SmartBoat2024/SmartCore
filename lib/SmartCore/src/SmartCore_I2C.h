#pragma once

#include <Arduino.h>
#include <Wire.h>

#ifndef I2C_SDA
#define I2C_SDA 8
#endif

#ifndef I2C_SCL
#define I2C_SCL 9
#endif

namespace SmartCore_I2C {
    void init();
    void scan(); 
}