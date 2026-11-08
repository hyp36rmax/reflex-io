#pragma once

#include <stdint.h>

enum PointerMouseSource : uint8_t {
  POINTER_MOUSE_OFF = 0,
  POINTER_MOUSE_LEFT_STICK = 1,
  POINTER_MOUSE_RIGHT_STICK = 2,
  POINTER_MOUSE_TOUCHPAD = 3,
};

constexpr uint8_t POINTER_MOUSE_SPEED_MAX = 16;
constexpr uint8_t POINTER_MOUSE_SPEED_DEFAULT = 4;
constexpr uint8_t POINTER_MOUSE_SPEED_UNITY = 8;
constexpr int16_t POINTER_MOUSE_STICK_DEADZONE = 24;

// Modern-controller targets historically stored 0=off and 1..16=touchpad speed in one byte.
// New values retain that byte and mark a packed source/speed configuration.
constexpr uint8_t POINTER_MOUSE_CONFIG_MARKER = 0x80;
constexpr uint8_t POINTER_MOUSE_CONFIG_SOURCE_SHIFT = 5;
constexpr uint8_t POINTER_MOUSE_CONFIG_SOURCE_MASK = 0x60;
constexpr uint8_t POINTER_MOUSE_CONFIG_SPEED_MASK = 0x1F;

inline uint8_t pointerMouseSanitizeSpeed(uint8_t speed) {
  return speed >= 1 && speed <= POINTER_MOUSE_SPEED_MAX
      ? speed
      : POINTER_MOUSE_SPEED_DEFAULT;
}

inline uint8_t pointerMouseEncodeConfig(PointerMouseSource source,
                                        uint8_t speed) {
  if (source == POINTER_MOUSE_OFF) return 0;
  return static_cast<uint8_t>(
      POINTER_MOUSE_CONFIG_MARKER |
      ((static_cast<uint8_t>(source) << POINTER_MOUSE_CONFIG_SOURCE_SHIFT) &
       POINTER_MOUSE_CONFIG_SOURCE_MASK) |
      pointerMouseSanitizeSpeed(speed));
}

// The Mouse output reads Off as the left stick, so Off keeps a chosen speed:
// the marker and speed with no source bits, which every reader decodes as
// Off. Off at the default speed stays 0, as before.
inline uint8_t pointerMouseEncodeConfigKeepingSpeed(PointerMouseSource source,
                                                    uint8_t speed) {
  const uint8_t sanitized = pointerMouseSanitizeSpeed(speed);
  if (source != POINTER_MOUSE_OFF) return pointerMouseEncodeConfig(source, sanitized);
  return sanitized == POINTER_MOUSE_SPEED_DEFAULT
      ? 0
      : static_cast<uint8_t>(POINTER_MOUSE_CONFIG_MARKER | sanitized);
}

inline PointerMouseSource pointerMouseConfigSource(uint8_t config) {
  if (config == 0) return POINTER_MOUSE_OFF;
  if ((config & POINTER_MOUSE_CONFIG_MARKER) == 0) {
    return POINTER_MOUSE_TOUCHPAD;
  }
  const uint8_t source = static_cast<uint8_t>(
      (config & POINTER_MOUSE_CONFIG_SOURCE_MASK) >>
      POINTER_MOUSE_CONFIG_SOURCE_SHIFT);
  return source >= POINTER_MOUSE_LEFT_STICK &&
         source <= POINTER_MOUSE_TOUCHPAD
      ? static_cast<PointerMouseSource>(source)
      : POINTER_MOUSE_OFF;
}

inline uint8_t pointerMouseConfigSpeed(uint8_t config) {
  if (config == 0) return POINTER_MOUSE_SPEED_DEFAULT;
  return pointerMouseSanitizeSpeed(
      (config & POINTER_MOUSE_CONFIG_MARKER) != 0
          ? static_cast<uint8_t>(config & POINTER_MOUSE_CONFIG_SPEED_MASK)
          : config);
}

inline bool pointerMouseStickSourceAvailable(PointerMouseSource source,
                                             bool hasMainStick,
                                             bool hasAuxStick) {
  return (source == POINTER_MOUSE_LEFT_STICK && hasMainStick) ||
         (source == POINTER_MOUSE_RIGHT_STICK && hasAuxStick);
}

inline int8_t pointerMouseStickDelta(int16_t axis, uint8_t speed) {
  if (axis >= -POINTER_MOUSE_STICK_DEADZONE &&
      axis <= POINTER_MOUSE_STICK_DEADZONE) {
    return 0;
  }
  int16_t delta = static_cast<int16_t>(
      (axis * pointerMouseSanitizeSpeed(speed)) /
      (16 * POINTER_MOUSE_SPEED_UNITY));
  if (delta == 0) delta = axis < 0 ? -1 : 1;
  if (delta > 127) delta = 127;
  if (delta < -127) delta = -127;
  return static_cast<int8_t>(delta);
}
