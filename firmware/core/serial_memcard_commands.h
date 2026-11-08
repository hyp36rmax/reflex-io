#pragma once

#include <Arduino.h>

#include "mister_serial_device.h"

bool handleSerialMemcardCommand(char* text, Print& out);

// The cards behind MiSTer Main's request channel, or nullptr when this
// firmware has no MiSTer request support.
mister_serial::CardBackend* misterSerialCardBackend();
