#pragma once

#include <stddef.h>
#include <stdint.h>

// Maps a USB configuration descriptor bInterfaceNumber to the ordinal of that
// HID interface.  The ordinal is then translated to Adapt's source port by the
// output runtime, which owns product-specific HID port ordering.
namespace output_usb_interface_mapping {

constexpr int8_t kUnknownSourcePort = -1;
constexpr uint8_t kInterfaceDescriptorType = 0x04;
constexpr uint8_t kHidClass = 0x03;

inline int8_t hidOrdinalForInterface(const uint8_t* descriptor, size_t length,
                                     uint8_t interfaceNumber) {
  if (!descriptor || length < 9 || descriptor[0] < 9 || descriptor[0] > length ||
      descriptor[1] != 0x02) {
    return kUnknownSourcePort;
  }

  uint8_t ordinal = 0;
  size_t offset = descriptor[0];
  while (offset + 2 <= length) {
    const uint8_t descriptorLength = descriptor[offset];
    if (descriptorLength < 2 || offset + descriptorLength > length) {
      return kUnknownSourcePort;
    }
    if (descriptor[offset + 1] == kInterfaceDescriptorType && descriptorLength >= 9 &&
        descriptor[offset + 3] == 0 && descriptor[offset + 5] == kHidClass) {
      if (descriptor[offset + 2] == interfaceNumber) {
        return static_cast<int8_t>(ordinal);
      }
      if (ordinal == INT8_MAX) {
        return kUnknownSourcePort;
      }
      ++ordinal;
    }
    offset += descriptorLength;
  }
  return kUnknownSourcePort;
}

}  // namespace output_usb_interface_mapping
