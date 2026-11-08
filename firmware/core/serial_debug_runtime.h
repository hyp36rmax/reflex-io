#pragma once

#include <Arduino.h>

void serialDebugRuntimeSetup();
void serialDebugRuntimeTask();

// Common transport-independent command dispatcher used by UART and USB CDC.
// Transport-specific diagnostics may run before this shared command surface.
bool handleSerialDebugCommand(const char* command, Print& out);

// Rollback-safe signed RP2040 update stream shared by native USB CDC and the
// board management UART/CH340 connection.
bool handleCdcFirmwareUpdateSerialCommand(const char* command, Print& out);
