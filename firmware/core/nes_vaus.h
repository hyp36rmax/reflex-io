#pragma once

#include <stdint.h>

struct NesVausSample {
  uint16_t raw = 0;
  uint8_t paddle = 0;
  bool fire = false;
};

// NESdev's Expansion port reference documents that controller lines are
// inverted before reaching the CPU. The physical HDMI adapter exposes the raw
// controller level, so Vaus's nine bits are already MSB-first raw data here.
// A strobe starts its slow ADC conversion; the next poll reads that result.
class NesVausDecoder {
 public:
  static constexpr uint32_t conversion_ms = 8;

  static uint16_t decodeWireMsbFirst(uint16_t wireBits) {
    return wireBits & 0x1ff;
  }

  void beginConversion(uint32_t nowMs) {
    readyAtMs_ = nowMs + conversion_ms;
    primed_ = true;
  }

  bool ready(uint32_t nowMs) const {
    return primed_ && static_cast<int32_t>(nowMs - readyAtMs_) >= 0;
  }

  bool primed() const { return primed_; }

  bool readPreviousAndBegin(uint32_t nowMs, uint16_t wireBits, bool fire,
                            NesVausSample& sample) {
    if (!ready(nowMs)) {
      return false;
    }
    sample.raw = decodeWireMsbFirst(wireBits);
    sample.paddle = static_cast<uint8_t>(sample.raw >> 1);
    sample.fire = fire;
    beginConversion(nowMs);
    return true;
  }

  void reset() { primed_ = false; }

 private:
  uint32_t readyAtMs_ = 0;
  bool primed_ = false;
};

// Hardware-independent transport used by the input module and host fixture.
// readBit returns the raw physical D1-2 level; pulseClock and pulseStrobe own
// their GPIO timing so the decoder never blocks for ADC conversion.
template <typename ReadBit, typename ReadFire, typename PulseClock,
          typename PulseStrobe>
bool pollNesVausTransport(NesVausDecoder& decoder, uint32_t nowMs,
                          ReadBit readBit, ReadFire readFire,
                          PulseClock pulseClock, PulseStrobe pulseStrobe,
                          NesVausSample& sample) {
  if (!decoder.primed()) {
    pulseStrobe();
    decoder.beginConversion(nowMs);
    return false;
  }
  if (!decoder.ready(nowMs)) {
    return false;
  }
  uint16_t wireBits = 0;
  for (uint8_t bit = 0; bit < 9; ++bit) {
    wireBits = static_cast<uint16_t>((wireBits << 1) |
                                     (readBit() ? 1 : 0));
    pulseClock();
  }
  if (!decoder.readPreviousAndBegin(nowMs, wireBits, readFire(), sample)) {
    return false;
  }
  pulseStrobe();
  return true;
}
