#pragma once

#include <stdint.h>

#include "device_mode.h"

// PlayStation controller timing chosen in the PSX Quick Menu, so a pad that
// misbehaves at the default rate can be tried slower. Auto keeps the timing
// the firmware picks for the attached controller; any other choice replaces it.
//
// Stored in the PSX mode record in the byte that holds the GameCube L
// assignment in the GameCube record. Every other record keeps that byte at 0,
// so records written before this setting read as Auto/Auto. A non-Auto value
// carries bit 7, which no GameCube value uses, so the byte never reads as L.
namespace psx_poll {

constexpr uint8_t kRateCount = 6;
constexpr uint8_t kGapCount = 9;
// Poll period, start to start, for Auto, 1000, 500, 250, 125 and 60 Hz.
constexpr uint16_t kRatePeriodUs[kRateCount] = {0, 1000, 2000, 4000, 8000, 16667};
// Idle time from the end of one command to the start of the next.
constexpr uint16_t kGapUs[kGapCount] = {0, 250, 500, 1000, 2000, 3000, 4000, 8000, 16000};
// One poll's commands, a DualShock 2 pressure reply included, fit in this.
constexpr uint16_t kCommandAllowanceUs = 1000;
// An SCPH-1200 DualShock's left stick stops updating with less than 2 ms
// between commands, at every poll rate (tester, 2026-10-02). Nothing the pad
// reports separates it from other DualShock pads, so all of them get this gap
// on Auto, paced like a chosen 2 ms gap: the combination that was tested.
constexpr uint16_t kDualShockGapUs = 2000;
constexpr uint32_t kDualShockPollUs = uint32_t(kDualShockGapUs) + kCommandAllowanceUs;

constexpr uint8_t kChosenTag = 0x80;
constexpr uint8_t kRateMask = 0x07;
constexpr uint8_t kGapShift = 3;
constexpr uint8_t kGapMask = 0x0F;

constexpr uint8_t rate(uint8_t raw) {
  return (raw & kChosenTag) && (raw & kRateMask) < kRateCount ? uint8_t(raw & kRateMask) : 0;
}

constexpr uint8_t gap(uint8_t raw) {
  return (raw & kChosenTag) && ((raw >> kGapShift) & kGapMask) < kGapCount
             ? uint8_t((raw >> kGapShift) & kGapMask)
             : 0;
}

constexpr uint8_t encode(uint8_t rateChoice, uint8_t gapChoice) {
  return rateChoice < kRateCount && gapChoice < kGapCount && (rateChoice || gapChoice)
             ? uint8_t(kChosenTag | rateChoice | (gapChoice << kGapShift))
             : 0;
}

constexpr uint8_t sanitize(uint8_t raw) {
  return encode(rate(raw), gap(raw));
}

// Whether this mode's record keeps the timing in the shared byte.
constexpr bool storedInRecord(DeviceEnum mode) {
#if defined(ENABLE_INPUT_PSX) && !ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  return mode == RZORD_PSX;
#else
  (void)mode;
  return false;
#endif
}

constexpr uint16_t attentionIntervalUs(uint8_t raw, uint16_t autoUs) {
  return gap(raw) ? kGapUs[gap(raw)] : autoUs;
}

// The driver waits out the gap before each command, so a chosen gap also
// stretches the poll period; a shorter period would spin in that wait.
constexpr uint32_t pollIntervalUs(uint8_t raw, uint32_t autoUs) {
  const uint32_t period = rate(raw) ? kRatePeriodUs[rate(raw)] : autoUs;
  const uint32_t floor = gap(raw) ? uint32_t(kGapUs[gap(raw)]) + kCommandAllowanceUs : 0;
  return period > floor ? period : floor;
}

}  // namespace psx_poll
