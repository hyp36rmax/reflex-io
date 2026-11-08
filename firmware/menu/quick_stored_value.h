#pragma once

#include <stdint.h>

// A Quick Menu value clamped to what the current pad and output can show. A
// row nobody edited saves what the record held, so plugging in a digital pad
// and saving an unrelated row does not erase the analog pad's settings.
struct QuickStoredValue {
  uint8_t stored = 0;
  uint8_t shown = 0;

  uint8_t saved(uint8_t edited) const {
    return edited == shown ? stored : edited;
  }
};
