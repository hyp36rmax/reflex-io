#pragma once

#include <Arduino.h>

// Human-readable management commands shared by native USB CDC and the
// optional hardware debug UART. Returns false when the command belongs to
// another serial command family.
bool handleSerialManagementCommand(const char* command, Print& out);
void appendSerialManagementHelp(Print& out);
