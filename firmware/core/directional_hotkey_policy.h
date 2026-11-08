#pragma once
#include <stdint.h>

// Stored semantic actions, never output button combinations. Zero means the
// controller-type default; 0x80 tagging distinguishes retired EEPROM bytes.
namespace direction_hotkey {
// LbRb/LtRt press both shoulders or both triggers (NSO NES L+R, NSO Pro
// ZL+ZR suspend menu). New actions append so stored values never shift.
enum Action : uint8_t { Auto, Off, A, B, X, Y, LB, RB, LT, RT, L3, R3,
                       Select, Capture, QuickMenu, SystemMenu, LbRb, LtRt, NsoMenu, Count };
// Jvs is an eight-or-more button panel; JvsSeven lacks K4 and JvsSix (six or
// fewer) lacks P4 and K4.
enum Kind : uint8_t { None, Nes, Snes, N64, GameCube, Wii, Genesis3, Genesis6,
                     Saturn, Dreamcast, NeoGeo, Jaguar, VirtualBoy, ThreeDO,
                     PlayStation, NeGcon, Gba, Jvs, Modern, JvsSeven, JvsSix };
constexpr Kind jvsPanelKind(uint8_t panelButtons) {
  return panelButtons >= 8 ? Jvs : panelButtons == 7 ? JvsSeven : JvsSix;
}
constexpr bool isJvsKind(Kind k) { return k == Jvs || k == JvsSeven || k == JvsSix; }
enum Direction : uint8_t { Left, Right, Up, Directions };
constexpr uint8_t encode(uint8_t a) { return uint8_t(0x80 | (a < Count ? a : uint8_t(Auto))); }
constexpr uint8_t decode(uint8_t b) { return b >= 0x80 && b < 0x80 + Count ? uint8_t(b & 0x7f) : uint8_t(Auto); }
constexpr bool needsSelectShortcut(Kind k) {
  return k == N64 || k == GameCube || k == Genesis3 || k == Genesis6 ||
         k == Saturn || k == Dreamcast || k == NeGcon;
}
// Start+Up on a native NSO identity with no X button of its own. NES and
// Genesis borrow their X wire bit (Genesis Y) for the host X action. N64 has
// no spare bit: every Pro position is a C, Z or ZR button, so Up stays empty
// there (owner, 2026-10-18). Minus is useless on Genesis and is C-Right on N64.
enum NsoUp : uint8_t { NsoUpDefault, NsoUpX, NsoUpNone };
constexpr Action defaultAction(Kind k, uint8_t direction, bool switchDefaults = false,
                               NsoUp nsoUp = NsoUpDefault) {
  if (k == None || direction >= Directions) return Off;
  if (k == Modern) return Off; // Resolved from the connected controller's capabilities.
  if (switchDefaults) return direction == Left ? NsoMenu : direction == Right ? Capture :
                             nsoUp == NsoUpX ? X : nsoUp == NsoUpNone ? Off :
                             needsSelectShortcut(k) ? Select : Off;
#if defined(ADAPT_BRIDGE_JVS_TO_USB)
  // Start+Left/Right supply a JVS panel's missing P4 (L1) and K4 (L2).
  // Other products never see the panel kinds and keep their code unchanged.
  if (isJvsKind(k)) return direction == Up ? Select :
                           direction == Left ? (k == JvsSix ? LB : L3) : (k == Jvs ? R3 : LT);
#else
  if (k == Jvs) return direction == Up ? Select : direction == Left ? L3 : R3;
#endif
  if (direction == Up) {
    return needsSelectShortcut(k) ? Select : Capture;
  }
  if (k == N64 || k == GameCube || k == Wii || k == PlayStation)
    return direction == Left ? L3 : R3;
  if (k == Snes || k == Genesis6 || k == Saturn || k == VirtualBoy ||
      k == ThreeDO || k == NeGcon || k == Gba)
    return direction == Left ? LT : RT;
  return direction == Left ? LB : RB;
}
constexpr Action resolve(uint8_t stored, Kind k, uint8_t d, bool switchDefaults = false,
                         NsoUp nsoUp = NsoUpDefault) {
  return stored == Auto ? defaultAction(k, d, switchDefaults, nsoUp) : stored < Count ? Action(stored) : Off;
}
inline Action resolveProfile(const uint8_t* actions, Kind k, uint8_t d, bool switchDefaults,
                             NsoUp nsoUp = NsoUpDefault) {
  const Action action = resolve(actions[d], k, d, switchDefaults, nsoUp);
  // A new output default must yield to an existing explicit assignment.
  if (switchDefaults && actions[d] == Auto && action != Off)
    for (uint8_t i=0; i<Directions; ++i)
      if (i != d && actions[i] != Auto && actions[i] == action) return Off;
  return action;
}
// Unknown controls do not count as missing. In particular, a quiet/unpressed
// button must never enable a shortcut; these masks describe capabilities.
constexpr Action modernDefault(uint8_t d, uint32_t known, uint32_t present) {
  const uint32_t bit = d == Left ? (1u << 8) : d == Right ? (1u << 9) : (1u << 13);
  return d >= Directions || !(known & bit) || (present & bit) ? Off :
         d == Left ? L3 : d == Right ? R3 : Capture;
}
constexpr Action resolveModern(uint8_t stored, uint8_t d, uint32_t known, uint32_t present) {
  return stored == Auto ? modernDefault(d, known, present) : stored < Count ? Action(stored) : Off;
}
enum HomePreference : uint8_t { HomeAuto, HomeOff, HomeOn };
constexpr bool modernHomeEnabled(uint8_t stored, uint32_t known, uint32_t present) {
  return stored == HomeOn || (stored == HomeAuto && (known & (1u << 12)) && !(present & (1u << 12)));
}
inline bool uniqueModern(const uint8_t* actions, uint32_t known, uint32_t present) {
  for (uint8_t i=0; i<Directions; ++i) {
    if (actions[i] >= Count) return false;
    const Action a = resolveModern(actions[i], i, known, present);
    for (uint8_t j=0; j<i; ++j)
      if (a != Off && a == resolveModern(actions[j], j, known, present)) return false;
  }
  return true;
}
constexpr uint32_t button(Action a) {
  return a >= A && a <= R3 ? uint32_t(1) << (a - A) :
         a == Select ? uint32_t(1) << 11 : a == Capture ? uint32_t(1) << 13 :
         a == LbRb ? (uint32_t(1) << (LB - A)) | (uint32_t(1) << (RB - A)) :
         a == LtRt ? (uint32_t(1) << (LT - A)) | (uint32_t(1) << (RT - A)) : 0;
}
inline bool unique(const uint8_t* actions, Kind k) {
  for (uint8_t i=0; i<Directions; ++i) for (uint8_t j=0; j<i; ++j) {
    const Action a = resolve(actions[i], k, i);
    if (a != Off && a == resolve(actions[j], k, j)) return false;
  }
  return true;
}
// Reject a choice if it collides on ANY subtype sharing this mode. Subtype
// changes must never silently steal a previously assigned direction.
inline bool uniqueForKinds(const uint8_t* actions, Kind first, Kind second) {
  return unique(actions, first) && unique(actions, second);
}
template<typename Record> inline void read(const Record& s, uint8_t* a) {
  a[Left] = decode(s.reserved_musical_buttons);
  a[Right] = decode(s.reserved_mouse_sensitivity);
  a[Up] = decode(s.reserved_mouse_stick);
}
template<typename Record> inline void write(Record& s, const uint8_t* a) {
  s.reserved_musical_buttons = encode(a[Left]);
  s.reserved_mouse_sensitivity = encode(a[Right]);
  s.reserved_mouse_stick = encode(a[Up]);
}
} // namespace direction_hotkey
