#pragma once
#include "../../input/shared/input_button_bits.h"

// Report mappers are also included by out_usb.h inside extern "C".
extern "C++" {
namespace switch_hotkey {
enum Profile : uint8_t { Pro, Nes, Snes, Genesis, N64 };
constexpr Profile profile(bool nes, bool snes, bool genesis, bool n64) {
  return nes ? Nes : snes ? Snes : genesis ? Genesis : n64 ? N64 : Pro;
}
constexpr uint32_t menuButtons(Profile p) {
  return p == Nes ? INPUT_L1 | INPUT_R1 :
         p == N64 ? INPUT_L3 : p == Genesis ? INPUT_R2 : INPUT_L2 | INPUT_R2;
}
template<typename Report> inline void applySystemButtons(Report& report, bool home,
                                                        bool capture, Profile p) {
  // NES/SNES NSO apps interpret shoulders/triggers as system controls.
  report.home = home && p != Nes && p != Snes;
  report.capture = capture && p != Nes && p != Snes;
  if (p == Nes) {
    if (home) report.r = 1;
    if (capture) report.l = 1;
  } else if (p == Snes) {
    if (home) report.zr = 1;
    if (capture) report.zl = 1;
  }
}
} // namespace switch_hotkey
} // extern "C++"
