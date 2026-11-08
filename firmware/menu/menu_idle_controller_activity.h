#pragma once

#include <stdint.h>

#include "../core/controller_state.h"

// Require roughly one-quarter stick travel so known noisy controllers (notably
// worn Xbox 360/Xbox Classic pads) cannot keep the OLED awake. These thresholds
// affect only idle detection and never alter controller output.
constexpr int32_t kIdleStickWakeThreshold8 = 32;
constexpr int32_t kIdleStickWakeThreshold12 = 512;
constexpr int32_t kIdleStickWakeThreshold16 = 8192;
constexpr int32_t kIdleTouchWakeThreshold = 512;

struct IdleControllerActivityTracker {
  bool connected = false;
  analog_stick_precision precision = ANALOG_STICK_PRECISION_8;
  int16_t stick_anchor[4] = {};
  ControllerTouchContact touch_anchor[2] = {};
  uint32_t mouse_motion_sequence = 0;
  uint64_t extended_buttons = 0;
};

inline int32_t idleAbsoluteDifference(int32_t current, int32_t anchor) {
  const int32_t difference = current - anchor;
  return difference < 0 ? -difference : difference;
}

inline int32_t idleStickWakeThreshold(analog_stick_precision precision) {
  switch (precision) {
    case ANALOG_STICK_PRECISION_8:
      return kIdleStickWakeThreshold8;
    case ANALOG_STICK_PRECISION_12:
      return kIdleStickWakeThreshold12;
    case ANALOG_STICK_PRECISION_16:
    default:
      return kIdleStickWakeThreshold16;
  }
}

inline void captureIdleControllerActivity(
    IdleControllerActivityTracker& tracker,
    const controller_state_t& frame,
    uint32_t mouseMotionSequence = 0) {
  tracker.connected = frame.connected;
  tracker.precision = frame.sticks_precision_bits;
  tracker.stick_anchor[0] = frame.LX;
  tracker.stick_anchor[1] = frame.LY;
  tracker.stick_anchor[2] = frame.RX;
  tracker.stick_anchor[3] = frame.RY;
  tracker.touch_anchor[0] = frame.touch[0];
  tracker.touch_anchor[1] = frame.touch[1];
  tracker.mouse_motion_sequence = mouseMotionSequence;
  tracker.extended_buttons = frame.extended_button_count ? frame.extended_buttons : 0;
}

inline void clearIdleControllerActivity(
    IdleControllerActivityTracker& tracker) {
  tracker = IdleControllerActivityTracker{};
}

inline bool updateIdleControllerMotionActivity(
    IdleControllerActivityTracker& tracker,
    const controller_state_t& frame,
    uint32_t mouseMotionSequence = 0) {
  if (!frame.connected) {
    clearIdleControllerActivity(tracker);
    return false;
  }

  if (!tracker.connected || tracker.precision != frame.sticks_precision_bits) {
    captureIdleControllerActivity(tracker, frame, mouseMotionSequence);
    return false;
  }

  const uint64_t extendedButtons =
      frame.extended_button_count ? frame.extended_buttons : 0;
  bool activity = mouseMotionSequence != tracker.mouse_motion_sequence ||
                  (extendedButtons & ~tracker.extended_buttons) != 0;
  tracker.mouse_motion_sequence = mouseMotionSequence;
  tracker.extended_buttons = extendedButtons;
  const int16_t currentSticks[4] = {
    frame.LX, frame.LY, frame.RX, frame.RY
  };
  const int32_t stickThreshold =
      idleStickWakeThreshold(frame.sticks_precision_bits);
  for (uint8_t axis = 0; axis < 4; ++axis) {
    if (idleAbsoluteDifference(
            currentSticks[axis], tracker.stick_anchor[axis]) >=
        stickThreshold) {
      tracker.stick_anchor[axis] = currentSticks[axis];
      activity = true;
    }
  }

  for (uint8_t index = 0; index < 2; ++index) {
    const ControllerTouchContact& current = frame.touch[index];
    ControllerTouchContact& anchor = tracker.touch_anchor[index];
    const bool currentActive = current.active != 0;
    const bool anchorActive = anchor.active != 0;
    const bool contactChanged =
        currentActive != anchorActive ||
        (currentActive && current.id != anchor.id);
    const bool contactMoved =
        currentActive && anchorActive &&
        (idleAbsoluteDifference(current.x, anchor.x) >=
             kIdleTouchWakeThreshold ||
         idleAbsoluteDifference(current.y, anchor.y) >=
             kIdleTouchWakeThreshold);
    if (contactChanged || contactMoved) {
      anchor = current;
      activity = true;
    }
  }

  return activity;
}

// Raw 16-bit analog channels that no controller frame carries, such as a JVS
// cabinet's with Analog Setup on Auto, where the USB axes stay centered. As with
// the sticks, a channel must move this far from where it last counted, so ADC
// jitter cannot keep the OLED awake. 1/16 of full scale is the travel JVS panel
// detection needs before it treats a control as moved.
constexpr uint16_t kIdleRawAnalogWakeThreshold = 0x1000;
constexpr uint8_t kIdleRawAnalogMaxChannels = 16;

struct IdleRawAnalogActivityTracker {
  uint8_t channels = 0;
  uint16_t anchor[kIdleRawAnalogMaxChannels] = {};
};

inline bool updateIdleRawAnalogActivity(IdleRawAnalogActivityTracker& tracker,
                                        const uint16_t* values, uint8_t channels,
                                        uint16_t threshold) {
  if (channels > kIdleRawAnalogMaxChannels) channels = kIdleRawAnalogMaxChannels;
  if (values == nullptr || channels == 0) {
    tracker = IdleRawAnalogActivityTracker{};
    return false;
  }
  if (tracker.channels != channels) {
    tracker.channels = channels;
    for (uint8_t i = 0; i < channels; ++i) tracker.anchor[i] = values[i];
    return false;
  }
  bool activity = false;
  for (uint8_t i = 0; i < channels; ++i) {
    if (idleAbsoluteDifference(values[i], tracker.anchor[i]) >= threshold) {
      tracker.anchor[i] = values[i];
      activity = true;
    }
  }
  return activity;
}
