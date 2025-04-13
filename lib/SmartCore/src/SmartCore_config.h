#pragma once

struct SmartCoreSettings {
    const char* serialNumber;
    const char* moduleName;
};

#ifndef WEBSERVER_ENABLED
#define WEBSERVER_ENABLED true  // Default if not overridden in main.cpp
#endif