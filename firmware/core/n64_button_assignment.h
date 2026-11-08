#pragma once

#include <stdint.h>

// Values live in the existing per-input remap record. Identity is Auto;
// explicit output slots are tagged so old controller-to-controller remaps
// remain distinguishable. No EEPROM offsets or record sizes change.
namespace n64_assignment {
enum class Family : uint8_t { DInput, Xbox, XboxClassic, PlayStation, Switch, NsoN64 };
constexpr uint8_t off = 0x7f;
constexpr uint8_t count = 10;
constexpr uint8_t sources[count] = {0, 1, 6, 4, 5, 2, 8, 3, 9, 10};
constexpr const char* names[count] = {"A", "B", "Z", "L", "R", "C-Up", "C-Down", "C-Left", "C-Right", "Start"};
constexpr bool tagged(uint8_t value) { return value >= 0x40 && value < 0x4e; }
constexpr uint8_t encode(uint8_t slot) { return slot < 14 ? uint8_t(0x40 | slot) : off; }
constexpr bool source(uint8_t slot) {
  for (auto s : sources) if (s == slot) return true;
  return false;
}
constexpr bool storedValue(uint8_t slot, uint8_t value) {
  return source(slot) && (tagged(value) || value == off);
}
inline void migrateLegacy(uint8_t* remaps, uint8_t* turbo) {
  // Previously-unused N64 backing slots make this migration idempotent.
  if (remaps[7] != 7 && remaps[8] == 8) remaps[8] = remaps[7];
  if (remaps[11] != 11 && remaps[9] == 9) remaps[9] = remaps[11];
  remaps[7] = 7; remaps[11] = 11;
  if (turbo[7] && !turbo[10]) turbo[10] = turbo[7];
  if (turbo[9] && !turbo[11]) turbo[11] = turbo[9];
  turbo[7] = 0; turbo[9] = 0;
}
constexpr uint8_t defaultTarget(Family family, bool buttons, uint8_t i) {
  if (family == Family::NsoN64) {
    constexpr uint8_t map[count] = {0,1,6,4,5,3,7,2,11,10};
    return map[i];
  }
  if (i >= 5 && i <= 8 && !buttons) return off;
  if (family == Family::DInput) return sources[i];
  if (i == 9) return 10;
  if (i == 2) return 11; // Z -> Back/Select/Minus.
  if (i == 3) return family == Family::XboxClassic ? 6 : 4;
  if (i == 4) return family == Family::XboxClassic ? 7 : 5;
  if (i < 2) {
    if (!buttons) return i;
    if (family == Family::XboxClassic) return i == 0 ? 5 : 4; // White/Black.
    return i == 0 ? 7 : 6; // RT/LT, R2/L2 or ZR/ZL; shoulders retain L/R.
  }
  constexpr uint8_t face[] = {3,0,2,1}; // Up, Down, Left, Right.
  constexpr uint8_t nintendo[] = {2,1,3,0};
  return family == Family::Switch ? nintendo[i-5] : face[i-5];
}
inline void resolve(Family family, bool buttons, const uint8_t* remaps, uint8_t* result) {
  for (uint8_t i=0; i<count; ++i) result[i] = off;
  // Each physical input resolves independently. project() ORs shared outputs,
  // so releasing one input cannot release another input's held destination.
  for (uint8_t i=0; i<count; ++i) {
    const uint8_t value = remaps[sources[i]];
    if (value == off) continue;
    if (!buttons && family != Family::NsoN64 && i>=5 && i<=8) continue;
    uint8_t target = off;
    if (tagged(value)) target = value & 0x0f;
    else {
      // Legacy C-down/right destination names used slots 7/11.
      const uint8_t legacy = value == 7 ? 8 : value == 11 ? 9 : value;
      for (uint8_t j=0; j<count; ++j)
        if (sources[j] == legacy) target = defaultTarget(family, buttons, j);
    }
    result[i] = target;
  }
}
inline uint32_t project(uint32_t physical, const uint8_t* destinations) {
  uint32_t result = physical;
  for (auto s : sources) result &= ~(uint32_t(1) << s);
  for (uint8_t i=0; i<count; ++i)
    if (destinations[i] < 14 && (physical & (uint32_t(1) << sources[i])))
      result |= uint32_t(1) << destinations[i];
  return result;
}
inline const char* outputName(Family family, uint8_t slot) {
  if (slot >= 14) return "Off";
  static const char* const xbox[] = {"A","B","X","Y","LB","RB","LT","RT","LS","RS","Start","Back","Home","Capture"};
  static const char* const xid[] = {"A","B","X","Y","Black","White","L","R","LS","RS","Start","Back","--","--"};
  static const char* const ps[] = {"Cross","Circle","Square","Triangle","L1","R1","L2","R2","L3","R3","Options","Share","PS","TPad"};
  static const char* const sw[] = {"A","B","X","Y","L","R","ZL","ZR","LS","RS","+","-","Home","Capture"};
  static const char* const nso[] = {"A","B","C-Left","C-Up","L","R","Z","C-Down","--","--","Start","C-Right","Home","Capture"};
  static const char* const hid[] = {"B1","B2","B3","B4","B5","B6","B7","B8","B9","B10","B11","B12","Home","Capture"};
  switch (family) {
    case Family::Xbox: return xbox[slot];
    case Family::XboxClassic: return xid[slot];
    case Family::PlayStation: return ps[slot];
    case Family::Switch: return sw[slot];
    case Family::NsoN64: return nso[slot];
    default: return hid[slot];
  }
}
} // namespace n64_assignment
