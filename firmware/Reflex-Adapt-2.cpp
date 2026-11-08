// Product configuration - must be first (sets ENABLE_INPUT_*, pin defines, etc.)
// Select product via build flags (-DPRODUCT_MODERN2USB) or edit product_config.h
#include "product_config.h"

#include <Arduino.h>

#include "core/runtime/firmware_runtime.h"

void setup() {
  runFirmwareSetup();
}

void loop() {
  runFirmwareLoop();
}
