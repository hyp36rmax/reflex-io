#pragma once

// Mouse output (OUTPUT_ROTARY_MOUSE). Three producers feed one report:
// - rotary sources queue relative motion (core/rotary_motion_state.h), sent
//   on the axis Mouse Axis selects, or as wheel steps;
// - an analog stick moves both axes by rate, with the stick math, Mouse source
//   and Pointer Speed of DInput's stick pointer (output_pointer_mouse_runtime.h);
// - the wheel source, a stick's Y axis or L/R, scrolls by rate.
// Each producer is drained by exactly what a submitted report carried. The
// buttons and the wheel follow the input mode's Mouse settings
// (core/rotary_mouse_settings.h). Included once, from out_usb.h, after the
// pointer runtime.

#include "../../core/rotary_mouse_settings.h"
#include "../../core/rotary_output_transform.h"
#include "../../core/classic_mouse_motion.h"

// menu/menu_ui_state.h and core/button_remap.h, with C++ linkage: out_usb.h
// includes this inside extern "C".
extern "C++" bool menuOwnsControllerInput();
extern "C++" uint32_t getInputButtonMask(DeviceEnum mode);

#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)

// Rotary queue units per wheel step: one Atari Driving step or two Jaguar
// transitions at 1x. Rotary Gain scales it.
constexpr int8_t ROTARY_MOUSE_WHEEL_UNITS = 8;
// A stick scrolls one step per this much summed pointer motion at the top
// Pointer Speed (about 15 per 8 ms sample at full deflection): about twelve
// steps a second at full deflection, fewer below it.
constexpr int16_t ROTARY_MOUSE_STICK_WHEEL_STEP = 150;
// A held D-pad Up or Down scrolls one step at once, then one every twelve
// 8 ms samples.
constexpr uint8_t ROTARY_MOUSE_DPAD_REPEAT_SAMPLES = 12;

struct RotaryMouseStickState {
  int16_t pending_x;
  int16_t pending_y;
  int16_t pending_wheel;
  int16_t wheel_sum;
  bool wheel_held;
  int8_t dpad_direction;
  uint8_t dpad_samples;
  uint32_t last_sample_ms;
};

RotaryMouseStickState rotaryMouseStick[MAX_USB_OUT] = {};

enum class RotaryMouseTarget : uint8_t { X, Y, Wheel };
// The rotary share of the report being sent, in queue units, and its target.
int8_t rotaryMouseSentRotary = 0;
RotaryMouseTarget rotaryMouseSentTarget = RotaryMouseTarget::X;
bool rotaryMouseSentNative = false;

// The live input mode's Mouse settings, reread when the mode or the store
// changes.
struct RotaryMouseActiveSettings {
  RotaryMouseSettings settings;
  uint16_t generation;
  DeviceEnum mode;
  bool valid;
};
RotaryMouseActiveSettings rotaryMouseActive = {};

inline __attribute__((noinline)) const RotaryMouseSettings& rotaryMouseActiveSettings() {
  const uint16_t generation = rotaryMouseSettingsGeneration();
  if (!rotaryMouseActive.valid || rotaryMouseActive.mode != deviceMode ||
      rotaryMouseActive.generation != generation) {
    rotaryMouseActive.settings = rotaryMouseSettingsRead(deviceMode);
    rotaryMouseActive.mode = deviceMode;
    rotaryMouseActive.generation = generation;
    rotaryMouseActive.valid = true;
  }
  return rotaryMouseActive.settings;
}

