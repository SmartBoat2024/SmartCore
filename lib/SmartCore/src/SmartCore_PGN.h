#pragma once

#include <stdint.h>

// ✅ Enumeration of supported PGNs
enum PGN : uint32_t {
    PGN_HEARTBEAT         = 126993,  // Module heartbeat/status
    PGN_PRODUCT_INFO      = 126996,  // Firmware & product identification
    PGN_MODULE_IDENTITY   = 130000,  // Serial number & SmartBox identification
    PGN_SYSTEM_STATUS     = 130001,  // Temperature, voltages, etc.
    PGN_CUSTOM_MESSAGE    = 130004,  // General-purpose SmartNet message
    // Add more PGNs as your SmartNet protocol expands
};

// ✅ PGN name resolution (for debugging/logs)
inline const char* getPGNName(PGN pgn) {
    switch (pgn) {
        case PGN_HEARTBEAT:         return "Heartbeat";
        case PGN_PRODUCT_INFO:      return "Product Info";
        case PGN_MODULE_IDENTITY:   return "Module Identity";
        case PGN_SYSTEM_STATUS:     return "System Status";
        case PGN_CUSTOM_MESSAGE:    return "Custom Message";
        default:                    return "Unknown PGN";
    }
}