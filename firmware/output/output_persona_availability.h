#pragma once

#include "output_mode.h"

// Which USB output personas an image builds. Every product shares one
// implementation of each persona; only availability differs, by ingress role
// or fitted hardware. Persisted and runtime sanitizers and the menu ask here
// instead of keeping their own lists.

// MiSTer JogCon/neGcon/Paddle/Spinner/GunCon and the rotary Mouse carry
// classic peripherals, so only classic-controller ingress builds them.
// Modes are compared as int so a stored, already range-checked value needs
// no narrowing first.
constexpr bool outputModeIsClassicMisterSpecial(int mode) {
  return mode == OUTPUT_MISTER_JOGCON || mode == OUTPUT_MISTER_NEGCON ||
         mode == OUTPUT_MISTER_PADDLE || mode == OUTPUT_MISTER_SPINNER ||
         mode == OUTPUT_MISTER_GUNCON || mode == OUTPUT_ROTARY_MOUSE;
}

// PS5 needs the fitted auth sidecar.
constexpr bool outputPersonaIsBuilt(int mode) {
#if !defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  if (mode == OUTPUT_REMOTE) return false;
  if (outputModeIsClassicMisterSpecial(mode)) return false;
#endif
#if !defined(ENABLE_OUTPUT_PS5)
  if (mode == OUTPUT_PS5) return false;
#endif
  return true;
}

// A persona this image does not build becomes the one that replaces it:
// classic MiSTer specials become MiSTer DInput, and PS5 becomes PS4. This
// mirrors outputPersonaIsBuilt(); tools/test_usb_output_parity.py checks
// that the two agree for every mode on every product.
constexpr outputMode_t foldUnbuiltOutputPersona(outputMode_t mode) {
#if !defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  if (mode == OUTPUT_REMOTE) return OUTPUT_MISTER;
  if (outputModeIsClassicMisterSpecial(mode)) return OUTPUT_MISTER;
#endif
#if !defined(ENABLE_OUTPUT_PS5)
  if (mode == OUTPUT_PS5) return OUTPUT_PS4;
#endif
  return mode;
}