// One wheel step per ROTARY_MOUSE_STICK_WHEEL_STEP of summed stick motion,
// the first as soon as the stick leaves its deadzone. Up scrolls up.
inline int8_t rotaryMouseStickWheelSteps(RotaryMouseStickState& stick, int8_t delta) {
  if (delta == 0) {
    stick.wheel_sum = 0;
    stick.wheel_held = false;
    return 0;
  }
  if (stick.wheel_held) {
    stick.wheel_sum = static_cast<int16_t>(stick.wheel_sum - delta);
  } else {
    stick.wheel_held = true;
    stick.wheel_sum = delta < 0 ? ROTARY_MOUSE_STICK_WHEEL_STEP : -ROTARY_MOUSE_STICK_WHEEL_STEP;
  }
  int8_t steps = 0;
  while (stick.wheel_sum >= ROTARY_MOUSE_STICK_WHEEL_STEP) {
    stick.wheel_sum = static_cast<int16_t>(stick.wheel_sum - ROTARY_MOUSE_STICK_WHEEL_STEP);
    ++steps;
  }
  while (stick.wheel_sum <= -ROTARY_MOUSE_STICK_WHEEL_STEP) {
    stick.wheel_sum = static_cast<int16_t>(stick.wheel_sum + ROTARY_MOUSE_STICK_WHEEL_STEP);
    --steps;
  }
  return steps;
}

// D-pad Up scrolls up and Down down: one step as soon as one is pressed (or
// the other takes over), then one per repeat period while it is held.
inline int8_t rotaryMouseDpadWheelSteps(RotaryMouseStickState& stick,
                                        const controller_state_t& frame) {
  const int8_t direction = frame.PAD_U == frame.PAD_D ? 0 : (frame.PAD_U ? 1 : -1);
  if (direction != stick.dpad_direction) {
    stick.dpad_direction = direction;
    stick.dpad_samples = 1;
    return direction;
  }
  if (direction == 0) return 0;
  if (++stick.dpad_samples > ROTARY_MOUSE_DPAD_REPEAT_SAMPLES) {
    stick.dpad_samples = 1;
    return direction;
  }
  return 0;
}

// A held stick or D-pad keeps moving or scrolling, so the transport task
// samples them every 8 ms instead of waiting for the input frame to change.
// Like DInput's stick pointer, pending motion is bounded to four reports, so
// the pointer stops at once when the stick is released. Menus own the
// controller, so they stop it too. The wheel stick is never the one moving
// the pointer.
inline __attribute__((noinline)) void queueRotaryMouseStickMotion(outputMode_t output,
                                                                 uint32_t now) {
  const bool active = output == OUTPUT_ROTARY_MOUSE && !menuOwnsControllerInput();
  const uint8_t config = adaptPointerMouseConfigValue();
  const PointerMouseSource source = pointerMouseConfigSource(config);
  const uint8_t speed = pointerMouseConfigSpeed(config);
  const uint8_t setting = active ? rotaryMouseActiveSettings().wheel : ROTARY_MOUSE_WHEEL_OFF;
  for (uint8_t port = 0; port < MAX_USB_OUT; ++port) {
    RotaryMouseStickState& stick = rotaryMouseStick[port];
    const controller_state_t& frame = controllerFrameConst(port);
    if (!active || port >= max_devices || port >= rotaryMouseOutputPlayerCount() || !frame.connected) {
      stick = {};
      resetClassicMouseMotion(port);
      if (output == OUTPUT_ROTARY_MOUSE) resetRotaryMotion(port);
      continue;
    }
    if (rotaryMouseNativeFrame(frame)) {
      stick = {};
      if (peekClassicMouseX(port) || peekClassicMouseY(port) || peekClassicMouseWheel(port)) requestControllerFrameDelivery(port);
      continue;
    }
    if (stick.last_sample_ms != 0 &&
        static_cast<uint32_t>(now - stick.last_sample_ms) < POINTER_MOUSE_STICK_INTERVAL_MS) {
      continue;
    }
    stick.last_sample_ms = now;
    const uint8_t wheel = rotaryMouseResolvedWheel(setting, frame, source);
    int8_t steps = 0;
    if (rotaryMouseStickFrame(frame)) {
      const bool right = rotaryMouseUsesRightStick(frame, source);
      stick.pending_x = static_cast<int16_t>(addBoundedPointerMouseDelta(
          stick.pending_x, pointerMouseFrameStickDelta(frame, right ? frame.RX : frame.LX, speed)));
      stick.pending_y = static_cast<int16_t>(addBoundedPointerMouseDelta(
          stick.pending_y, pointerMouseFrameStickDelta(frame, right ? frame.RY : frame.LY, speed)));
      const bool rightWheel = wheel == ROTARY_MOUSE_WHEEL_RIGHT_STICK &&
                              frame.HAS_ANALOG_STICK_AUX && !right;
      const bool leftWheel = wheel == ROTARY_MOUSE_WHEEL_LEFT_STICK &&
                             frame.HAS_ANALOG_STICK_MAIN && right;
      steps = rightWheel || leftWheel
          ? rotaryMouseStickWheelSteps(stick, pointerMouseFrameStickDelta(
                frame, rightWheel ? frame.RY : frame.LY, POINTER_MOUSE_SPEED_MAX))
          : rotaryMouseStickWheelSteps(stick, 0);
    } else {
      stick.pending_x = 0;
      stick.pending_y = 0;
      (void)rotaryMouseStickWheelSteps(stick, 0);
    }
    if (wheel == ROTARY_MOUSE_WHEEL_DPAD) {
      steps = static_cast<int8_t>(steps + rotaryMouseDpadWheelSteps(stick, frame));
    } else {
      stick.dpad_direction = 0;
      stick.dpad_samples = 0;
    }
    stick.pending_wheel = static_cast<int16_t>(addBoundedPointerMouseDelta(stick.pending_wheel, steps));
    if (stick.pending_x != 0 || stick.pending_y != 0 || stick.pending_wheel != 0) {
      requestControllerFrameDelivery(port);
    }
  }
}

