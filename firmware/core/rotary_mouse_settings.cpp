#include "../product_config.h"
#include "rotary_mouse_settings.h"

#include "remap_pair_store.h"
#include "device_runtime_state.h"

namespace {
uint16_t generation = 0;
}

RotaryMouseSettings rotaryMouseSettingsRead(uint8_t input) {
  uint8_t bytes[REMAP_MAX_BUTTONS];
  for (uint8_t i = 0; i < REMAP_MAX_BUTTONS; ++i) bytes[i] = i;
  (void)remapPairRead(input, ROTARY_MOUSE_SETTINGS_PAIR_KEY, bytes);
  return rotaryMouseSettingsFromBytes(bytes);
}

bool rotaryMouseSettingsWrite(uint8_t input, const RotaryMouseSettings& settings) {
  uint8_t bytes[REMAP_MAX_BUTTONS];
  rotaryMouseSettingsToBytes(settings, bytes);
  if (!remapPairWrite(input, ROTARY_MOUSE_SETTINGS_PAIR_KEY, bytes)) return false;
  ++generation;
  return true;
}

uint16_t rotaryMouseSettingsGeneration() {
  return generation;
}

uint8_t rotaryMouseOutputPlayerCount() {
  static bool valid = false;
  static DeviceEnum mode;
  static uint16_t cachedGeneration;
  static uint8_t count = 1;
  if (!valid || mode != deviceMode || cachedGeneration != generation) {
    mode = deviceMode;
    cachedGeneration = generation;
    count = rotaryMousePlayerCount(rotaryMouseSettingsRead(mode));
    valid = true;
  }
  return count;
}
