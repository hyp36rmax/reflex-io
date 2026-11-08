#pragma once

#include "../product_config.h"
#include <stdint.h>

#if ADAPT_POLICY_COOPERATIVE_OLED
#include <SSD1306Ascii/SSD1306AsciiWire.h>
#include <U8g2lib.h>

// Preserve the existing renderer APIs; runtime writes update RAM only.
class CooperativeOledAscii : public SSD1306AsciiWire {
 public:
  explicit CooperativeOledAscii(decltype(Wire)& wire) : SSD1306AsciiWire(wire) {}
  void flush();
 protected:
  void writeDisplay(uint8_t value, uint8_t mode) override;
};

class CooperativeOledU8g2 : public U8G2_SSD1306_128X64_NONAME_F_HW_I2C {
 public:
  CooperativeOledU8g2();
};

bool cooperativeOledActive();
void startCooperativeOled();
void serviceCooperativeOled();
#else
inline bool cooperativeOledActive() { return false; }
inline void startCooperativeOled() {}
inline void serviceCooperativeOled() {}
#endif

// Existing blocking UI waits must keep the cooperative display progressing.
void delayWithCooperativeOled(uint32_t durationMs);
