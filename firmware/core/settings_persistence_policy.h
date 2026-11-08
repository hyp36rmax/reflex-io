#pragma once

#include <stdint.h>

constexpr uint32_t SETTINGS_PERSISTENCE_SAFE_WINDOW_MS = 50;

// Pure timing policy kept independent of Arduino/TinyUSB so the protected
// session boundary and wrap-safe dwell can be host tested.
struct SettingsPersistenceSafeWindow {
  bool armed = false;
  uint32_t started_ms = 0;

  void reset() {
    armed = false;
    started_ms = 0;
  }

  bool shouldFlush(bool pending, bool protectedSessionActive, uint32_t nowMs) {
    if (!pending || protectedSessionActive) {
      reset();
      return false;
    }
    if (!armed) {
      armed = true;
      started_ms = nowMs;
      return false;
    }
    return (uint32_t)(nowMs - started_ms) >= SETTINGS_PERSISTENCE_SAFE_WINDOW_MS;
  }
};
