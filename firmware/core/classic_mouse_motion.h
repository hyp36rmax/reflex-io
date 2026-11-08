#pragma once

#include <stdint.h>
#include "../firmware_platform_config.h"

extern "C++" {
// Relative samples belong to a poll, not to the persistent controller frame.
// Keep them until the output accepts a report, with a bounded backlog per port.
struct ClassicMouseMotion {
  int16_t x = 0;
  int16_t y = 0;
  int16_t wheel = 0;
};

inline ClassicMouseMotion* classicMouseMotionState() {
  static ClassicMouseMotion state[MAX_USB_OUT] = {};
  return state;
}

// The UI runs after USB and at a lower rate than input polling. Retain an
// activity generation independently of the queue USB is allowed to consume.
inline uint32_t* classicMouseMotionActivityState() {
  static uint32_t sequence[MAX_USB_OUT] = {};
  return sequence;
}

inline uint32_t classicMouseMotionActivitySequence(uint8_t port) {
  return port < MAX_USB_OUT ? classicMouseMotionActivityState()[port] : 0;
}

inline int16_t classicMouseBoundedAdd(int16_t value, int16_t delta) {
  const int32_t sum = static_cast<int32_t>(value) + delta;
  // Eight USB chunks retain a complete signed ten-bit console-mouse sample.
  return sum > 1016 ? 1016 : (sum < -1016 ? -1016 : sum);
}

inline void queueClassicMouseMotion(uint8_t port, int16_t dx, int16_t dy,
                                    int16_t wheel = 0) {
  if (port >= MAX_USB_OUT) return;
  if (dx != 0 || dy != 0 || wheel != 0) {
    ++classicMouseMotionActivityState()[port];
  }
  ClassicMouseMotion& state = classicMouseMotionState()[port];
  state.x = classicMouseBoundedAdd(state.x, dx);
  state.y = classicMouseBoundedAdd(state.y, dy);
  state.wheel = classicMouseBoundedAdd(state.wheel, wheel);
}

inline int8_t classicMouseChunk(int16_t value) {
  return static_cast<int8_t>(value > 127 ? 127 : (value < -127 ? -127 : value));
}

inline int8_t peekClassicMouseX(uint8_t port) {
  return port < MAX_USB_OUT ? classicMouseChunk(classicMouseMotionState()[port].x) : 0;
}
inline int8_t peekClassicMouseY(uint8_t port) {
  return port < MAX_USB_OUT ? classicMouseChunk(classicMouseMotionState()[port].y) : 0;
}
inline int8_t peekClassicMouseWheel(uint8_t port) {
  return port < MAX_USB_OUT ? classicMouseChunk(classicMouseMotionState()[port].wheel) : 0;
}

inline void consumeClassicMouseAxis(int16_t& pending, int8_t sent) {
  if (pending > 0 && sent > 0) pending -= sent > pending ? pending : sent;
  else if (pending < 0 && sent < 0) pending -= sent < pending ? pending : sent;
}
inline void consumeClassicMouseMotion(uint8_t port, int8_t dx, int8_t dy,
                                      int8_t wheel = 0) {
  if (port >= MAX_USB_OUT) return;
  ClassicMouseMotion& state = classicMouseMotionState()[port];
  consumeClassicMouseAxis(state.x, dx);
  consumeClassicMouseAxis(state.y, dy);
  consumeClassicMouseAxis(state.wheel, wheel);
}
inline void resetClassicMouseMotion(uint8_t port) {
  if (port < MAX_USB_OUT) classicMouseMotionState()[port] = {};
}
inline void resetAllClassicMouseMotion() {
  for (uint8_t port = 0; port < MAX_USB_OUT; ++port) resetClassicMouseMotion(port);
}
}  // extern "C++"
