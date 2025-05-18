#pragma once
#include <Arduino.h>
#include "SmartCore_config.h" 

// ===========================
// 2. Firmware and Hardware Details
// ===========================
#ifndef FW_VER
#define FW_VER "v0.0.1"  // firmware version
#endif

#define SMARTBOAT_HW_VERSION 3  //module pcb board revision
#define APIKEY "85731a76-840c-4d3b-b4d3-51cc58183643"

extern void getModuleSpecificConfig();
