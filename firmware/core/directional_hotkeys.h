#pragma once
#include "../product_config.h"
#include "directional_hotkey_policy.h"
#include <cstring>
#include "device_mode.h"
#include "controller_state.h"
#include "settings_store.h"
#include "../output/output_runtime_state.h"

#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
extern uint8_t active_direction_hotkeys[3];
#if defined(ADAPT_BRIDGE_JVS_TO_USB)
// Panel buttons per player on the attached JVS board (input/jvs/).
uint8_t jvsHotkeyPanelButtons();
#endif
inline bool directionalTypeNamed(const char* a, const char* b) { return a && std::strcmp(a,b)==0; }
inline direction_hotkey::Kind directionalHotkeyKind(DeviceEnum mode, const char* type) {
  using namespace direction_hotkey;
#if defined(ADAPT_BRIDGE_JVS_TO_USB)
  (void)mode; (void)type;
  return jvsPanelKind(jvsHotkeyPanelButtons());
#elif defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
  (void)mode; (void)type;
  return Modern;
#else
  // Devices without an independently usable Start + D-pad get no defaults.
  if (directionalTypeNamed(type,"Nunchuk") || directionalTypeNamed(type,"Nunchuck") || directionalTypeNamed(type,"GunCon") || directionalTypeNamed(type,"Guncon") ||
      directionalTypeNamed(type,"Wheel") || directionalTypeNamed(type,"Saturn Mouse") ||
      directionalTypeNamed(type,"Sega Mouse") || directionalTypeNamed(type,"SNES Mouse") ||
      directionalTypeNamed(type,"PSX Mouse") || directionalTypeNamed(type,"N64 Mouse") ||
      directionalTypeNamed(type,"3DO Mouse") || directionalTypeNamed(type,"PCE Mouse") ||
      directionalTypeNamed(type,"DC Mouse") || directionalTypeNamed(type,"CD-i Mouse")) return None;
  if (mode == RZORD_NES || mode == RZORD_PCE || mode == RZORD_JPC) return Nes;
  if (mode == RZORD_SNES) return Snes;
  if (mode == RZORD_N64) return N64;
  if (mode == RZORD_GAMECUBE) return GameCube;
  if (mode == RZORD_WII) return Wii;
  if (mode == RZORD_MEGADRIVE) return directionalTypeNamed(type,"Mega3") ? Genesis3 : Genesis6;
  if (mode == RZORD_SATURN) return Saturn;
  if (mode == RZORD_DREAMCAST) return Dreamcast;
  if (mode == RZORD_NEOGEO) return NeoGeo;
  if (mode == RZORD_JAGUAR) return Jaguar;
  if (mode == RZORD_VBOY) return VirtualBoy;
  if (mode == RZORD_3DO) return ThreeDO;
  if (mode == RZORD_GBA) return Gba;
  if (mode == RZORD_PSX || mode == RZORD_PSX_JOG || mode == RZORD_PSX_DANCE)
    return directionalTypeNamed(type,"neGcon") ? NeGcon : PlayStation;
  return None;
#endif
}
inline bool directionalHotkeysValid(DeviceEnum mode, const uint8_t* actions) {
  using namespace direction_hotkey;
  for (uint8_t i=0;i<3;++i) if (actions[i]>=Count) return false;
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  const Kind first = directionalHotkeyKind(mode, mode == RZORD_MEGADRIVE ? "Mega3" : "");
  const Kind second = directionalHotkeyKind(mode, mode == RZORD_PSX ? "neGcon" : "");
  return uniqueForKinds(actions,first,second);
#elif defined(ADAPT_BRIDGE_JVS_TO_USB)
  // A different cabinet must not turn a saved choice into a duplicate.
  (void)mode;
  return uniqueForKinds(actions,Jvs,JvsSeven) && unique(actions,JvsSix);
#else
  return unique(actions, directionalHotkeyKind(mode, ""));
#endif
}
inline void sanitizeDirectionalHotkeys(DeviceEnum mode, PerModeSettingsRecord& s) {
#if ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
  // Retired custom assignments are inert; preserve their bytes for downgrade.
  (void)mode; (void)s;
#else
  using namespace direction_hotkey;
  uint8_t actions[3]; read(s,actions);
  if (!directionalHotkeysValid(mode,actions)) {
    // Corrupt/old conflicting profiles fail closed, not by stealing actions.
    for (auto& a : actions) a = Off;
  }
  write(s,actions);
#endif
}
bool directionalHotkeyAvailable(direction_hotkey::Action action, outputMode_t output);
direction_hotkey::Action directionalHotkeyDefault(direction_hotkey::Kind kind,
                                                 uint8_t direction, outputMode_t output,
                                                 const uint8_t* actions = nullptr);
const char* directionalHotkeyName(uint8_t action, DeviceEnum mode, const char* type,
                                uint8_t direction, outputMode_t output);
void cycleDirectionalHotkey(uint8_t* actions, uint8_t direction, DeviceEnum mode,
                           const char* type, outputMode_t output, bool forward);

uint32_t directionalMenuCombo(direction_hotkey::Action action);
void updateDirectionalHotkeys();
void applyDirectionalHotkeysToOutput(bool enabled = true);
uint32_t directionalHotkeyOutputButtons(uint8_t port);
uint32_t directionalHotkeySuppression(uint8_t port);
bool directionalHomeHotkeyEnabled(uint8_t port);
#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
// The edit token describes a connection, not merely a VID:PID. A replaced or
// reconnected controller cannot inherit an open menu's pending edits.
uint32_t directionalHotkeyEditIdentity(uint8_t port);
bool loadDirectionalHotkeySelection(uint8_t port, uint8_t* actions, uint8_t& home);
bool saveDirectionalHotkeySelection(uint8_t port, uint32_t capturedIdentity,
                                   const uint8_t* actions, uint8_t home);
const char* directionalControllerHotkeyName(uint8_t stored, uint8_t direction,
                                           uint8_t port, outputMode_t output);
void cycleDirectionalControllerHotkey(uint8_t* actions, uint8_t direction,
                                      uint8_t port, outputMode_t output, bool forward);
#endif
#endif
