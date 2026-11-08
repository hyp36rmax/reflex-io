#pragma once

#include <string.h>

#include "../../core/pointer_mouse_config.h"

#ifndef ADAPT_POINTER_MOUSE_REPORT_ID
  #error "Product USB policy must define ADAPT_POINTER_MOUSE_REPORT_ID"
#endif

// menu/menu_ui_state.h, with C++ linkage: out_usb.h includes this inside
// extern "C".
extern "C++" bool menuOwnsControllerInput();

constexpr uint8_t POINTER_MOUSE_REPORT_ID = ADAPT_POINTER_MOUSE_REPORT_ID;
constexpr int32_t TOUCHPAD_MOUSE_COUNTS_PER_PIXEL = 64;
constexpr int32_t POINTER_MOUSE_REPORT_DELTA_MAX = 127;
constexpr int32_t POINTER_MOUSE_PENDING_LIMIT =
    POINTER_MOUSE_REPORT_DELTA_MAX * 4;
constexpr int32_t TOUCHPAD_MOUSE_MAX_SAMPLE_DELTA = 16384;
constexpr uint32_t POINTER_MOUSE_STICK_INTERVAL_MS = 8;

struct PointerMouseRuntimeState {
  bool anchored;
  bool mouse_turn_due;
  uint8_t contact_id;
  uint8_t source;
  uint8_t setting_level;
  uint16_t previous_x;
  uint16_t previous_y;
  int32_t residual_x;
  int32_t residual_y;
  int32_t pending_x;
  int32_t pending_y;
  uint8_t desired_buttons;
  uint8_t sent_buttons;
  int32_t last_sample_dx;
  int32_t last_sample_dy;
  int8_t last_sent_dx;
  int8_t last_sent_dy;
  uint32_t last_stick_sample_ms;
  uint32_t sample_count;
  uint32_t rejected_sample_count;
  uint32_t report_count;
};

PointerMouseRuntimeState pointerMouseState[MAX_USB_OUT] = {};

inline bool pointerMouseHasPending(const PointerMouseRuntimeState& state) {
  return state.pending_x != 0 || state.pending_y != 0 ||
         state.desired_buttons != state.sent_buttons;
}

inline void clearPointerMouseQueuedMotion(PointerMouseRuntimeState& state) {
  state.residual_x = 0;
  state.residual_y = 0;
  state.pending_x = 0;
  state.pending_y = 0;
}

inline int32_t addBoundedPointerMouseDelta(int32_t pending, int32_t delta) {
  const int64_t sum = static_cast<int64_t>(pending) + delta;
  if (sum > POINTER_MOUSE_PENDING_LIMIT) return POINTER_MOUSE_PENDING_LIMIT;
  if (sum < -POINTER_MOUSE_PENDING_LIMIT) return -POINTER_MOUSE_PENDING_LIMIT;
  return static_cast<int32_t>(sum);
}

inline bool touchpadMouseDeltaPlausible(int32_t deltaX, int32_t deltaY) {
  return deltaX >= -TOUCHPAD_MOUSE_MAX_SAMPLE_DELTA &&
         deltaX <= TOUCHPAD_MOUSE_MAX_SAMPLE_DELTA &&
         deltaY >= -TOUCHPAD_MOUSE_MAX_SAMPLE_DELTA &&
         deltaY <= TOUCHPAD_MOUSE_MAX_SAMPLE_DELTA;
}

inline bool pointerMouseControllerHasTouchpad(const controller_state_t& frame) {
  return strcmp(frame.controller_type_name, "DualShock4") == 0 ||
         strcmp(frame.controller_type_name, "DualSense") == 0 ||
         strcmp(frame.controller_type_name, "OctaProPS4") == 0 ||
         strcmp(frame.controller_type_name, "OctaProPS5") == 0;
}

inline PointerMouseSource pointerMouseSource() {
  return pointerMouseConfigSource(adaptPointerMouseConfigValue());
}

inline uint8_t pointerMouseSpeed() {
  return pointerMouseConfigSpeed(adaptPointerMouseConfigValue());
}

