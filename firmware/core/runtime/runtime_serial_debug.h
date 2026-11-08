#pragma once

#include <Arduino.h>

void processRuntimeSerialCommands();
// True when a binary stream may not start; the refusal goes to out.
bool runtimeSerialRefusesStream(Print& out);
void runtimeSerialWriteAuthSidecarStatus(Print& out);
void runtimeSerialWriteXboxOneAuthTrace(Print& out, bool clear);
