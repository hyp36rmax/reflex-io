#pragma once

#include <stdint.h>

namespace psx_fishing {

// Agetec captures contain brief positive reel updates, typically 109 ms apart,
// with zeros between them. This is a chosen speed envelope, not a claim that
// byte 12 is a calibrated RPM value. At 1x the observed maximum (14) is full.
class ReelSpeed {
 public:
  static constexpr uint32_t holdUs = 125000;
  static constexpr uint32_t releaseUs = 200000;
  static constexpr uint8_t fullScaleRaw = 14;

  void reset() { raw_ = 0; lastUpdateUs_ = 0; }

  void sample(uint8_t raw, uint32_t nowUs) {
    if (raw != 0) {
      raw_ = raw;
      lastUpdateUs_ = nowUs;
    }
  }

  uint8_t value(uint32_t nowUs, uint8_t scaleQuarterUnits = 4) {
    const uint32_t age = nowUs - lastUpdateUs_;
    if (raw_ == 0 || age >= releaseUs) {
      raw_ = 0;
      return 0;
    }
    uint32_t speed = uint32_t(raw_) * scaleQuarterUnits * 255 /
                     (uint32_t(fullScaleRaw) * 4);
    if (speed > 255) speed = 255;
    if (age > holdUs) speed = speed * (releaseUs - age) / (releaseUs - holdUs);
    return static_cast<uint8_t>(speed);
  }

 private:
  uint32_t lastUpdateUs_ = 0;
  uint8_t raw_ = 0;
};

}  // namespace psx_fishing