inline bool pointerMouseOutputEnabled() {
  if (pointerMouseSource() == POINTER_MOUSE_OFF) return false;
  const outputMode_t mode = get_effective_output_mode();
  return (mode == OUTPUT_HID || mode == OUTPUT_MISTER) &&
         output_allows_management_usb_endpoints(mode);
}

inline bool pointerMouseSourceAvailable(PointerMouseSource source,
                                        const controller_state_t& frame) {
  if (source == POINTER_MOUSE_TOUCHPAD) {
    return pointerMouseControllerHasTouchpad(frame);
  }
  return pointerMouseStickSourceAvailable(
      source, frame.HAS_ANALOG_STICK_MAIN, frame.HAS_ANALOG_STICK_AUX);
}

inline int8_t pointerMouseFrameStickDelta(const controller_state_t& frame,
                                          int16_t axis,
                                          uint8_t speed) {
  return pointerMouseStickDelta(
      convertAnalogPrecision(axis, frame.sticks_precision_bits,
                             ANALOG_STICK_PRECISION_8),
      speed);
}

inline bool observePointerMouse(uint8_t port) {
  if (port >= MAX_USB_OUT) return false;
  PointerMouseRuntimeState& state = pointerMouseState[port];
  const PointerMouseSource source = pointerMouseSource();
  const uint8_t settingLevel = pointerMouseSpeed();
  if (state.source != source || state.setting_level != settingLevel) {
    const uint8_t sentButtons = state.sent_buttons;
    state = PointerMouseRuntimeState{};
    state.sent_buttons = sentButtons;
    state.source = source;
    state.setting_level = settingLevel;
  }

  // Idle passes see the restored frame, so a menu has to stop the pointer here.
  const controller_state_t& frame = controllerFrameConst(port);
  const bool enabled = pointerMouseOutputEnabled() && frame.connected &&
                       pointerMouseSourceAvailable(source, frame) &&
                       !menuOwnsControllerInput();

  if (source == POINTER_MOUSE_LEFT_STICK ||
      source == POINTER_MOUSE_RIGHT_STICK) {
    state.anchored = false;
    state.residual_x = 0;
    state.residual_y = 0;
    const uint32_t held = held_across_menu_close[port];
    state.desired_buttons = enabled
        ? static_cast<uint8_t>((frame.A && !(held & INPUT_A) ? MOUSE_BUTTON_LEFT : 0) |
                               (frame.B && !(held & INPUT_B) ? MOUSE_BUTTON_RIGHT : 0))
        : 0;
    if (!enabled) {
      state.pending_x = 0;
      state.pending_y = 0;
      state.mouse_turn_due = pointerMouseHasPending(state);
      return pointerMouseHasPending(state);
    }

    const uint32_t now = millis();
    if (state.last_stick_sample_ms == 0 ||
        static_cast<uint32_t>(now - state.last_stick_sample_ms) >=
            POINTER_MOUSE_STICK_INTERVAL_MS) {
      const bool right = source == POINTER_MOUSE_RIGHT_STICK;
      const int8_t moveX = pointerMouseFrameStickDelta(
          frame, right ? frame.RX : frame.LX, settingLevel);
      const int8_t moveY = pointerMouseFrameStickDelta(
          frame, right ? frame.RY : frame.LY, settingLevel);
      state.pending_x = addBoundedPointerMouseDelta(state.pending_x, moveX);
      state.pending_y = addBoundedPointerMouseDelta(state.pending_y, moveY);
      state.last_sample_dx = moveX;
      state.last_sample_dy = moveY;
      state.last_stick_sample_ms = now;
      state.sample_count++;
    }
    return pointerMouseHasPending(state);
  }

  const bool physicalTouchpadClick =
      enabled && source == POINTER_MOUSE_TOUCHPAD &&
      (pre_remap_buttons[port] & ~held_across_menu_close[port] & INPUT_CAPTURE) != 0;
  state.desired_buttons = physicalTouchpadClick ? MOUSE_BUTTON_LEFT : 0;

  const ControllerTouchContact* active = nullptr;
  if (enabled && source == POINTER_MOUSE_TOUCHPAD) {
    if (frame.touch[0].active) active = &frame.touch[0];
    else if (frame.touch[1].active) active = &frame.touch[1];
  }

  if (active == nullptr) {
    state.last_sample_dx = 0;
    state.last_sample_dy = 0;
    state.anchored = false;
    clearPointerMouseQueuedMotion(state);
    if (!pointerMouseHasPending(state)) state.mouse_turn_due = false;
  } else if (!state.anchored || state.contact_id != active->id) {
    clearPointerMouseQueuedMotion(state);
    state.anchored = true;
    state.contact_id = active->id;
    state.previous_x = active->x;
    state.previous_y = active->y;
    state.last_sample_dx = 0;
    state.last_sample_dy = 0;
    state.sample_count++;
    if (!pointerMouseHasPending(state)) state.mouse_turn_due = false;
  } else {
    const int32_t deltaX = static_cast<int32_t>(active->x) - state.previous_x;
    const int32_t deltaY = static_cast<int32_t>(active->y) - state.previous_y;
    state.last_sample_dx = deltaX;
    state.last_sample_dy = deltaY;
    state.sample_count++;
    state.previous_x = active->x;
    state.previous_y = active->y;
    if (!touchpadMouseDeltaPlausible(deltaX, deltaY)) {
      clearPointerMouseQueuedMotion(state);
      state.rejected_sample_count++;
      if (!pointerMouseHasPending(state)) state.mouse_turn_due = false;
    } else {
      constexpr int32_t kScaledCountsPerPixel =
          TOUCHPAD_MOUSE_COUNTS_PER_PIXEL * POINTER_MOUSE_SPEED_UNITY;
      state.residual_x += deltaX * settingLevel;
      state.residual_y += deltaY * settingLevel;
      const int32_t moveX = state.residual_x / kScaledCountsPerPixel;
      const int32_t moveY = state.residual_y / kScaledCountsPerPixel;
      state.residual_x -= moveX * kScaledCountsPerPixel;
      state.residual_y -= moveY * kScaledCountsPerPixel;
      state.pending_x = addBoundedPointerMouseDelta(state.pending_x, moveX);
      state.pending_y = addBoundedPointerMouseDelta(state.pending_y, moveY);
    }
  }
  return pointerMouseHasPending(state);
}

