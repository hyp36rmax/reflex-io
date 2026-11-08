#pragma once

#include "../../core/controller_state.h"

// Stick centering subtracts its offset from the frame on every poll. A module
// that rewrites a frame only when the controller changes puts its decoded
// sticks back on the polls in between, so the offset is never subtracted twice.
struct DecodedSticks {
  int16_t lx = 0;
  int16_t ly = 0;
  int16_t rx = 0;
  int16_t ry = 0;

  void capture(const controller_state_t& frame) {
    lx = frame.LX;
    ly = frame.LY;
    rx = frame.RX;
    ry = frame.RY;
  }

  void restore(controller_state_t& frame) const {
    frame.LX = lx;
    frame.LY = ly;
    frame.RX = rx;
    frame.RY = ry;
  }
};
