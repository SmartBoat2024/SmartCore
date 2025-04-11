#include <Arduino.h>
#include <SmartCore.h>

void setup() {
    Serial.begin(115200);
    delay(1000);
    SmartCore::test();
}

void loop() {
    // nothing yet
}