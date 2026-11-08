#pragma once
#include <stdint.h>
#include "../input/shared/input_button_bits.h"

// Apply host actions to the FINAL report, never through an input-specific
// face-button/trigger projection (N64 C buttons, VB right D-pad, Genesis, GC).
// out_usb.h includes report mappers inside its TinyUSB extern "C" block.
extern "C++" {
uint32_t directionalHotkeyOutputButtons(uint8_t port);

template <typename Report> inline void applyDirectionalXinputHotkeys(Report& report, uint32_t buttons) {
  if (buttons & INPUT_A) report.a = 1;
  if (buttons & INPUT_B) report.b = 1;
  if (buttons & INPUT_X) report.x = 1;
  if (buttons & INPUT_Y) report.y = 1;
  if (buttons & INPUT_L1) report.lb = 1;
  if (buttons & INPUT_R1) report.rb = 1;
  if (buttons & INPUT_L3) report.ls = 1;
  if (buttons & INPUT_R3) report.rs = 1;
  if (buttons & INPUT_SELECT) report.back = 1;
  if (buttons & INPUT_L2) report.lt = 255;
  if (buttons & INPUT_R2) report.rt = 255;
}

template <typename Report> inline void applyDirectionalPokkenHotkeys(Report& report, uint32_t buttons) {
  if (buttons & INPUT_A) report.a = 1;
  if (buttons & INPUT_B) report.b = 1;
  if (buttons & INPUT_X) report.x = 1;
  if (buttons & INPUT_Y) report.y = 1;
  if (buttons & INPUT_L1) report.l = 1;
  if (buttons & INPUT_R1) report.r = 1;
  if (buttons & INPUT_L2) report.zl = 1;
  if (buttons & INPUT_R2) report.zr = 1;
  if (buttons & INPUT_L3) report.l3 = 1;
  if (buttons & INPUT_R3) report.r3 = 1;
  if (buttons & INPUT_SELECT) report.select = 1;
  if (buttons & INPUT_CAPTURE) report.capture = 1;
}

template <typename Report> inline void applyDirectionalPs4Hotkeys(Report& report, uint32_t buttons) {
  if (buttons & INPUT_A) report.cross = 1;
  if (buttons & INPUT_B) report.circle = 1;
  if (buttons & INPUT_X) report.square = 1;
  if (buttons & INPUT_Y) report.triangle = 1;
  if (buttons & INPUT_L1) report.l1 = 1;
  if (buttons & INPUT_R1) report.r1 = 1;
  if (buttons & INPUT_L2) report.l2 = 1;
  if (buttons & INPUT_R2) report.r2 = 1;
  if (buttons & INPUT_L3) report.l3 = 1;
  if (buttons & INPUT_R3) report.r3 = 1;
  if (buttons & INPUT_SELECT) report.share = 1;
  if (buttons & INPUT_CAPTURE) report.touch_click = 1;
  if (buttons & INPUT_L2) report.analog_l2 = 255;
  if (buttons & INPUT_R2) report.analog_r2 = 255;
}

template <typename Report> inline void applyDirectionalPs5Hotkeys(Report& report, uint32_t buttons) {
  if (buttons & INPUT_A) report.button_south = 1;
  if (buttons & INPUT_B) report.button_east = 1;
  if (buttons & INPUT_X) report.button_west = 1;
  if (buttons & INPUT_Y) report.button_north = 1;
  if (buttons & INPUT_L1) report.button_l1 = 1;
  if (buttons & INPUT_R1) report.button_r1 = 1;
  if (buttons & INPUT_L2) report.button_l2 = 1;
  if (buttons & INPUT_R2) report.button_r2 = 1;
  if (buttons & INPUT_L3) report.button_l3 = 1;
  if (buttons & INPUT_R3) report.button_r3 = 1;
  if (buttons & INPUT_SELECT) report.button_select = 1;
  if (buttons & INPUT_CAPTURE) report.button_touchpad = 1;
  if (buttons & INPUT_L2) report.left_trigger = 255;
  if (buttons & INPUT_R2) report.right_trigger = 255;
}

template <typename Report> inline void applyDirectionalPs3Hotkeys(Report& report, uint32_t buttons) {
  if (buttons & INPUT_A) report.cross_btn = 1;
  if (buttons & INPUT_B) report.circle_btn = 1;
  if (buttons & INPUT_X) report.square_btn = 1;
  if (buttons & INPUT_Y) report.triangle_btn = 1;
  if (buttons & INPUT_L1) report.l1_btn = 1;
  if (buttons & INPUT_R1) report.r1_btn = 1;
  if (buttons & INPUT_L2) report.l2_btn = 1;
  if (buttons & INPUT_R2) report.r2_btn = 1;
  if (buttons & INPUT_L3) report.l3_btn = 1;
  if (buttons & INPUT_R3) report.r3_btn = 1;
  if (buttons & INPUT_SELECT) report.select_btn = 1;
}

template <typename Report> inline void applyDirectionalPs3PressureHotkeys(Report& report, uint32_t buttons) {
  if (buttons & INPUT_A) report.cross_axis = 255;
  if (buttons & INPUT_B) report.circle_axis = 255;
  if (buttons & INPUT_X) report.square_axis = 255;
  if (buttons & INPUT_Y) report.triangle_axis = 255;
  if (buttons & INPUT_L1) report.l1_axis = 255;
  if (buttons & INPUT_R1) report.r1_axis = 255;
  if (buttons & INPUT_L2) report.l2_axis = 255;
  if (buttons & INPUT_R2) report.r2_axis = 255;
}

template <typename Report> inline void applyDirectionalSwitchHotkeys(Report& report, uint32_t buttons,
                                                                  bool nativeN64 = false) {
  if (buttons & INPUT_A) report.a = 1;
  if (buttons & INPUT_B) report.b = 1;
  // Native N64's host X action is C-Down (Pro ZR wire bit 7). Pro X
  // wire bit 1 is C-Left/host Y on that identity. Only synthesized actions
  // pass here; physical C-button assignments stay in the gameplay mapper.
  if (buttons & INPUT_X) {
    if (nativeN64) report.zr = 1;
    else report.x = 1;
  }
  if (buttons & INPUT_Y) report.y = 1;
  if (buttons & INPUT_L1) report.l = 1;
  if (buttons & INPUT_R1) report.r = 1;
  if (buttons & INPUT_L2) report.zl = 1;
  if (buttons & INPUT_R2) report.zr = 1;
  if (buttons & INPUT_L3) report.l3 = 1;
  if (buttons & INPUT_R3) report.r3 = 1;
  if (buttons & INPUT_CAPTURE) report.capture = 1;
}

template <typename Report> inline void applyDirectionalXidHotkeys(Report& report, uint32_t buttons) {
  if (buttons & INPUT_A) report.A = 255;
  if (buttons & INPUT_B) report.B = 255;
  if (buttons & INPUT_X) report.X = 255;
  if (buttons & INPUT_Y) report.Y = 255;
  if (buttons & INPUT_L1) report.BLACK = 255;
  if (buttons & INPUT_R1) report.WHITE = 255;
  if (buttons & INPUT_L2) report.L = 255;
  if (buttons & INPUT_R2) report.R = 255;
  if (buttons & INPUT_SELECT) report.dButtons |= 1u << 5;
  if (buttons & INPUT_L3) report.dButtons |= 1u << 6;
  if (buttons & INPUT_R3) report.dButtons |= 1u << 7;
}
template <typename Report> inline void applyDirectionalXboxOneHotkeys(Report& report, uint32_t buttons) {
  if (buttons & INPUT_A) report.a = 1;
  if (buttons & INPUT_B) report.b = 1;
  if (buttons & INPUT_X) report.x = 1;
  if (buttons & INPUT_Y) report.y = 1;
  if (buttons & INPUT_L1) report.leftShoulder = 1;
  if (buttons & INPUT_R1) report.rightShoulder = 1;
  if (buttons & INPUT_L3) report.leftThumbClick = 1;
  if (buttons & INPUT_R3) report.rightThumbClick = 1;
  if (buttons & INPUT_SELECT) report.back = 1;
  if (buttons & INPUT_L2) report.leftTrigger = 0x03FF;
  if (buttons & INPUT_R2) report.rightTrigger = 0x03FF;
}
} // extern "C++"
