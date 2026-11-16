#pragma once

#include <stdint.h>

// MiSTer GunCon output (GUNCON_FORCE_MODE 3) maps the GunCon 1's raw counts on
// the screen onto 0..32767, the range Main's default calibration for the
// ReflexPSGun identity expects (QUIRK_LIGHTGUN, input.cpp). Until 2026-11-11
// it sent the raw counts, so a core showed the whole screen in about 1% of
// the axis until the gun was calibrated there. A per-core calibration still
// refines this window.
//
// The window starts from Main's own PSX calibration of this gun on the
// owner's CRT (bench MiSTer, 2026-07-18, raw counts): x 89..444, y 32..244.
// Other cores calibrated then mostly fall within a few percent (median
// x 82..438, y 34..239; SNES y starts at 71, Atari 7800 x at 123, NES x ends
// at 406). With that window and
// no calibration the owner's PSX shots (2026-11-11) landed 5% left of aim at
// the left edge, about 2.5% left at the centre and on aim at the right; with
// x from 80, 2.5% and 1.25% left. Both fit the core placing X = 0 5% left of
// the picture, with the gun reading 89 at the picture's left edge, so x
// starts at 71, which puts all three on aim (the July NES calibration started
// at 72). The gun times the light from the video sync, so the window follows
// the core's picture timing, not the TV: other cores and PAL pictures (y up
// to 295) still need a per-core calibration, as a GunCon 2 does.
//
// With no light the last position is held for kNoLightHoldMs, then (0, 0) is
// sent, which Main pins to the screen edge: off-screen, for reloading. Main
// gives a GunCon 2 the same 50 ms. The hold used to be 254 polls.
namespace guncon_screen {

constexpr int32_t kRawXMin = 71;
constexpr int32_t kRawXMax = 444;
constexpr int32_t kRawYMin = 32;
constexpr int32_t kRawYMax = 244;
constexpr uint16_t kScaledMax = 32767;
constexpr uint32_t kNoLightHoldMs = 50;

constexpr uint16_t scaleAxis(int32_t raw, int32_t rawMin, int32_t rawMax) {
  return raw <= rawMin ? 0
       : raw >= rawMax ? kScaledMax
       : (uint16_t)(((raw - rawMin) * (int32_t)kScaledMax + (rawMax - rawMin) / 2) /
                    (rawMax - rawMin));
}

constexpr uint16_t scaleX(int32_t raw) { return scaleAxis(raw, kRawXMin, kRawXMax); }
constexpr uint16_t scaleY(int32_t raw) { return scaleAxis(raw, kRawYMin, kRawYMax); }

static_assert(scaleX(kRawXMin) == 0 && scaleX(kRawXMax) == kScaledMax, "X window ends");
static_assert(scaleY(kRawYMin) == 0 && scaleY(kRawYMax) == kScaledMax, "Y window ends");

}  // namespace guncon_screen
