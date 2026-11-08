#pragma once
#include "../../features/psx_capture/psx_capture.h"

#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
// Extend the existing driver at its virtual boundaries, leaving vendored
// protocol parsing, SPI settings and delay-based acknowledgement untouched.
template <class Base> class PsxCaptureDriver final : public Base {
  psx_capture::Event event;
  const uint8_t port;
  const uint8_t transport;
  uint16_t wireCount = 0;

protected:
  byte shiftInOut(const byte out) override {
    const byte in = Base::shiftInOut(out);
    if (wireCount < psx_capture::kWireBytes) {
      event.tx[wireCount] = out;
      event.rx[wireCount] = in;
    }
    ++wireCount;
    return in;
  }

public:
  PsxCaptureDriver(uint8_t spi, uint8_t att, uint8_t ack, uint8_t cmd, uint8_t dat, uint8_t clk,
                   uint8_t portIndex, uint8_t transportIndex = 1)
      : Base(spi, att, ack, cmd, dat, clk), port(portIndex), transport(transportIndex) {}
  void attention() override {
    event = psx_capture::Event{};
    event.kind = uint8_t(psx_capture::Kind::Spi);
    event.port = port;
    event.stage = transport;
    event.us = micros();
    event.gapUs = this->getAttentionInterval();
    event.aux = uint32_t(event.us - this->lastCmdTime);
    wireCount = 0;
    Base::attention();
  }
  void noAttention() override {
    Base::noAttention();
    // Memory-card probes can also occur during controller setup. Keep every
    // non-controller address out of the export, including explicit Deep.
    if (wireCount == 0 || (event.tx[0] != 0x01 && event.tx[0] != 0x61))
      return;
    // The ACK driver also serves memory cards. Export it only within the
    // explicitly requested controller deep probe, never card operations.
    if (transport == 2 && !psxCaptureDeepActive())
      return;
    if (transport == 2)
      event.flags |= psx_capture::PhysicalAck;
    event.durationUs = uint32_t(micros() - event.us);
    event.count =
        wireCount > psx_capture::kWireBytes ? psx_capture::kWireBytes : uint8_t(wireCount);
    if (wireCount > psx_capture::kWireBytes)
      event.flags |= psx_capture::Truncated;
    if (wireCount >= 3) {
      if (event.rx[0] == 0xFF)
        event.flags |= psx_capture::HeaderFF;
      if (event.rx[2] == 0x5A)
        event.flags |= psx_capture::Sync5A;
      if (event.rx[2] == 0)
        event.flags |= psx_capture::Sync00;
      const bool fixedIr = event.tx[0] == 0x61 &&
          (event.tx[1] == 0x0f || event.tx[1] == 0x06 || event.tx[1] == 0x04);
      const uint16_t wanted = fixedIr ? 7 : 3 + Base::getReplyLength(event.rx);
      if (Base::isValidReply(event.rx) && wireCount == wanted)
        event.flags |= psx_capture::Complete;
    }
    psxCaptureRecord(event);
  }
};
#endif
