#pragma once

#include <Arduino.h>

static inline void prepareProductButtonRead(uint8_t pin) {
#ifdef ADAPT_CAP_JVS_INPUT_STATUS
  // Reassert the dedicated board button pin as a pulled-up GPIO in case any
  // shared runtime path temporarily reconfigured it.
  pinMode(pin, INPUT_PULLUP);
#else
  (void)pin;
#endif
}
