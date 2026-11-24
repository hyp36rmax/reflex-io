#pragma once

#include <stdint.h>

#include "../core/controller_state.h"
#include "menu_dpad_graphics.h"
#include "oled_pixel_surface.h"

// HKT-7500 physical input display. Only full-width Home panel pages 3-7
// are owned here; product logo and input/output status stay unchanged.
namespace dreamcast_twin_stick_home {

enum : uint16_t {
  LeftUp = 1u << 0, LeftDown = 1u << 1, LeftLeft = 1u << 2, LeftRight = 1u << 3,
  RightUp = 1u << 4, RightDown = 1u << 5, RightLeft = 1u << 6, RightRight = 1u << 7,
  LeftTrigger = 1u << 8, LeftTop = 1u << 9,
  RightTrigger = 1u << 10, RightTop = 1u << 11,
  Pause = 1u << 12, Start = 1u << 13
};
constexpr uint16_t kLeftDirections = LeftUp | LeftDown | LeftLeft | LeftRight;
constexpr uint16_t kRightDirections = RightUp | RightDown | RightLeft | RightRight;

// Physical Maple input, independent of XInput/DInput remapping.
inline uint16_t read(const controller_state_t& frame) {
  uint16_t mask = 0;
  if (frame.PAD_U) mask |= LeftUp;
  if (frame.PAD_D) mask |= LeftDown;
  if (frame.PAD_L) mask |= LeftLeft;
  if (frame.PAD_R) mask |= LeftRight;
  if (frame.EXTRA & (1u << 0)) mask |= RightUp;
  if (frame.EXTRA & (1u << 1)) mask |= RightDown;
  if (frame.EXTRA & (1u << 2)) mask |= RightLeft;
  if (frame.EXTRA & (1u << 3)) mask |= RightRight;
  if (frame.L2) mask |= LeftTrigger;
  if (frame.L1) mask |= LeftTop;
  if (frame.R2) mask |= RightTrigger;
  if (frame.R1) mask |= RightTop;
  if (frame.EXTRA & (1u << 4)) mask |= Pause;
  if (frame.START) mask |= Start;
  return mask;
}

template <typename Display>
void drawCircle(Display& display, uint8_t x, bool pressed) {
  static constexpr uint8_t kOutline[7] = {0x1C, 0x22, 0x41, 0x41, 0x41, 0x22, 0x1C};
  static constexpr uint8_t kFilled[7] = {0x1C, 0x3E, 0x7F, 0x7F, 0x7F, 0x3E, 0x1C};
  display.setCursor(x, 3);
  for (uint8_t i = 0; i < 7; ++i)
    display.ssd1306WriteRamBuf(pressed ? kFilled[i] : kOutline[i]);
}

template <typename Display>
void drawCenterButton(Display& display, uint8_t x, uint8_t page,
                      bool pressed, bool isStart) {
  // P / S distinguish Pause from Start even on an unpressed monochrome OLED.
  static constexpr uint8_t kP[5] = {0x7F, 0x09, 0x09, 0x09, 0x06};
  static constexpr uint8_t kS[5] = {0x46, 0x49, 0x49, 0x49, 0x31};
  const uint8_t* letter = isStart ? kS : kP;
  display.setCursor(x, page);
  for (uint8_t i = 0; i < 11; ++i) {
    uint8_t bits = pressed ? 0x7F : 0x00;
    if (i == 0 || i == 10)
      bits = pressed ? 0x7F : 0x3E;
    else if (i >= 3 && i < 8)
      bits = pressed ? uint8_t(0x7F & ~letter[i - 3]) : letter[i - 3];
    display.ssd1306WriteRamBuf(bits);
  }
}

template <typename Display>
void draw(Display& display, uint16_t state, uint16_t previous, bool force) {
  if (force) {
    // Wipe any previous single/dual pad content, including the bottom row.
    display.clear(0, 127, 3, 7);
  }
  oled_pixel::Origin origin(oled_pixel::centerOffset(24, 55));
  if (force || ((state ^ previous) & kLeftDirections) != 0) {
    pad_dpad_graphics::draw(display, 18, 4,
        (state & LeftUp) != 0, (state & LeftDown) != 0,
        (state & LeftLeft) != 0, (state & LeftRight) != 0);
  }
  if (force || ((state ^ previous) & kRightDirections) != 0) {
    pad_dpad_graphics::draw(display, 92, 4,
        (state & RightUp) != 0, (state & RightDown) != 0,
        (state & RightLeft) != 0, (state & RightRight) != 0);
  }
  constexpr uint16_t upper[] = {LeftTrigger, LeftTop, RightTrigger, RightTop};
  constexpr uint8_t positions[] = {14, 30, 92, 108};
  for (uint8_t i = 0; i < 4; ++i) {
    if (force || ((state ^ previous) & upper[i]) != 0)
      drawCircle(display, positions[i], (state & upper[i]) != 0);
  }
  if (force || ((state ^ previous) & Pause) != 0)
    drawCenterButton(display, 51, 5, (state & Pause) != 0, false);
  if (force || ((state ^ previous) & Start) != 0)
    drawCenterButton(display, 67, 5, (state & Start) != 0, true);
}

} // namespace dreamcast_twin_stick_home
