#pragma once

#include <stdint.h>

// A PC Engine device blanks its four data lines while its CLR pin (the Mega
// Drive TH line) is high, so the Mega Drive driver reads it as a pad holding
// Up, Down, Left and Right on every poll. A Mega Drive pad shows a released
// direction on its first read. Under Auto, a pad that never has within this
// time is dropped so Auto scans the port again and finds the PC Engine device.
constexpr uint32_t kMegadrivePceAliasDropMs = 500;

struct MegadrivePceAliasTimer {
  uint32_t blankSinceMs = 0;
  bool blankStarted = false;

  void reset() {
    blankSinceMs = 0;
    blankStarted = false;
  }

  // True while the slot should read as disconnected. Multitap pads are typed
  // without the direction-page check, so a tapped port is never dropped.
  bool drop(bool megadriveType, bool autoActive, bool tapActive,
            bool directionPageSeen, uint32_t nowMs) {
    if (!megadriveType || !autoActive || tapActive || directionPageSeen) {
      reset();
      return false;
    }
    if (!blankStarted) {
      blankStarted = true;
      blankSinceMs = nowMs;
      return false;
    }
    return uint32_t(nowMs - blankSinceMs) >= kMegadrivePceAliasDropMs;
  }
};
