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

// A PS4/PS5 session holds saves only while authentication may be running
// (owner, issue #87, 2026-11-09). A flash write stops both RP2040 cores for
// about 150 ms, which would stall an auth exchange and the USB host bus to its
// dongle. The console authenticates after it connects or wakes, so a session
// holds for its first 10 s, and then for 5 s after each console auth request.
// A PS5 signing dongle signs every input report, so those sessions hold
// throughout and write when the console sleeps or unplugs, as before.
constexpr uint32_t SETTINGS_PERSISTENCE_AUTH_START_GRACE_MS = 10000;
constexpr uint32_t SETTINGS_PERSISTENCE_AUTH_QUIET_MS = 5000;

struct ConsoleAuthSaveGate {
  bool session = false;
  bool start_grace = false;
  bool auth_recent = false;
  uint32_t session_started_ms = 0;
  uint32_t last_auth_ms = 0;
  uint32_t last_auth_count = 0;

  // authRequestCount is any counter that moves on each console auth request;
  // only a change within this session counts.
  bool holdsSaves(bool sessionActive, bool signsEveryReport,
                  uint32_t authRequestCount, uint32_t nowMs) {
    if (!sessionActive) {
      session = false;
      return false;
    }
    if (!session) {
      session = true;
      start_grace = true;
      auth_recent = false;
      session_started_ms = nowMs;
      last_auth_count = authRequestCount;
    }
    if (authRequestCount != last_auth_count) {
      last_auth_count = authRequestCount;
      last_auth_ms = nowMs;
      auth_recent = true;
    }
    // Latched off once passed, so a millis() wrap cannot reopen them.
    if (start_grace && (uint32_t)(nowMs - session_started_ms) >=
                           SETTINGS_PERSISTENCE_AUTH_START_GRACE_MS) {
      start_grace = false;
    }
    if (auth_recent && (uint32_t)(nowMs - last_auth_ms) >=
                           SETTINGS_PERSISTENCE_AUTH_QUIET_MS) {
      auth_recent = false;
    }
    return signsEveryReport || start_grace || auth_recent;
  }
};
