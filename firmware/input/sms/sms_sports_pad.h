#pragma once

#include <stdint.h>

// Sega Master System Sports Pad (US model 3040) with its switch on Sports.
// The host drives TH; each TH edge presents the next nibble on Up, Down,
// Left and Right as D0..D3 (line high = 1): TH low X high, TH high X low,
// TH low Y high, TH high Y low. Each axis is an 8-bit two's-complement count
// since the previous packet. Protocol from MAME's sms_ctrl/sports.cpp
// (research only); its timing, polarity and resting output are unverified on
// hardware. The Japanese Sports Pad uses another protocol and is not read.

struct SmsSportsPadPort {
  uint32_t dueUs = 0;
  int16_t deferX = 0;
  int16_t deferY = 0;
  bool active = false;
  uint8_t confirm = 0;
  uint8_t leave = 0;
  uint8_t tail = 0xFF;
};

namespace sms_sports_pad {

// Waits after each TH edge before sampling, as SMS games do.
constexpr uint8_t kFirstNibbleWaitUs = 80;
constexpr uint8_t kNibbleWaitUs = 40;
// An identified pad is read this often. TH stays high in between, longer
// than the 2.1 ms a Genesis six-button pad needs to reset its phase counter.
constexpr uint32_t kPacketIntervalUs = 4000;
// A port that is not a Sports Pad is probed at most this often.
constexpr uint32_t kProbeIntervalUs = 250000;
// Consecutive all-zero packets (a pad at rest) that identify one. After the
// first, the rest follow at the packet interval.
constexpr uint8_t kConfirmPackets = 3;
// Consecutive pad-like packets, about a quarter second, that drop one.
constexpr uint8_t kLeavePackets = 64;
constexpr uint8_t kNoTail = 0xFF;

// Packet nibbles in read order: X high in bits 15..12, Y low in bits 3..0.
inline uint8_t nibble(uint16_t packet, uint8_t index) {
  return static_cast<uint8_t>((packet >> (12 - 4 * index)) & 0x0F);
}

// A packet a Sports Pad does not send but another SMS-mode device does. A
// pad or Driving controller ignores TH, so its four nibbles are equal, and an
// empty port reads all ones. A Genesis pad answers Up, Down, 0, 0 on TH low
// and its normal lines on TH high; Up and Down are never both pressed.
// All zero is a Sports Pad at rest.
inline bool padLikePacket(uint16_t packet) {
  const uint8_t n0 = nibble(packet, 0), n1 = nibble(packet, 1);
  const uint8_t n2 = nibble(packet, 2), n3 = nibble(packet, 3);
  if (n0 == n1 && n1 == n2 && n2 == n3) return n0 != 0;
  return n0 == n2 && n1 == n3 && n0 == (n1 & 0x3) && (n1 & 0x3) != 0;
}

// USB X grows right and Y down. MAME reverses both axes, so the USB delta is
// the negated count; unverified on hardware.
inline int16_t usbDx(uint16_t packet) {
  return static_cast<int16_t>(-static_cast<int8_t>(packet >> 8));
}
inline int16_t usbDy(uint16_t packet) {
  return static_cast<int16_t>(-static_cast<int8_t>(packet & 0xFF));
}

}  // namespace sms_sports_pad
