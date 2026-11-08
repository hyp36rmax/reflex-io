#pragma once

#include <stddef.h>
#include <stdint.h>

// Configured interrupt-IN interval for the primary USB control interface.
// This is descriptor metadata, not a measurement of host polls/report delivery.
// Zero means unavailable (including non-USB outputs and unmounted devices).
uint32_t outputUsbInputIntervalUs();

namespace output_usb_interval_detail {

inline bool isControlInterface(uint8_t cls, uint8_t subclass, uint8_t protocol) {
  return cls == 0x03 || // HID gamepad, mouse or keyboard.
         (cls == 0x58 && subclass == 0x42) || // Original Xbox XID.
         (cls == 0xFF &&
          ((subclass == 0x5D && (protocol == 0x01 || protocol == 0x81)) ||
           (subclass == 0x47 && protocol == 0xD0))); // XUSB / wireless / GIP.
}

inline uint32_t usbInputIntervalUs(const uint8_t* configuration,
                                   size_t available, bool highSpeed) {
  if (configuration == nullptr || available < 9 ||
      configuration[0] != 9 || configuration[1] != 2) {
    return 0;
  }
  const size_t totalLength =
    static_cast<size_t>(configuration[2]) |
    (static_cast<size_t>(configuration[3]) << 8);
  if (totalLength < 9 || totalLength > available) return 0;

  bool controlInterface = false;
  for (size_t offset = 9; offset < totalLength;) {
    if (totalLength - offset < 2) return 0;
    const uint8_t* descriptor = configuration + offset;
    const uint8_t length = descriptor[0];
    if (length < 2 || length > totalLength - offset) return 0;

    if (descriptor[1] == 4) {
      if (length < 9) return 0;
      controlInterface = descriptor[3] == 0 &&
        isControlInterface(descriptor[5], descriptor[6], descriptor[7]);
    } else if (descriptor[1] == 5) {
      if (length < 7) return 0;
      // Ignore CDC notification, MSC/bulk, OUT and authentication endpoints.
      if (controlInterface && (descriptor[2] & 0x80) != 0 &&
          (descriptor[3] & 0x03) == 3) {
        const uint8_t interval = descriptor[6];
        if (interval == 0) return 0;
        if (!highSpeed) return static_cast<uint32_t>(interval) * 1000u;
        if (interval > 16) return 0;
        return 125u << (interval - 1u);
      }
    }
    offset += length;
  }
  return 0;
}

} // namespace output_usb_interval_detail
