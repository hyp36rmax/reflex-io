#pragma once
#include "saturnlib_transport.h"
#include "../../features/genesis_capture/genesis_capture.h"

inline void saturnCaptureUpdate(SaturnPort &bus, uint8_t port, uint8_t stage,
                                genesis_capture::Pins pins, uint8_t mode) {
  genesisCaptureBegin(port, stage, pins, mode);
  bus.update();
  genesisCaptureEnd(bus);
}
