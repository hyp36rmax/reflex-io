#pragma once

#include <stdint.h>

// Famicom Network Controller (HVC-051), on the Famicom expansion port: OUT0
// latch (HDMI 2 on the Famicom adapter), $4016 clock (HDMI 1) and $4016 D1
// (HDMI 3), the same pins as an NES pad. It shifts out 24 bits per latch:
// 0-7 a standard pad (A, B, Select, Start, Up, Down, Left, Right), 8-17 keys
// 0-9, 18 *, 19 #, 20 ., 21 C, 22 always released, 23 END (End of
// Communication), then pressed-level bits like a standard pad's tail.
// Sources: NESdev "Famicom Network Controller" and Raphnet's HVC-051 page.
// https://www.nesdev.org/wiki/Famicom_Network_Controller
//
// Line levels here are physical: 1 is high (released). An idle controller
// therefore drives the first 17 bits high, which SnesLib's 17-bit read takes
// for an empty port, and its keys 4-7 land in the SNES ID bits. Reading 32
// bits shows the signature: bit 22 high and bits 24-31 low. An empty port
// reads all high; NES pads go low from bit 8, SNES pads from bit 16.
namespace famicom_network {

constexpr uint8_t kReadBits = 32;
constexpr uint8_t kReportBits = 24;
constexpr uint8_t kAlwaysReleasedBit = 22;
constexpr uint32_t kSignatureMask = (1UL << kAlwaysReleasedBit) | 0xFF000000UL;
constexpr uint32_t kSignatureLevels = 1UL << kAlwaysReleasedBit;
constexpr uint32_t kReportMask = (1UL << kReportBits) - 1;

// Report bit numbers (pressed = 1 after decodePressed()).
enum Bit : uint8_t {
  kA = 0, kB, kSelect, kStart, kUp, kDown, kLeft, kRight,
  kKey0, kKey1, kKey2, kKey3, kKey4, kKey5, kKey6, kKey7, kKey8, kKey9,
  kStar, kHash, kDot, kClear, kUnused, kEnd,
};

inline constexpr bool signature(uint32_t levels) {
  return (levels & kSignatureMask) == kSignatureLevels;
}

// Pressed bits 0-23; bit 22 is never set.
inline constexpr uint32_t decodePressed(uint32_t levels) {
  return ~levels & kReportMask & ~(1UL << kAlwaysReleasedBit);
}

inline constexpr bool pressed(uint32_t report, Bit bit) {
  return (report >> bit) & 1UL;
}

// The NTT Data Keypad (Super Famicom) carries the same keys, so the HVC-051
// is handed to the adapter as an NTT report: the pad in SnesLib's SNES bit
// positions (A on SNES A, B on SNES B, no X/Y/L/R) and the keys in its
// extended word, END on the NTT's = key. The masks are SnesLib's
// SnesDigital_Enum and SnesDigitalNTT_Enum values.
struct NttShapedReport {
  uint16_t digital;
  uint16_t extended;
};

inline constexpr NttShapedReport toNttShape(uint32_t report) {
  return {
    static_cast<uint16_t>(
      (pressed(report, kA) ? 0x0100 : 0) | (pressed(report, kB) ? 0x0001 : 0) |
      (pressed(report, kSelect) ? 0x0004 : 0) | (pressed(report, kStart) ? 0x0008 : 0) |
      (pressed(report, kUp) ? 0x0010 : 0) | (pressed(report, kDown) ? 0x0020 : 0) |
      (pressed(report, kLeft) ? 0x0040 : 0) | (pressed(report, kRight) ? 0x0080 : 0)),
    static_cast<uint16_t>(
      ((report >> kKey0) & 0x03FF) |
      (pressed(report, kStar) ? 0x0400 : 0) | (pressed(report, kHash) ? 0x0800 : 0) |
      (pressed(report, kDot) ? 0x1000 : 0) | (pressed(report, kClear) ? 0x2000 : 0) |
      (pressed(report, kEnd) ? 0x8000 : 0)),
  };
}

// One latch and 32 bits, with the clock idling high like SnesLib's pad
// reader. Each bit is sampled after a rising edge; halfPeriodUs sets both
// clock phases. A Famicom spends about 7 us per bit, so the runtime uses
// 6 us, as the Auto probe does. Returns physical levels, bit 0 first.
template <typename Bus>
uint32_t readLevels(Bus& bus, unsigned halfPeriodUs) {
  bus.clock(true);
  bus.latch(true);
  bus.waitUs(12);
  bus.latch(false);
  bus.waitUs(halfPeriodUs);
  uint32_t levels = bus.data() ? 1UL : 0UL;
  for (uint8_t bit = 1; bit < kReadBits; ++bit) {
    bus.clock(false);
    bus.waitUs(halfPeriodUs);
    bus.clock(true);
    bus.waitUs(halfPeriodUs);
    if (bus.data()) levels |= 1UL << bit;
  }
  return levels;
}

}  // namespace famicom_network
