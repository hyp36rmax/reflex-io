#pragma once

#include <stdint.h>

// A mixed mouse/pad setup stays on the host's gamepad personality. A mouse
// on P2 alone is eligible only when the saved setting exposes two ports.
inline bool autoClassicMouseWanted(uint32_t connected, uint32_t mice,
                                   uint8_t players) {
  const uint32_t eligible = players >= 2 ? 3u : 1u;
  return (connected & mice & eligible) != 0 && (connected & ~mice) == 0;
}

struct AutoClassicMouseDebounce {
  bool pending = false;
  bool target = false;
  uint32_t since = 0;

  bool ready(bool desired, bool current, uint32_t now) {
    if (desired == current) { pending = false; return false; }
    if (!pending || target != desired) {
      pending = true; target = desired; since = now; return false;
    }
    // Allow input identity to settle after a reboot or a brief disconnect.
    return static_cast<uint32_t>(now - since) >= 2000;
  }
};
