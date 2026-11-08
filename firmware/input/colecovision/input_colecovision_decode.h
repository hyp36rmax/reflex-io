#pragma once

#include <stdint.h>

#include "../shared/input_button_bits.h"

// ColecoVision controller decoding. A scan reads the four data lines and the
// fire line twice:
// - once with the joystick select (DE9 8) low: joystick and left fire;
// - once with the keypad select (DE9 5) low: keypad and right fire.
// Nibble bits follow MAME coleco/controller/hand.cpp:
//   bit 0 = DE9 1 (Up), bit 1 = DE9 4 (Right),
//   bit 2 = DE9 2 (Down), bit 3 = DE9 3 (Left); 1 = line pulled low.
// The keypad codes and the Super Action Controller's extra buttons (sac.cpp)
// are diode-encoded on the same lines. Any other nibble is two keys at once
// and decodes to nothing.
namespace colecovision_decode {

struct Scan {
  uint8_t joystick;  // nibble read with the joystick select low
  uint8_t keypad;    // nibble read with the keypad select low
  bool leftFire;     // fire line with the joystick select low
  bool rightFire;    // fire line with the keypad select low
};

struct Result {
  uint32_t buttons;
  int16_t lx;
  int16_t ly;
};

constexpr uint8_t kUp = 0x01;
constexpr uint8_t kRight = 0x02;
constexpr uint8_t kDown = 0x04;
constexpr uint8_t kLeft = 0x08;

struct Key {
  uint8_t code;
  uint32_t button;
};

// Keypad layout from the contributed ColecoVision mode: 2/4/6/8 on the D-pad
// (the joystick is the left stick), the rest on shoulders, Select and Start.
// 0x07 and 0x0B are the Super Action Controller's third and fourth buttons.
constexpr Key kKeys[14] = {
  {0x02, INPUT_L1},  {0x08, INPUT_PAD_U}, {0x03, INPUT_R1},
  {0x0D, INPUT_PAD_L}, {0x0C, INPUT_L3},  {0x01, INPUT_PAD_R},
  {0x0A, INPUT_L2},  {0x0E, INPUT_PAD_D}, {0x04, INPUT_R2},
  {0x06, INPUT_SELECT}, {0x05, INPUT_R3}, {0x09, INPUT_START},
  {0x07, INPUT_X},   {0x0B, INPUT_Y},
};

inline Result decode(const Scan& scan) {
  Result result{0, 0, 0};
  for (const Key& key : kKeys) {
    if (scan.keypad == key.code) {
      result.buttons |= key.button;
      break;
    }
  }
  if (scan.leftFire) result.buttons |= INPUT_A;
  if (scan.rightFire) result.buttons |= INPUT_B;
  // An 8-way switch stick; opposite directions cancel.
  const uint8_t stick = scan.joystick;
  if ((stick & kRight) && !(stick & kLeft)) result.lx = 127;
  if ((stick & kLeft) && !(stick & kRight)) result.lx = -127;
  if ((stick & kDown) && !(stick & kUp)) result.ly = 127;
  if ((stick & kUp) && !(stick & kDown)) result.ly = -127;
  return result;
}

}  // namespace colecovision_decode
