#pragma once

#include <stdint.h>
#include "../../core/controller_state.h"
#include "../output_mode.h"

constexpr uint8_t REMOTE_OUTPUT_REPORT_ID = 1;
constexpr uint8_t REMOTE_OUTPUT_BUTTON_COUNT = 64;
constexpr uint8_t REMOTE_OUTPUT_NO_SOURCE = 0xFF;

struct RemoteOutputReport {
  uint8_t buttons[8];
};
static_assert(sizeof(RemoteOutputReport) == 8, "Remote HID payload is exactly 64 bits");

inline uint8_t remoteOutputSource(const controller_state_t* frames, uint8_t count) {
  for (uint8_t port = 0; port < count; ++port) {
    if (frames[port].connected && frames[port].extended_button_count == REMOTE_OUTPUT_BUTTON_COUNT) {
      return port;
    }
  }
  return REMOTE_OUTPUT_NO_SOURCE;
}

inline RemoteOutputReport remoteOutputReport(const controller_state_t& frame, bool suppressed) {
  RemoteOutputReport report{};
  const uint8_t count = frame.extended_button_count;
  uint64_t buttons = (!suppressed && frame.connected && count != 0 && count <= 64)
      ? frame.extended_buttons : 0;
  if (count < 64) buttons &= (uint64_t(1) << count) - 1;
  // Define the wire byte order independently of host ABI/alignment.
  for (uint8_t i = 0; i < sizeof(report.buttons); ++i) {
    report.buttons[i] = static_cast<uint8_t>(buttons >> (8u * i));
  }
  return report;
}

inline bool remoteOutputSelectionWanted(outputMode_t configured, bool genericHost,
                                        bool enabled, bool hasSource) {
  if (configured == OUTPUT_REMOTE) return true;
  if (!enabled || !hasSource) return false;
  return configured == OUTPUT_HID || configured == OUTPUT_MISTER ||
         (configured == OUTPUT_AUTO && genericHost);
}

struct RemoteOutputDebounce {
  bool pending = false;
  bool target = false;
  uint32_t since = 0;

  bool ready(bool wanted, bool current, uint32_t now) {
    if (wanted == current) { pending = false; return false; }
    if (!pending || wanted != target) {
      pending = true;
      target = wanted;
      since = now;
      return false;
    }
    return static_cast<uint32_t>(now - since) >= 2000;
  }
};
