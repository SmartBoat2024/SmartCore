#pragma once
#include <Arduino.h>
#include "SmartCore_config.h" 

// ===========================
// 2. Firmware and Hardware Details
// ===========================

#define SMARTBOAT_HW_VERSION 1 //module pcb board revision
#define APIKEY "API KEY GOES HERE"  //update with new ota api key
#define SAFE_APIKEY "SAFE API KEY GOES HERE"  //update with safe API key

extern void getModuleSpecificConfig();
