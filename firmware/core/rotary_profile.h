#pragma once

#include <stdint.h>

// Runtime profiles retain an explicit marker. EEPROM stores them compactly in
// the upper six bits of existing legacy fields, preserving each field's low
// two-bit legacy value.
constexpr uint8_t ROTARY_PROFILE_MARKER = 0x80;
constexpr uint8_t ROTARY_PROFILE_REVERSE = 0x20;
constexpr uint8_t ROTARY_PROFILE_GAIN_MASK = 0x1F;
constexpr uint8_t ROTARY_PROFILE_MAX_GAIN = 23;

inline constexpr bool rotaryProfileConfigured(uint8_t raw) {
  return (raw & ROTARY_PROFILE_MARKER) != 0 &&
         (raw & 0x40) == 0 &&
         (raw & ROTARY_PROFILE_GAIN_MASK) >= 1 &&
         (raw & ROTARY_PROFILE_GAIN_MASK) <= ROTARY_PROFILE_MAX_GAIN;
}
inline constexpr uint8_t rotaryProfileGain(uint8_t raw) {
  return rotaryProfileConfigured(raw) ? (raw & ROTARY_PROFILE_GAIN_MASK) : 0;
}
inline constexpr bool rotaryProfileReverse(uint8_t raw) {
  return rotaryProfileConfigured(raw) && (raw & ROTARY_PROFILE_REVERSE) != 0;
}
inline constexpr uint8_t encodeRotaryProfile(uint8_t gain, bool reverse) {
  return gain >= 1 && gain <= ROTARY_PROFILE_MAX_GAIN
    ? static_cast<uint8_t>(ROTARY_PROFILE_MARKER | gain |
                           (reverse ? ROTARY_PROFILE_REVERSE : 0))
    : 0;
}
inline constexpr uint8_t sanitizeRotaryProfile(uint8_t raw) {
  return rotaryProfileConfigured(raw) ? raw : 0;
}

constexpr uint8_t ROTARY_PROFILE_LEGACY_LOW_MASK = 0x03;
constexpr uint8_t ROTARY_PROFILE_PACKED_GAIN_SHIFT = 2;
constexpr uint8_t ROTARY_PROFILE_PACKED_GAIN_MASK = 0x7C;
constexpr uint8_t ROTARY_PROFILE_PACKED_REVERSE = 0x80;

inline constexpr uint8_t rotaryLegacyLowBits(uint8_t raw) {
  return raw & ROTARY_PROFILE_LEGACY_LOW_MASK;
}

inline constexpr uint8_t rotaryWheelSensitivity(uint8_t raw) {
  const uint8_t sensitivity = rotaryLegacyLowBits(raw);
  return sensitivity <= 2 ? sensitivity : 1;
}

inline constexpr uint8_t rotaryProfileFromPacked(uint8_t raw) {
  return encodeRotaryProfile(
    static_cast<uint8_t>((raw & ROTARY_PROFILE_PACKED_GAIN_MASK) >>
                         ROTARY_PROFILE_PACKED_GAIN_SHIFT),
    (raw & ROTARY_PROFILE_PACKED_REVERSE) != 0);
}

inline constexpr uint8_t withPackedRotaryProfile(uint8_t raw, uint8_t profile) {
  const uint8_t sanitized = sanitizeRotaryProfile(profile);
  return static_cast<uint8_t>(
    rotaryLegacyLowBits(raw) |
    ((rotaryProfileGain(sanitized) << ROTARY_PROFILE_PACKED_GAIN_SHIFT) &
     ROTARY_PROFILE_PACKED_GAIN_MASK) |
    (rotaryProfileReverse(sanitized) ? ROTARY_PROFILE_PACKED_REVERSE : 0));
}

inline constexpr uint8_t rotaryProfileFromJogconDigitalMap(uint8_t raw) {
  return rotaryProfileFromPacked(raw);
}

inline constexpr uint8_t rotaryProfileFromWheelSensitivity(uint8_t raw) {
  return rotaryProfileFromPacked(raw);
}

inline constexpr uint8_t withRotaryProfileInJogconDigitalMap(uint8_t raw, uint8_t profile) {
  return withPackedRotaryProfile(raw, profile);
}

inline constexpr uint8_t withRotaryProfileInWheelSensitivity(uint8_t raw, uint8_t profile) {
  return withPackedRotaryProfile(rotaryWheelSensitivity(raw), profile);
}
