#pragma once

#include <stdint.h>

// Per-interface Switch report cadence: 8 ms for Switch Pro
// (ADAPT_FEATURE_SWITCH_REPORT_STREAM), 4 ms for Switch 2 Pro.
struct SwitchReportStream {
  uint32_t submitted_at_ms = 0;
  bool started = false;
  uint8_t period_ms = 8;

  bool due(uint32_t now) const {
    return !started || uint32_t(now - submitted_at_ms) >= period_ms;
  }
  void submitted(uint32_t now) {
    submitted_at_ms = now;
    started = true;
  }
  void reset() { started = false; }
};
