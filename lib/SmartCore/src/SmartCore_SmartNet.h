#pragma once

#include<arduino.h>
#include <stdint.h>

// Address Ranges (0x00–0x7F = safe for your system)
#define SMARTNET_ADDR_SMARTBOX         0x01  // Central SmartBox
#define SMARTNET_ADDR_UI_MODULE        0x10  // Touchscreens, keypads
#define SMARTNET_ADDR_RELAY_MODULE     0x20  // Relay modules
#define SMARTNET_ADDR_SENSOR_MODULE    0x30  // Sensors
#define SMARTNET_ADDR_SMARTWIRING      0x40  // Wiring hubs
#define SMARTNET_ADDR_DEV_TEST         0x7E  // Dev/test
#define SMARTNET_ADDR_UNASSIGNED       0x7F  // Not yet assigned

// SmartNet CAN TX/RX pins 
#ifndef SMARTNET_CAN_TX
#define SMARTNET_CAN_TX GPIO_NUM_11
#endif
#ifndef SMARTNET_CAN_RX
#define SMARTNET_CAN_RX GPIO_NUM_12
#endif

#define SMARTBOAT_MANUFACTURER_ID 2025

namespace SmartCore_SmartNet {

    extern TaskHandle_t smartNetTaskHandle;

    // Startup and setup
    bool initSmartNet();
    void initializeAddress();
    uint8_t suggestedAddress();

    // Communication
    bool sendMessage(uint32_t id, const uint8_t* data, uint8_t len);
    void sendCANMessage(uint32_t id, const uint8_t* data, uint8_t len);

    // Incoming
    void handleIncoming();
    void parseMessage(uint32_t id, const uint8_t* data, uint8_t len);
    void handlePGN(uint32_t pgn, const uint8_t* data, uint8_t len);

    // PGN handlers
    void handleHeartbeat(const uint8_t* data, uint8_t len);
    void handleProductInfo(const uint8_t* data, uint8_t len);
    void handleModuleIdentity(const uint8_t* data, uint8_t len);
    void handleSystemStatus(const uint8_t* data, uint8_t len);
    void handleCustomMessage(const uint8_t* data, uint8_t len);

    // Responses
    void sendHeartbeat();
    void sendModuleIdentity();
    void sendProductInfo();
    void getFirmwareVersion(uint8_t& major, uint8_t& minor);

    // SmartNet address negotiation
    bool testAddressConflict(uint8_t addressToTest);
    void sendTestPing(uint8_t testAddr);
    uint8_t findFreeAddressNear(uint8_t baseAddr);
    void sendIdentityRequest();
    void waitForAssignment();

    // smartnet task
    void smartNetTask(void* pvParameters);

} // namespace
