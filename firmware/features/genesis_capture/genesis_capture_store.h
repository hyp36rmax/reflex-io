#pragma once
#include <stdint.h>
#include <string.h>

namespace genesis_capture {
constexpr uint16_t kCapacity = 256, kFirstRetained = 64;
constexpr uint16_t kRecentCapacity = kCapacity - kFirstRetained;
constexpr uint8_t kEdges = 12, kPages = 8;
constexpr uint16_t kHeaderSectors = 4, kFooterSectors = 2;
constexpr uint32_t kReportFileBytes = uint32_t(kHeaderSectors + kCapacity + kFooterSectors) * 512;
enum class Kind : uint8_t { AutoBegin = 1, AutoEnd, AutoSample, Library, Decoded, Selection, Manual, Reenum };
enum Flag : uint16_t { Truncated = 1, TimeOverflow = 2, IdCandidate = 4, MarkerValid = 8,
                      Confirmed = 16, HotSwap = 32, ManualPhase = 64, PartialPins = 128 };
struct Edge { uint16_t us = 0; uint8_t high = 0, reserved = 0; };
struct Page { uint16_t us = 0; uint8_t data = 0, high = 0; };
// Only normalized controller pins/buttons: no serials, settings or unrelated GPIO.
struct Event {
  uint32_t sequence = 0, us = 0, durationUs = 0, highGapUs = 0;
  uint16_t flags = 0, digital = 0xFFFF;
  uint8_t kind = 0, port = 0, stage = 0, mode = 0;
  uint8_t observed = 0, latched = 0, hits = 0, misses = 0;
  uint8_t count = 0, tap = 0, edgeCount = 0, pageCount = 0;
  Edge edges[kEdges] = {};
  Page pages[kPages] = {};
};
static_assert(sizeof(Event) == 112, "Genesis capture RAM contract");
struct PortStats {
  uint32_t scans = 0, transactions = 0, noController = 0, idCandidates = 0;
  uint32_t markers = 0, confirmed = 0, truncated = 0, sampledOut = 0;
};
struct Store {
  Event events[kCapacity] = {}, previous[2] = {};
  PortStats ports[2] = {};
  uint32_t attempted = 0, dropped = 0, startedUs = 0, frozenUs = 0, lastSampleUs[2] = {};
  uint16_t count = 0, recentWrite = 0;
  uint8_t firstPolls[2] = {};
  bool frozen = false, previousValid[2] = {};
  uint32_t decoded[2][6] = {};
  bool decodedValid[2][6] = {};
  void freeze(uint32_t now) { frozenUs = now; frozen = true; }
  const Event &record(uint16_t index) const {
    if (count < kCapacity || index < kFirstRetained) return events[index];
    return events[kFirstRetained + (recentWrite + index - kFirstRetained) % kRecentCapacity];
  }
  static bool sameState(const Event &a, const Event &b) {
    if (a.kind != b.kind || a.stage != b.stage || a.mode != b.mode || a.flags != b.flags ||
        a.digital != b.digital || a.observed != b.observed || a.latched != b.latched ||
        a.hits != b.hits || a.misses != b.misses || a.count != b.count || a.tap != b.tap ||
        a.pageCount != b.pageCount) return false;
    for (uint8_t i = 0; i < a.pageCount; ++i)
      if (a.pages[i].data != b.pages[i].data || a.pages[i].high != b.pages[i].high) return false;
    return true;
  }
  void push(Event event) {
    if (frozen || event.port >= 2) return;
    PortStats &p = ports[event.port];
    if (Kind(event.kind) == Kind::AutoBegin) ++p.scans;
    if (Kind(event.kind) == Kind::Decoded && event.stage < 6) {
      const uint32_t value = uint32_t(event.digital) | (uint32_t(event.observed) << 16) |
                             (uint32_t(event.latched) << 24);
      if (decodedValid[event.port][event.stage] && decoded[event.port][event.stage] == value) return;
      decodedValid[event.port][event.stage] = true;
      decoded[event.port][event.stage] = value;
    }
    if (Kind(event.kind) == Kind::Library) {
      ++p.transactions;
      if (!event.count) ++p.noController;
      if (event.flags & IdCandidate) ++p.idCandidates;
      if (event.flags & MarkerValid) ++p.markers;
      if (event.flags & Confirmed) ++p.confirmed;
      if (event.flags & Truncated) ++p.truncated;
      // Count every transaction; retain first16, state/page edges and 10 Hz.
      if (firstPolls[event.port] >= 16 && previousValid[event.port] &&
          sameState(event, previous[event.port]) &&
          uint32_t(event.us - lastSampleUs[event.port]) < 100000) {
        ++p.sampledOut;
        return;
      }
      if (firstPolls[event.port] < 16) ++firstPolls[event.port];
      previous[event.port] = event;
      previousValid[event.port] = true;
      lastSampleUs[event.port] = event.us;
    }
    event.sequence = ++attempted;
    if (count == kCapacity) {
      ++dropped;
      events[kFirstRetained + recentWrite] = event;
      recentWrite = (recentWrite + 1) % kRecentCapacity;
    } else events[count++] = event;
  }
};
} // namespace genesis_capture
