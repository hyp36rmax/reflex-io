#pragma once

#include "../saturn/saturn_mouse_reader.h"

// Require two complete transactions; a partial 0/B prefix is not an identity.
template <typename Bus>
bool confirmAutoSaturnMouse(Bus& bus) {
  SaturnMouseReport report;
  if (!readSaturnMouseReport(bus, report)) return false;
  bus.waitUs(100);
  return readSaturnMouseReport(bus, report);
}
