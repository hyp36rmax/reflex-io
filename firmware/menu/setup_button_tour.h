#pragma once

#include <stdint.h>

// Practice settings never leave this object. Holds use the same 1.5-second
// threshold as the menus, and every successful hold consumes its release.
namespace setup_button_tour {
enum Lesson : uint8_t { Scroll, Toggle, Cancel, Save, Defaults, Finished };
struct State {
  Lesson lesson = Scroll;
  uint8_t cursor = 0;
  uint8_t edited = 0;
  uint8_t saved = 0;
  bool active = false;
  bool intro = true;
  bool waitRelease = false;
  bool timing = false;
  uint32_t holdStart = 0;
};
constexpr uint32_t kHoldMs = 1500;

inline void begin(State& state) { state = State{}; state.active = true; }

// Returns true only when the final short press finishes the tour. Wrong or
// incomplete gestures cannot skip a lesson or run a real menu action.
inline bool update(State& s, bool left, bool right, bool leftTap,
                   bool rightTap, uint32_t now) {
  if (!s.active) return false;
  if (s.waitRelease) {
    if (!left && !right) s.waitRelease = false;
    return false;
  }
  if (s.intro) {
    if (rightTap) s.intro = false;
    return false;
  }
  if (s.lesson == Finished) {
    if (rightTap) { s.active = false; return true; }
    return false;
  }
  const bool expectedHold =
      (s.lesson == Cancel && left && !right) ||
      (s.lesson == Save && right && !left) ||
      (s.lesson == Defaults && left && right);
  if (expectedHold) {
    if (!s.timing) { s.timing = true; s.holdStart = now; }
    if (uint32_t(now - s.holdStart) >= kHoldMs) {
      if (s.lesson == Cancel) {
        s.edited = s.saved;
        s.lesson = Save;
      } else if (s.lesson == Save) {
        s.saved = s.edited;
        s.lesson = Defaults;
      } else {
        s.saved = s.edited = 0;
        s.lesson = Finished;
      }
      s.timing = false;
      s.waitRelease = true;
    }
    return false;
  }
  s.timing = false;
  if (leftTap) {
    s.cursor = (s.cursor + 1) % 3;
    if (s.lesson == Scroll) s.lesson = Toggle;
  } else if (rightTap) {
    s.edited ^= uint8_t(1u << s.cursor);
    if (s.lesson == Toggle) s.lesson = Cancel;
  }
  return false;
}
}  // namespace setup_button_tour
