#pragma once

#include <stdint.h>

#include "../shared/input_button_bits.h"

// Intellivision hand controller decoding. Bit i of a state is controller line
// Bi, set while that line is pulled to ground. Codes follow MAME
// bus/intv_ctrl/handctrl.cpp:
// - the disc uses B0-B4 (16 positions);
// - each side button pulls two of B5-B7;
// - each key pulls one of B5-B7 and one of B0-B3.
// The controller ORs whatever is held, so some combinations cannot be told
// apart; those decode to neutral rather than to a wrong input.
namespace intv_decode {

struct Result {
  uint32_t buttons;
  int16_t lx;
  int16_t ly;
};

struct DiscPosition {
  uint8_t code;
  int8_t lx;
  int8_t ly;
};

// Clockwise from north, on a circle of radius 127.
constexpr DiscPosition kDisc[16] = {
  {0x04,    0, -127}, {0x14,   49, -117}, {0x16,   90,  -90}, {0x06,  117,  -49},
  {0x02,  127,    0}, {0x12,  117,   49}, {0x13,   90,   90}, {0x03,   49,  117},
  {0x01,    0,  127}, {0x11,  -49,  117}, {0x19,  -90,   90}, {0x09, -117,   49},
  {0x08, -127,    0}, {0x18, -117,  -49}, {0x1C,  -90,  -90}, {0x0C,  -49, -117},
};

struct Key {
  uint8_t code;
  uint32_t button;
};

// Keypad layout from the contributed Intellivision mode: 2/4/6/8 on the
// D-pad (the disc is the left stick), the rest on shoulders, Select and Start.
constexpr Key kKeys[12] = {
  {0x81, INPUT_L1},  {0x41, INPUT_PAD_U}, {0x21, INPUT_R1},
  {0x82, INPUT_PAD_L}, {0x42, INPUT_L3},  {0x22, INPUT_PAD_R},
  {0x84, INPUT_L2},  {0x44, INPUT_PAD_D}, {0x24, INPUT_R2},
  {0x88, INPUT_SELECT}, {0x48, INPUT_R3}, {0x28, INPUT_START},
};

constexpr uint8_t kSideTop = 0xA0;
constexpr uint8_t kSideLeft = 0x60;
constexpr uint8_t kSideRight = 0xC0;

inline Result decode(uint8_t state) {
  for (const Key& key : kKeys) {
    if (state == key.code) return {key.button, 0, 0};
  }
  uint32_t buttons = 0;
  switch (state & 0xE0) {
    case 0x00: break;
    case kSideTop: buttons = INPUT_A; break;
    case kSideLeft: buttons = INPUT_B; break;
    case kSideRight: buttons = INPUT_X; break;
    case 0xE0: break;  // two side buttons: which two is not knowable
    default: return {0, 0, 0};  // a key held with something else
  }
  const uint8_t disc = state & 0x1F;
  for (const DiscPosition& position : kDisc) {
    if (disc == position.code) return {buttons, position.lx, position.ly};
  }
  return {buttons, 0, 0};
}

}  // namespace intv_decode
