#pragma once

#include <stdint.h>

// PC Engine pads blank D0-D3 while CLR is high (the 74HC157 enable), and a
// still mouse reads 0000 there too, so that read finds a mouse. Nothing
// documents the mouse blanking while it moves or clicks, and the bench mouse
// fails that read then (2026-11-15). Once a mouse is recognised it is kept
// until the bus looks unplugged: every line high on every read for this long.
constexpr uint32_t kPceMouseOpenBusDropMs = 300;

// An unplugged port reads all lines high through the pull-ups. A mouse moving
// exactly -1/-1 with no button held reads the same; that sample is dropped.
inline bool pceMouseSweepOpenBus(uint8_t rawNibble, const uint8_t data[4],
                                 const uint8_t buttons[4]) {
  if (rawNibble != 0x0F) return false;
  for (uint8_t i = 0; i < 4; ++i) {
    if (data[i] != 0x0F || buttons[i] != 0x0F) return false;
  }
  return true;
}

struct PceMousePresence {
  uint32_t openSinceMs = 0;
  bool open = false;

  void reset() {
    openSinceMs = 0;
    open = false;
  }

  // For a recognised mouse: true while it should stay connected.
  bool present(bool openBus, uint32_t nowMs) {
    if (!openBus) {
      reset();
      return true;
    }
    if (!open) {
      open = true;
      openSinceMs = nowMs;
      return true;
    }
    return uint32_t(nowMs - openSinceMs) < kPceMouseOpenBusDropMs;
  }
};