// Each mouse button follows its Mouse setting, or the controller's default
// (rotaryMouseDefaultSlot). Rotation moves the axis Mouse Axis selects, or
// turns the wheel in whole steps (clockwise scrolls down; Reverse flips it),
// leaving the rest queued. The Axis byte that is JogCon's Wheel Axis in the
// PSX record is the Mouse Axis (in PSX mode, for the neGcon too).
// Runs from flash so the RAM-resident USB send path stays small.
inline __attribute__((noinline)) void map_rotary_mouse_output(uint8_t port) {
  const controller_state_t& frame = controllerFrameConst(port);
  _rotary_mouse = {};
  rotaryMouseSentRotary = 0;
  rotaryMouseSentNative = false;
  if (!frame.connected || port >= rotaryMouseOutputPlayerCount() || menuOwnsControllerInput()) return;
  const RotaryMouseSettings& settings = rotaryMouseActiveSettings();
  const uint32_t modeButtons = getInputButtonMask(deviceMode);
  const bool keepFirst = rotaryMouseKeepsFirstLayout(deviceMode);
  const bool negcon = rotaryInputKindForFrame(frame) == RotaryInputKind::NeGcon;
  static constexpr uint8_t kButtonBits[ROTARY_MOUSE_ROLE_COUNT] = {
    MOUSE_BUTTON_LEFT, MOUSE_BUTTON_MIDDLE, MOUSE_BUTTON_RIGHT,
    MOUSE_BUTTON_BACKWARD, MOUSE_BUTTON_FORWARD};
  uint8_t buttons = 0;
  for (uint8_t role = 0; role < ROTARY_MOUSE_ROLE_COUNT; ++role) {
    const uint8_t slot = rotaryMouseButtonSlot(settings, modeButtons, keepFirst, negcon, role, &frame);
    if (slot != ROTARY_MOUSE_NO_SLOT && rotaryMouseSlotPressed(frame, slot)) {
      buttons = static_cast<uint8_t>(buttons | kButtonBits[role]);
    }
  }
  _rotary_mouse.buttons = buttons;
  if (rotaryMouseNativeFrame(frame)) {
    rotaryMouseSentNative = true;
    _rotary_mouse.x = peekClassicMouseX(port);
    _rotary_mouse.y = peekClassicMouseY(port);
    _rotary_mouse.wheel = peekClassicMouseWheel(port);
    return;
  }
  const RotaryMouseStickState& stick = rotaryMouseStick[port];
  const int8_t queued = peekRotaryMotion(port);
  int16_t x = stick.pending_x;
  int16_t y = stick.pending_y;
  int16_t wheel = stick.pending_wheel;
  if (settings.rotary_wheel) {
    const int8_t steps = static_cast<int8_t>(queued / ROTARY_MOUSE_WHEEL_UNITS);
    wheel = static_cast<int16_t>(wheel - steps);
    rotaryMouseSentRotary = static_cast<int8_t>(steps * ROTARY_MOUSE_WHEEL_UNITS);
    rotaryMouseSentTarget = RotaryMouseTarget::Wheel;
  } else if (menu_jogcon_wheel_axis != 0) {
    y = static_cast<int16_t>(y + queued);
    rotaryMouseSentRotary = queued;
    rotaryMouseSentTarget = RotaryMouseTarget::Y;
  } else {
    x = static_cast<int16_t>(x + queued);
    rotaryMouseSentRotary = queued;
    rotaryMouseSentTarget = RotaryMouseTarget::X;
  }
  _rotary_mouse.x = takePointerMouseDelta(x);
  _rotary_mouse.y = takePointerMouseDelta(y);
  _rotary_mouse.wheel = takePointerMouseDelta(wheel);
}

