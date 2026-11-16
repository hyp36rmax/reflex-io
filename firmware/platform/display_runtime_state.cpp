#include "display_runtime_state.h"
#include "u8g2_oled_mirror.h"

#ifdef USE_I2C_DISPLAY
#if ADAPT_POLICY_COOPERATIVE_OLED
CooperativeOledAscii display(Wire);
CooperativeOledU8g2 u8g2;
#else
SSD1306AsciiWire display(Wire);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
#endif
namespace {
// Defined after u8g2 in this file, so it runs after u8g2's constructor.
struct U8g2OledMirrorInstall {
  U8g2OledMirrorInstall() { installU8g2OledMirror(u8g2.getU8x8()); }
} u8g2OledMirrorInstall;
}  // namespace
#else
// Some dormant menu handlers retain a reference to the legacy display object
// even in a headless product. Provide construction-only storage; all display
// initialization and I2C access remain gated by USE_I2C_DISPLAY.
#include <Wire.h>
#include <SSD1306Ascii/SSD1306AsciiWire.h>
SSD1306AsciiWire display(Wire);
#endif

namespace {
bool displayPresenceChecked = false;
bool displayPresent = false;
}

bool displayRuntimeAvailable() {
#ifdef USE_I2C_DISPLAY
  if (!displayPresenceChecked) {
    beginDisplayWire();
    Wire.beginTransmission(I2C_ADDRESS);
    displayPresent = Wire.endTransmission() == 0;
    endDisplayWire();
    displayPresenceChecked = true;
  }
  return displayPresent;
#else
  return false;
#endif
}
