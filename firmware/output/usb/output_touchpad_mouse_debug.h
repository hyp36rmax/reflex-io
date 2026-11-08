#pragma once

#include <stdint.h>

class Print;

// Appends transport-independent relative-mouse telemetry for one source port.
// The caller owns the leading label so this can be embedded in FRAME output.
void appendTouchpadMouseDebug(Print& out, uint8_t port);
