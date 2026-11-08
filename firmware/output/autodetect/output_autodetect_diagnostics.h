#pragma once

#include <Arduino.h>

// Shared by USB CDC status and the independent debug UART so diagnostics do
// not depend on the active USB personality surviving long enough to enumerate.
void appendOutputAutodetectDiagnostics(Print& out);
void printOutputAutodetectDiagnostics(Print& out, const char* phase);
