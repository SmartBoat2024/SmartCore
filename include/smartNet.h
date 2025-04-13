#pragma once
#include <arduino.h>

bool handleModuleSpecificPGN(uint32_t pgn, const uint8_t* data, uint8_t len);
void sendDataViaSmartNet(uint32_t id, const uint8_t* data, uint8_t len)