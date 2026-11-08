#pragma once

#include <stdint.h>

// Classic2USB reuses the existing Wii range byte for rotary policy without
// changing the persisted per-mode record or its EEPROM addresses.
constexpr uint8_t ROTARY_ANALOG_RANGE_MASK = 0x03u;
constexpr uint8_t ROTARY_DIVISOR_MASK = 0x3Cu;
constexpr uint8_t ROTARY_DIVISOR_SHIFT = 2u;
constexpr uint8_t ROTARY_REVERSE_MASK = 0x40u;
constexpr uint8_t ROTARY_WRAP_MASK = 0x80u;

inline constexpr uint8_t rotaryAnalogRange(uint8_t raw) { return raw & ROTARY_ANALOG_RANGE_MASK; }
inline constexpr uint8_t rotaryDivisor(uint8_t raw) {
  return static_cast<uint8_t>(((raw & ROTARY_DIVISOR_MASK) >> ROTARY_DIVISOR_SHIFT) + 1u);
}
inline constexpr bool rotaryReverse(uint8_t raw) { return (raw & ROTARY_REVERSE_MASK) != 0; }
inline constexpr bool rotaryWrap(uint8_t raw) { return (raw & ROTARY_WRAP_MASK) != 0; }

inline constexpr uint8_t encodeRotaryAnalogRange(uint8_t raw, uint8_t range) {
  return static_cast<uint8_t>((raw & ~ROTARY_ANALOG_RANGE_MASK) | (range & ROTARY_ANALOG_RANGE_MASK));
}
inline constexpr uint8_t encodeRotaryDivisor(uint8_t raw, uint8_t divisor) {
  const uint8_t bounded = divisor >= 1u && divisor <= 16u ? divisor : 1u;
  return static_cast<uint8_t>((raw & ~ROTARY_DIVISOR_MASK) | ((bounded - 1u) << ROTARY_DIVISOR_SHIFT));
}
inline constexpr uint8_t encodeRotaryReverse(uint8_t raw, bool reverse) {
  return static_cast<uint8_t>((raw & ~ROTARY_REVERSE_MASK) | (reverse ? ROTARY_REVERSE_MASK : 0u));
}
inline constexpr uint8_t encodeRotaryWrap(uint8_t raw, bool wrap) {
  return static_cast<uint8_t>((raw & ~ROTARY_WRAP_MASK) | (wrap ? ROTARY_WRAP_MASK : 0u));
}

// Canonical rotary gain codes preserve the existing 1/divisor values while
// adding explicit multipliers without changing the stored per-mode layout.
inline constexpr uint8_t rotaryGainNumerator(uint8_t gainCode) {
  return gainCode == 17u ? 2u : gainCode == 18u ? 4u :
         gainCode == 19u ? 3u : gainCode == 20u ? 8u :
         gainCode == 21u ? 12u : gainCode == 22u ? 16u :
         gainCode == 23u ? 3u : 1u;
}

inline constexpr uint8_t rotaryGainDenominator(uint8_t gainCode) {
  return gainCode >= 1u && gainCode <= 16u ? gainCode :
         gainCode == 19u ? 2u : 1u;
}

inline constexpr uint8_t rotaryGainFromSpinnerSpeed(uint8_t spinnerSpeed) {
  return spinnerSpeed == 0u ? 4u : spinnerSpeed == 1u ? 2u :
         spinnerSpeed == 2u ? 1u : spinnerSpeed == 3u ? 17u :
         spinnerSpeed == 4u ? 18u : 1u;
}

inline constexpr uint8_t rotaryGainFromWheelSensitivity(uint8_t sensitivity) {
  return sensitivity == 0u ? 2u : sensitivity == 1u ? 1u :
         sensitivity == 2u ? 17u : 1u;
}

inline constexpr uint8_t rotaryGainFromDrivingSpeed(uint8_t drivingSpeed) {
  return drivingSpeed == 0u ? 4u : drivingSpeed == 1u ? 2u :
         drivingSpeed == 2u ? 1u : drivingSpeed == 3u ? 19u :
         drivingSpeed == 4u ? 17u : 1u;
}

inline uint8_t applyRotaryPositionGain(uint8_t raw, uint8_t gainCode, bool reverse) {
  const int displacement = static_cast<int>(raw) - 128;
  const int scaled = displacement * rotaryGainNumerator(gainCode) /
                     rotaryGainDenominator(gainCode);
  int value = 128 + scaled;
  if (value < 0) value = 0;
  if (value > 255) value = 255;
  return reverse ? static_cast<uint8_t>(255 - value) : static_cast<uint8_t>(value);
}

inline uint8_t applyPaddleGain(uint8_t raw, uint8_t divisor, bool reverse) {
  const uint8_t bounded = divisor >= 1u && divisor <= 16u ? divisor : 1u;
  return applyRotaryPositionGain(raw, bounded, reverse);
}
