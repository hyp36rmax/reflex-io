#pragma once
#include "genesis_capture_store.h"

namespace genesis_capture {
struct Pins { uint8_t d0, d1, d2, d3, th, tr, tl; };
// Bounded observation of existing reads/writes. No extra reads, waits or retries.
struct BusTrace {
  Event event{};
  Pins pins{};
  uint32_t highSince[2] = {};
  bool highKnown[2] = {}, lastHigh[2] = {}, active = false;
  bool gapRecorded = false, currentPageRecorded = false;
  void begin(uint8_t port, uint8_t stage, uint8_t mode, Pins wiring, uint32_t now) {
    active = port < 2;
    if (!active) return;
    event = Event{};
    event.port = port; event.stage = stage; event.mode = mode;
    event.kind = uint8_t(Kind::Library); event.us = now;
    pins = wiring; gapRecorded = false; currentPageRecorded = false;
  }
  uint16_t relative(uint32_t now) {
    const uint32_t elapsed = now - event.us;
    if (elapsed > 65535) { event.flags |= TimeOverflow; return 65535; }
    return uint16_t(elapsed);
  }
  void put(uint8_t pin, bool high, uint32_t now) {
    if (!active || pin != pins.th) return;
    const uint8_t port = event.port;
    // A repeated TH-high write does not restart the physical high interval.
    if (!gapRecorded) {
      if (highKnown[port] && lastHigh[port]) event.highGapUs = now - highSince[port];
      gapRecorded = true;
    }
    if (high && (!highKnown[port] || !lastHigh[port])) highSince[port] = now;
    highKnown[port] = true; lastHigh[port] = high;
    if (event.edgeCount == kEdges) { event.flags |= Truncated; return; }
    event.edges[event.edgeCount++] = {relative(now), uint8_t(high), 0};
  }
  void sample(uint32_t gpio, uint32_t now) {
    currentPageRecorded = false;
    if (!active) return;
    if (event.pageCount == kPages) { event.flags |= Truncated; return; }
    const uint8_t data = ((gpio >> pins.d0) & 1) | (((gpio >> pins.d1) & 1) << 1) |
        (((gpio >> pins.d2) & 1) << 2) | (((gpio >> pins.d3) & 1) << 3) |
        (((gpio >> pins.tl) & 1) << 4) | (((gpio >> pins.tr) & 1) << 5);
    event.pages[event.pageCount++] = {relative(now), data, uint8_t((gpio >> pins.th) & 1)};
    currentPageRecorded = true;
  }
  void observeBit(uint8_t pin, bool value) {
    if (!active || !currentPageRecorded || (pin != pins.tl && pin != pins.tr)) return;
    // Use the library's subsequent TL/TR read, rather than assuming that the
    // earlier bulk snapshot represents the exact six bits fed to its decoder.
    Page &page = event.pages[event.pageCount - 1];
    const uint8_t mask = pin == pins.tl ? 16 : 32;
    page.data = (page.data & ~mask) | (value ? mask : 0);
  }
};
} // namespace genesis_capture
