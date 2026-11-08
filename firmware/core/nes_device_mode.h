#pragma once

#include <stdint.h>

// Stored in PerModeSettingsRecord::powerpad_mode. Keep the legacy values so
// existing Off/On records continue to select Pad/Power Pad.
enum NesDeviceMode : uint8_t {
  NES_DEVICE_PAD = 0,
  NES_DEVICE_POWERPAD = 1,
  NES_DEVICE_VAUS_NES = 2,
  NES_DEVICE_VAUS_FAMICOM = 3,
  NES_DEVICE_MODE_COUNT,
};

inline constexpr uint8_t sanitizeNesDeviceMode(uint8_t value) {
  return value < NES_DEVICE_MODE_COUNT ? value : static_cast<uint8_t>(NES_DEVICE_PAD);
}
