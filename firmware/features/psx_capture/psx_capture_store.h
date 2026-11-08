#pragma once

#include <stdint.h>
#include <string.h>

namespace psx_capture {

constexpr uint16_t kCapacity = 256;
constexpr uint16_t kFirstRetained = 64;
constexpr uint16_t kRecentCapacity = kCapacity - kFirstRetained;
constexpr uint8_t kWireBytes = 35;
constexpr uint32_t kPollSampleUs = 100000;
constexpr uint16_t kHeaderSectors = 4;
constexpr uint16_t kFooterSectors = 2;
constexpr uint32_t kReportFileBytes = uint32_t(kHeaderSectors + kCapacity + kFooterSectors) * 512;
enum class Kind : uint8_t {
  AutoBegin = 1,
  AutoEnd,
  AutoProbe,
  Spi,
  Decoded,
  Reenum,
  Deep,
  AckResult,
  Selection,
  IrWire
};
enum Flag : uint16_t {
  HeaderFF = 1,
  Sync5A = 2,
  Sync00 = 4,
  KnownType = 8,
  LegacyType = 16,
  Complete = 32,
  Truncated = 64,
  Connected = 128,
  Pressure = 256,
  HotSwap = 512,
  Accepted = 1024,
  DeepPhase = 2048,
  PhysicalAck = 4096,
  TransportOk = 8192,
};

// Wire bytes are kept even when the library rejects a reply. No pointers,
// unique device serials, persistence data or allocated storage.
struct Event {
  uint32_t sequence = 0;
  uint32_t us = 0;
  uint32_t durationUs = 0;
  uint32_t aux = 0;
  uint16_t gapUs = 0;
  uint16_t flags = 0;
  uint8_t kind = 0;
  uint8_t port = 0;
  uint8_t count = 0;
  uint8_t stage = 0;
  uint8_t tx[kWireBytes] = {};
  uint8_t rx[kWireBytes] = {};
};
static_assert(sizeof(Event) == 96, "Capture record/RAM contract");

struct PortStats {
  uint32_t scans = 0;
  uint32_t probes = 0;
  uint32_t rejects = 0;
  uint32_t otherSelections = 0;
  uint32_t spi = 0;
  uint32_t badSpi = 0;
  uint32_t sampledOut = 0;
};

struct Store {
  Event events[kCapacity] = {};
  PortStats ports[2] = {};
  uint32_t attempted = 0;
  uint32_t dropped = 0;
  uint32_t startedUs = 0;
  uint32_t frozenUs = 0;
  uint32_t generation = 1;
  uint16_t count = 0;
  uint16_t recentWrite = 0;
  bool frozen = false;
  uint32_t lastPollUs[2] = {};
  uint16_t pollSamples[2] = {};
  uint8_t lastPollType[2] = {};
  bool lastPollValid[2] = {};
  uint16_t lastDecoded[2] = {0xFFFF, 0xFFFF};

  void clear(uint32_t now) {
    const uint32_t next = generation + 1;
    // Old rows are unreachable once count resets; avoid a 24 KiB clear.
    count = 0;
    recentWrite = 0;
    attempted = 0;
    dropped = 0;
    startedUs = now;
    frozenUs = 0;
    generation = next;
    frozen = false;
    ports[0] = PortStats{};
    ports[1] = PortStats{};
    memset(lastPollUs, 0, sizeof(lastPollUs));
    memset(pollSamples, 0, sizeof(pollSamples));
    memset(lastPollType, 0, sizeof(lastPollType));
    memset(lastPollValid, 0, sizeof(lastPollValid));
    lastDecoded[0] = lastDecoded[1] = 0xFFFF;
  }

  void freeze(uint32_t now) {
    frozenUs = now;
    frozen = true;
  }

  const Event &record(uint16_t index) const {
    if (count < kCapacity || index < kFirstRetained)
      return events[index];
    return events[kFirstRetained + (recentWrite + index - kFirstRetained) % kRecentCapacity];
  }

  void push(Event event) {
    if (frozen || event.port >= 2)
      return;
    PortStats &p = ports[event.port];
    const Kind kind = static_cast<Kind>(event.kind);
    if (kind == Kind::AutoBegin)
      ++p.scans;
    if (kind == Kind::AutoProbe && event.stage == 0) {
      ++p.probes;
      if (!(event.flags & Accepted))
        ++p.rejects;
    }
    if (kind == Kind::AutoEnd && (event.aux & 0xFF) != 0 && !(event.flags & Accepted))
      ++p.otherSelections;
    if (kind == Kind::Spi) {
      ++p.spi;
      const bool valid = (event.flags & Complete) != 0;
      if (event.count < 3 || event.rx[1] == 0xFF || !(event.flags & (Sync5A | Sync00)))
        ++p.badSpi;
      // Retain every negotiation command, the first 8 polls, mode/validity
      // edges, then 10 Hz samples. Counts still include every transaction.
      if (event.count > 1 && event.tx[1] == 0x42) {
        const uint8_t type = event.count > 1 ? event.rx[1] : 0xFF;
        const bool edge = type != lastPollType[event.port] || valid != lastPollValid[event.port];
        if (pollSamples[event.port] >= 8 && !edge &&
            uint32_t(event.us - lastPollUs[event.port]) < kPollSampleUs) {
          ++p.sampledOut;
          return;
        }
        if (pollSamples[event.port] < 8)
          ++pollSamples[event.port];
        lastPollUs[event.port] = event.us;
        lastPollType[event.port] = type;
        lastPollValid[event.port] = valid;
      }
    }
    if (kind == Kind::Decoded) {
      const uint16_t state = uint16_t(event.aux) | event.flags;
      if (state == lastDecoded[event.port])
        return;
      lastDecoded[event.port] = state;
    }
    event.sequence = ++attempted;
    if (event.count > kWireBytes) {
      event.count = kWireBytes;
      event.flags |= Truncated;
    }
    if (count == kCapacity) {
      ++dropped;
      // Preserve startup evidence and the newest manual comparison. A tester
      // navigating the menus must not lose every later probe to a full log.
      events[kFirstRetained + recentWrite] = event;
      recentWrite = (recentWrite + 1) % kRecentCapacity;
      return;
    }
    events[count++] = event;
  }
};

} // namespace psx_capture
