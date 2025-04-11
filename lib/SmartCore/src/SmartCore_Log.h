#pragma once

#include <Arduino.h>

// Log levels
enum LogLevel {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
};

// 🧠 Public function to log messages
void logMessage(LogLevel level, const String& message);

// Optional: Fancy formatting if needed
String logLevelToString(LogLevel level);
