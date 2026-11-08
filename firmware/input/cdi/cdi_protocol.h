#pragma once

#include <stdint.h>

// Philips CD-i pointing-device protocol, from Philips Interactive Media
// Systems, "Technical Info: Pointing Devices" (August 1990):
// - RS-232-C framing, 1200 or 9600 baud, 1 start bit, 7 data bits LSB first,
//   2 stop bits. On the wire a mark (1) is below +0.8 V and a space (0) above
//   +2.4 V, so a TTL-level line idles low.
// - The player drives RTS; the device sends on RXD. Negating RTS stops the
//   device. Asserting it again makes the device send its one-byte ID within
//   10 ms, or 100-500 ms after power-on: 'M' relative (mouse, trackball),
//   'J' maneuvering (joystick, joypad), 'T' absolute, 'S' absolute screen.
// - Relative and maneuvering devices send 3-byte packets; only the first byte
//   has d6 set:
//     byte 0: 1 bt1 bt2 Y7 Y6 X7 X6
//     byte 1: 0 X5..X0
//     byte 2: 0 Y5..Y0
//   X/Y are two's complement, left/up negative. One extra d6=0 byte may follow
//   for additional buttons; it is ignored here. A relative device sends only on
//   motion or a button change; a maneuvering device sends continuously while
//   out of center, so its direction is held until the packets stop.
// - Absolute devices send 4 bytes: 1 bt1 bt2 X9 X8 X7 X6 / 0 pd 0 Y9 Y8 Y7 Y6
//   / 0 X5..X0 / 0 Y5..Y0, 10-bit positions, pd = pen on the active area.
// No hardware access; Input_Cdi.h feeds this from the PIO receiver.
namespace cdi_protocol {

enum class Device : uint8_t {
  None = 0,
  Relative,        // 'M'
  Maneuvering,     // 'J'
  Absolute,        // 'T'
  AbsoluteScreen,  // 'S'
};

enum class Phase : uint8_t {
  RtsNegated,  // RTS held low so the next assertion asks for the ID
  AwaitId,     // RTS asserted, waiting for the ID byte
  Active,      // identified; decoding packets
};

constexpr uint32_t kBaudRates[2] = {1200, 9600};
constexpr uint8_t kBaudRateCount = 2;

// The spec's minimum negation is 10 ms.
constexpr uint32_t kRtsNegateUs = 20000;
// Power-on to ID is at most 500 ms; RTS assertion to ID at most 10 ms.
constexpr uint32_t kIdWaitUs = 600000;
// A character has at most 8 space bit-times (6.7 ms at 1200 baud), so a
// longer space with no characters is a break: the board's pull-up with no
// powered device driving the line.
constexpr uint32_t kBreakUs = 100000;
// Largest maneuvering value in the spec (an analog device at full deflection
// with a 1 s cursor speed); it maps to full stick deflection.
constexpr int16_t kFullDeflection = 0x13;

constexpr uint8_t kButton1 = 0x01;
constexpr uint8_t kButton2 = 0x02;

// CDISTAT shows the latest characters as received, in every phase. A framing
// error is kept as kRecentFramingError, which no 7-bit character can be.
constexpr uint8_t kRecentCount = 24;
constexpr uint8_t kRecentFramingError = 0x80;

struct Port {
  Phase phase;
  Device device;
  uint8_t baudIndex;
  bool lineSeenAtMark;  // a powered device holds the line at mark
  uint32_t phaseStartUs;
  uint32_t lastMarkUs;
  uint32_t lastCharacterUs;
  uint8_t packet[4];
  uint8_t packetLength;
  uint8_t buttons;
  // Relative and maneuvering motion: the latest packet per axis and when it
  // arrived. An axis is released by a zero value or when packets stop.
  int8_t heldX;
  int8_t heldY;
  uint32_t heldXUs;
  uint32_t heldYUs;
  // Relative motion accumulated until the next output.
  int16_t pendingMouseX;
  int16_t pendingMouseY;
  // Absolute devices.
  uint16_t absX;
  uint16_t absY;
  bool penDown;
  uint32_t ids;
  uint32_t packets;
  uint32_t framingErrors;
  uint32_t rejectedCharacters;
  uint32_t characters;
  // A ring of the latest recentCount entries; recent[recentNext] is the next
  // to be overwritten, so the oldest once the ring is full.
  uint8_t recent[kRecentCount];
  uint8_t recentNext;
  uint8_t recentCount;
};

struct Output {
  bool connected;
  Device device;
  uint8_t buttons;
  bool up;
  bool down;
  bool left;
  bool right;
  int8_t lx;
  int8_t ly;
  int8_t mouseX;
  int8_t mouseY;
};

inline uint32_t baudRate(const Port& port) {
  return kBaudRates[port.baudIndex < kBaudRateCount ? port.baudIndex : 0];
}

// A 7N2 character is 10 bit times, so a 3-byte packet takes 30: 25 ms at
// 1200 baud, the rate a maneuvering device repeats at. Hold a direction for
// 2.5 packets before treating the axis as centred.
inline uint32_t motionHoldUs(const Port& port) {
  return 75000000u / baudRate(port);
}

inline Device deviceForId(uint8_t id) {
  switch (id) {
    case 'M': return Device::Relative;
    case 'J': return Device::Maneuvering;
    case 'T': return Device::Absolute;
    case 'S': return Device::AbsoluteScreen;
    default: return Device::None;
  }
}

inline const char* deviceName(Device device) {
  switch (device) {
    case Device::Relative: return "CD-i Mouse";
    case Device::Maneuvering: return "CD-i Pad";
    case Device::Absolute: return "CD-i Tablet";
    case Device::AbsoluteScreen: return "CD-i Screen";
    default: return "CD-i";
  }
}

inline bool rtsAsserted(const Port& port) {
  return port.phase != Phase::RtsNegated;
}

inline void clearDeviceState(Port& port) {
  port.device = Device::None;
  port.packetLength = 0;
  port.buttons = 0;
  port.heldX = 0;
  port.heldY = 0;
  port.pendingMouseX = 0;
  port.pendingMouseY = 0;
  port.absX = 512;
  port.absY = 512;
  port.penDown = false;
}

inline void enterPhase(Port& port, Phase phase, uint32_t nowUs) {
  port.phase = phase;
  port.phaseStartUs = nowUs;
  port.packetLength = 0;
  if (phase == Phase::AwaitId) port.lineSeenAtMark = false;
}

// Starts identification at 1200 baud with RTS negated.
inline void reset(Port& port, uint32_t nowUs) {
  port = Port{};
  clearDeviceState(port);
  port.lastMarkUs = nowUs;
  port.lastCharacterUs = nowUs;
  enterPhase(port, Phase::RtsNegated, nowUs);
}

inline int16_t addClamped(int16_t total, int16_t delta) {
  const int32_t sum = (int32_t)total + delta;
  if (sum > 32767) return 32767;
  if (sum < -32768) return -32768;
  return (int16_t)sum;
}

inline void applyMotionPacket(Port& port, int8_t x, int8_t y, uint32_t nowUs) {
  port.heldX = x;
  port.heldY = y;
  if (x != 0) port.heldXUs = nowUs;
  if (y != 0) port.heldYUs = nowUs;
  if (port.device == Device::Relative) {
    port.pendingMouseX = addClamped(port.pendingMouseX, x);
    port.pendingMouseY = addClamped(port.pendingMouseY, y);
  }
}

inline void completePacket(Port& port, uint32_t nowUs) {
  const uint8_t* b = port.packet;
  port.buttons = (uint8_t)(((b[0] & 0x20) ? kButton1 : 0) |
                           ((b[0] & 0x10) ? kButton2 : 0));
  if (port.device == Device::Absolute || port.device == Device::AbsoluteScreen) {
    port.absX = (uint16_t)(((b[0] & 0x0F) << 6) | (b[2] & 0x3F));
    port.absY = (uint16_t)(((b[1] & 0x0F) << 6) | (b[3] & 0x3F));
    port.penDown = (b[1] & 0x20) != 0;
  } else {
    const int8_t x = (int8_t)(uint8_t)(((b[0] & 0x03) << 6) | (b[1] & 0x3F));
    const int8_t y = (int8_t)(uint8_t)(((b[0] & 0x0C) << 4) | (b[2] & 0x3F));
    applyMotionPacket(port, x, y, nowUs);
  }
  ++port.packets;
}

inline uint8_t packetLength(Device device) {
  return (device == Device::Absolute || device == Device::AbsoluteScreen) ? 4 : 3;
}

inline void recordRecent(Port& port, uint8_t entry) {
  port.recent[port.recentNext] = entry;
  port.recentNext = (uint8_t)((port.recentNext + 1) % kRecentCount);
  if (port.recentCount < kRecentCount) ++port.recentCount;
}

// The i-th recorded entry, oldest first; i < port.recentCount.
inline uint8_t recentEntry(const Port& port, uint8_t i) {
  const uint8_t oldest = port.recentCount < kRecentCount ? 0 : port.recentNext;
  return port.recent[(oldest + i) % kRecentCount];
}

// A 7-bit character from the receiver, which has already checked its stop
// bit.
inline void onCharacter(Port& port, uint8_t character, uint32_t nowUs) {
  port.lastCharacterUs = nowUs;
  const uint8_t value = character & 0x7F;
  ++port.characters;
  recordRecent(port, value);

  if (port.phase == Phase::RtsNegated) {
    // At most one character may still arrive after RTS is negated.
    return;
  }
  if (port.phase == Phase::AwaitId) {
    const Device device = deviceForId(value);
    if (device == Device::None) {
      ++port.rejectedCharacters;
      return;
    }
    clearDeviceState(port);
    port.device = device;
    ++port.ids;
    enterPhase(port, Phase::Active, nowUs);
    return;
  }

  if (value & 0x40) {
    // d6 starts a packet. An unfinished one is dropped.
    if (port.packetLength != 0) ++port.rejectedCharacters;
    port.packet[0] = value;
    port.packetLength = 1;
    return;
  }
  if (port.packetLength == 0) {
    // A data byte with no header, or the optional extra button byte after a
    // complete relative/maneuvering packet.
    return;
  }
  port.packet[port.packetLength++] = value;
  if (port.packetLength >= packetLength(port.device)) {
    completePacket(port, nowUs);
    port.packetLength = 0;
  }
}

// The receiver saw a space where the stop bit belongs: a break or noise.
inline void onFramingError(Port& port, uint32_t nowUs) {
  (void)nowUs;
  ++port.framingErrors;
  recordRecent(port, kRecentFramingError);
  port.packetLength = 0;
}

// Called every poll with the raw line level (true = mark, the idle level of a
// powered device). Returns true when the receiver must restart, because the
// baud rate changed or RTS was just asserted.
inline bool tick(Port& port, bool lineAtMark, uint32_t nowUs) {
  if (lineAtMark) {
    port.lastMarkUs = nowUs;
    port.lineSeenAtMark = true;
  }
  const uint32_t inPhaseUs = nowUs - port.phaseStartUs;

  switch (port.phase) {
    case Phase::RtsNegated:
      if (inPhaseUs >= kRtsNegateUs) {
        enterPhase(port, Phase::AwaitId, nowUs);
        return true;
      }
      return false;

    case Phase::AwaitId:
      if (inPhaseUs >= kIdWaitUs) {
        // A powered device that sent no valid ID may use the other baud rate;
        // with nothing attached, stay at 1200 and keep asking.
        if (port.lineSeenAtMark) {
          port.baudIndex = (uint8_t)((port.baudIndex + 1) % kBaudRateCount);
        } else {
          port.baudIndex = 0;
        }
        enterPhase(port, Phase::RtsNegated, nowUs);
      }
      return false;

    case Phase::Active:
      if (nowUs - port.lastMarkUs >= kBreakUs &&
          nowUs - port.lastCharacterUs >= kBreakUs) {
        // Unplugged: identify again from the start.
        clearDeviceState(port);
        port.baudIndex = 0;
        enterPhase(port, Phase::RtsNegated, nowUs);
      }
      return false;
  }
  return false;
}

inline int8_t clampToInt8(int32_t value) {
  if (value > 127) return 127;
  if (value < -127) return -127;
  return (int8_t)value;
}

inline int8_t stickFromMotion(int8_t value) {
  return clampToInt8((int32_t)value * 127 / kFullDeflection);
}

// Absolute 0..1023 to -127..127.
inline int8_t stickFromPosition(uint16_t value) {
  return clampToInt8(((int32_t)(value & 0x3FF) - 512) / 4);
}

// The controller state at nowUs. Consumes accumulated relative motion.
inline Output output(Port& port, uint32_t nowUs) {
  Output out{};
  out.connected = port.phase == Phase::Active && port.device != Device::None;
  out.device = port.device;
  if (!out.connected) {
    port.pendingMouseX = 0;
    port.pendingMouseY = 0;
    return out;
  }
  out.buttons = port.buttons;

  if (port.device == Device::Absolute || port.device == Device::AbsoluteScreen) {
    if (port.penDown) {
      out.lx = stickFromPosition(port.absX);
      out.ly = stickFromPosition(port.absY);
    }
    return out;
  }

  const uint32_t holdUs = motionHoldUs(port);
  const int8_t x = (port.heldX != 0 && nowUs - port.heldXUs < holdUs) ? port.heldX : 0;
  const int8_t y = (port.heldY != 0 && nowUs - port.heldYUs < holdUs) ? port.heldY : 0;
  out.left = x < 0;
  out.right = x > 0;
  out.up = y < 0;
  out.down = y > 0;
  out.lx = stickFromMotion(x);
  out.ly = stickFromMotion(y);
  out.mouseX = clampToInt8(port.pendingMouseX);
  out.mouseY = clampToInt8(port.pendingMouseY);
  port.pendingMouseX = (int16_t)(port.pendingMouseX - out.mouseX);
  port.pendingMouseY = (int16_t)(port.pendingMouseY - out.mouseY);
  return out;
}

}  // namespace cdi_protocol