inline bool pointerMouseShouldPrecedeGamepad(uint8_t port,
                                             bool gamepadPending) {
  if (port >= MAX_USB_OUT) return false;
  const PointerMouseRuntimeState& state = pointerMouseState[port];
  if (!pointerMouseHasPending(state)) return false;
  return !gamepadPending || state.mouse_turn_due;
}

inline void notePointerGamepadReportSent(uint8_t port, bool mousePending) {
  if (port < MAX_USB_OUT && mousePending) {
    pointerMouseState[port].mouse_turn_due = true;
  }
}

inline int8_t takePointerMouseDelta(int32_t value) {
  if (value > POINTER_MOUSE_REPORT_DELTA_MAX) {
    return POINTER_MOUSE_REPORT_DELTA_MAX;
  }
  if (value < -POINTER_MOUSE_REPORT_DELTA_MAX) {
    return -POINTER_MOUSE_REPORT_DELTA_MAX;
  }
  return static_cast<int8_t>(value);
}

inline bool sendPointerMouseReport(uint8_t port, Adafruit_USBD_HID* hid) {
  if (port >= MAX_USB_OUT || hid == nullptr) return false;
  PointerMouseRuntimeState& state = pointerMouseState[port];
  if (!pointerMouseHasPending(state)) return false;

  hid_mouse_report_t report{};
  report.buttons = state.desired_buttons;
  report.x = takePointerMouseDelta(state.pending_x);
  report.y = takePointerMouseDelta(state.pending_y);
  if (!hid->sendReport(POINTER_MOUSE_REPORT_ID, &report, sizeof(report))) {
    return false;
  }
  state.pending_x -= report.x;
  state.pending_y -= report.y;
  state.sent_buttons = state.desired_buttons;
  state.last_sent_dx = report.x;
  state.last_sent_dy = report.y;
  state.report_count++;
  state.mouse_turn_due = false;
  return true;
}