// After a Mouse report is accepted: drain each producer by its share (the
// stick's share is what each field carried beyond the rotary part) and ask
// for another report while motion remains; on the wheel, a rotary remainder
// short of one step waits for more turning. A port with no controller reports
// no motion, so nothing it queued could ever drain: drop it instead.
inline __attribute__((noinline)) void completeRotaryMouseReport(uint8_t sourcePort) {
  if (rotaryMouseSentNative) {
    consumeClassicMouseMotion(sourcePort, _rotary_mouse.x, _rotary_mouse.y, _rotary_mouse.wheel);
    if (!controllerFrameConst(sourcePort).connected) resetClassicMouseMotion(sourcePort);
    if (peekClassicMouseX(sourcePort) || peekClassicMouseY(sourcePort) || peekClassicMouseWheel(sourcePort)) {
      requestControllerFrameDelivery(sourcePort);
    }
    return;
  }
  const int8_t rotary = rotaryMouseSentRotary;
  const RotaryMouseTarget target = rotaryMouseSentTarget;
  consumeRotaryMotion(sourcePort, rotary);
  RotaryMouseStickState& stick = rotaryMouseStick[sourcePort];
  stick.pending_x = static_cast<int16_t>(
      stick.pending_x - (_rotary_mouse.x - (target == RotaryMouseTarget::X ? rotary : 0)));
  stick.pending_y = static_cast<int16_t>(
      stick.pending_y - (_rotary_mouse.y - (target == RotaryMouseTarget::Y ? rotary : 0)));
  stick.pending_wheel = static_cast<int16_t>(
      stick.pending_wheel - (_rotary_mouse.wheel +
          (target == RotaryMouseTarget::Wheel ? rotary / ROTARY_MOUSE_WHEEL_UNITS : 0)));
  if (!controllerFrameConst(sourcePort).connected) {
    resetRotaryMotion(sourcePort);
    stick = {};
  }
  const int8_t left = peekRotaryMotion(sourcePort);
  const bool rotaryLeft = target == RotaryMouseTarget::Wheel
      ? (left >= ROTARY_MOUSE_WHEEL_UNITS || left <= -ROTARY_MOUSE_WHEEL_UNITS)
      : left != 0;
  if (rotaryLeft || stick.pending_x != 0 || stick.pending_y != 0 || stick.pending_wheel != 0) {
    requestControllerFrameDelivery(sourcePort);
  }
}

#endif
