#include "SmartCore_Log.h"

String logLevelToString(LogLevel level) {
    switch (level) {
        case LOG_INFO:  return "ℹ️ INFO";
        case LOG_WARN:  return "⚠️ WARN";
        case LOG_ERROR: return "❌ ERROR";
        default:        return "⁉️ UNKNOWN";
    }
}

void logMessage(LogLevel level, const String& message) {
    String logLine = logLevelToString(level) + ": " + message;
    Serial.println(logLine);

    // 🛰️ TODO: Add MQTT / SmartNet / File logging here in future
}