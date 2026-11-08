#pragma once

#include <stdint.h>

// Transport-independent two-finger touch state. Coordinates are normalized to
// the full unsigned 16-bit range so controller-specific touchpad dimensions do
// not leak into the neutral frame.
struct __attribute__((packed, aligned(1))) ControllerTouchContact {
  uint16_t x;
  uint16_t y;
  uint8_t id;
  uint8_t active;
};

inline uint16_t normalizeTouchCoordinate(uint16_t value, uint16_t sourceMax) {
  if (sourceMax == 0) return 0;
  if (value > sourceMax) value = sourceMax;
  return static_cast<uint16_t>(
      (static_cast<uint32_t>(value) * 65535u + sourceMax / 2u) / sourceMax);
}

inline uint16_t denormalizeTouchCoordinate(uint16_t value, uint16_t targetMax) {
  return static_cast<uint16_t>(
      (static_cast<uint32_t>(value) * targetMax + 32767u) / 65535u);
}

// Sony touch points use a 7-bit contact ID plus an inactive high bit followed
// by packed 12-bit X/Y coordinates. DualShock 4 and DualSense share this shape.
inline ControllerTouchContact decodeSonyTouchPoint(
    const uint8_t packed[4],
    uint16_t sourceMaxX,
    uint16_t sourceMaxY) {
  ControllerTouchContact contact{};
  if (packed == nullptr) return contact;
  contact.id = packed[0] & 0x7Fu;
  if ((packed[0] & 0x80u) != 0 ||
      sourceMaxX == 0 || sourceMaxY == 0) {
    return contact;
  }
  const uint16_t rawX =
      static_cast<uint16_t>(packed[1]) |
      (static_cast<uint16_t>(packed[2] & 0x0Fu) << 8);
  const uint16_t rawY =
      (static_cast<uint16_t>(packed[2] >> 4) & 0x0Fu) |
      (static_cast<uint16_t>(packed[3]) << 4);
  // Fail malformed active contacts closed. Clamping them to the surface
  // maximum creates the exact repeated bottom-right cursor failure this
  // neutral transport must prevent.
  if (rawX > sourceMaxX || rawY > sourceMaxY) {
    return contact;
  }
  contact.active = 1;
  contact.x = normalizeTouchCoordinate(rawX, sourceMaxX);
  contact.y = normalizeTouchCoordinate(rawY, sourceMaxY);
  return contact;
}

inline void encodeSonyTouchPoint(
    const ControllerTouchContact& contact,
    uint16_t targetMaxX,
    uint16_t targetMaxY,
    uint8_t packed[4]) {
  if (packed == nullptr) return;
  const uint16_t rawX = denormalizeTouchCoordinate(contact.x, targetMaxX);
  const uint16_t rawY = denormalizeTouchCoordinate(contact.y, targetMaxY);
  packed[0] = static_cast<uint8_t>(
      (contact.id & 0x7Fu) | (contact.active ? 0u : 0x80u));
  packed[1] = static_cast<uint8_t>(rawX & 0xFFu);
  packed[2] = static_cast<uint8_t>(
      ((rawX >> 8) & 0x0Fu) | ((rawY & 0x0Fu) << 4));
  packed[3] = static_cast<uint8_t>((rawY >> 4) & 0xFFu);
}
