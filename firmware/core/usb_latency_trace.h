#pragma once

#include <Arduino.h>

enum UsbLatencyInputClass : uint8_t {
  USB_LATENCY_INPUT_UNKNOWN = 0,
  USB_LATENCY_INPUT_NATIVE,
  USB_LATENCY_INPUT_KNOWN_HID,
  USB_LATENCY_INPUT_SDL_HID,
  USB_LATENCY_INPUT_GENERIC_HID,
  USB_LATENCY_INPUT_CLASS_COUNT,
};

// MODERN2USB hot-path timing. The trace starts at the controller-facing PIO-USB
// frame/report callback and ends only when the upstream host completes the IN
// transfer. It is disabled by default and intentionally passive while active:
// no serial output occurs on the hot path, and bounded samples are summarized
// only when requested.
#ifdef __cplusplus
extern "C" {
#endif
void usbLatencyTraceNoteRawReport(uint8_t player, uint8_t inputClass);
void usbLatencyTraceNoteDecodeComplete(uint8_t player);
void usbLatencyTraceNoteNeutralCommit(uint8_t player);
void usbLatencyTraceNoteBuildStart(uint8_t player);
void usbLatencyTraceNoteBuildComplete(uint8_t player);
void usbLatencyTraceNoteSubmit(uint8_t sourcePlayer, uint8_t outputSlot);
void usbLatencyTraceNoteComplete(uint8_t outputSlot, bool success);
bool usbLatencyTraceHandleSerialCommand(const char* command, Print& out);
#ifdef __cplusplus
}
#endif

class UsbLatencyDecodeScope {
 public:
  UsbLatencyDecodeScope(uint8_t player, uint8_t inputClass)
      : player_(player) {
    usbLatencyTraceNoteRawReport(player_, inputClass);
  }
  ~UsbLatencyDecodeScope() {
    usbLatencyTraceNoteDecodeComplete(player_);
  }

 private:
  uint8_t player_;
};

class UsbLatencyBuildScope {
 public:
  explicit UsbLatencyBuildScope(uint8_t player) : player_(player) {
    usbLatencyTraceNoteBuildStart(player_);
  }
  ~UsbLatencyBuildScope() {
    usbLatencyTraceNoteBuildComplete(player_);
  }

 private:
  uint8_t player_;
};
