#include "rotary_motion_state.h"

#include <limits.h>

namespace {

int32_t pendingMotion[MAX_USB_OUT] = {};

int32_t saturatingAdd(int32_t value, int32_t delta) {
  if (delta > 0 && value > INT32_MAX - delta) {
    return INT32_MAX;
  }
  if (delta < 0 && value < INT32_MIN - delta) {
    return INT32_MIN;
  }
  return value + delta;
}

}  // namespace

void addRotaryMotion(uint8_t port, int32_t delta) {
  if (port >= MAX_USB_OUT || delta == 0) {
    return;
  }
  pendingMotion[port] = saturatingAdd(pendingMotion[port], delta);
}

int8_t peekRotaryMotion(uint8_t port) {
  if (port >= MAX_USB_OUT) {
    return 0;
  }
  if (pendingMotion[port] > INT8_MAX) {
    return INT8_MAX;
  }
  if (pendingMotion[port] < -INT8_MAX) {
    return -INT8_MAX;
  }
  return static_cast<int8_t>(pendingMotion[port]);
}

void consumeRotaryMotion(uint8_t port, int8_t sent) {
  if (port >= MAX_USB_OUT || sent == 0) {
    return;
  }

  const int32_t pending = pendingMotion[port];
  const int32_t submitted = sent;
  if ((pending > 0 && submitted > 0) || (pending < 0 && submitted < 0)) {
    const int32_t bounded = pending > 0
      ? (submitted > pending ? pending : submitted)
      : (submitted < pending ? pending : submitted);
    pendingMotion[port] = pending - bounded;
  }
}

void resetRotaryMotion(uint8_t port) {
  if (port < MAX_USB_OUT) {
    pendingMotion[port] = 0;
  }
}

void resetAllRotaryMotion() {
  for (uint8_t port = 0; port < MAX_USB_OUT; ++port) {
    pendingMotion[port] = 0;
  }
}
