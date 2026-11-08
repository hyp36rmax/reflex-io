#include "directional_hotkeys.h"
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
#include "controller_frame_state.h"
#include "controller_delivery_state.h"
#include "controller_output_cache_state.h"
#include "device_runtime_state.h"
#include "n64_assignment_runtime.h"
#include "../menu/menu_runtime_state.h"
#include "../menu/menu_ui_state.h"
#include "../input/shared/input_button_bits.h"
#include "../output/switch/output_switch_hotkey_policy.h"
#include <cstring>
#include <cstdio>
#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
#include "modern2usb_hotkey_profiles.h"
#endif

using namespace direction_hotkey;

namespace {
uint32_t outputs[MAX_USB_OUT] = {}, suppressed[MAX_USB_OUT] = {};
uint32_t readyOutputs[MAX_USB_OUT] = {};
#if ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
uint32_t previousPhysical[MAX_USB_OUT] = {};
#endif
constexpr uint32_t directions[] = {INPUT_PAD_L, INPUT_PAD_R, INPUT_PAD_U};
bool switchDefaultsActive(outputMode_t output) {
  return (output == OUTPUT_SWITCHPRO || output == OUTPUT_SWITCH2PRO ||
          output == OUTPUT_SWITCH) &&
         (!is_auto_output_mode_selected() || autoDetectState == AUTO_STATE_SWITCH);
}
// Native NSO NES, Genesis and N64 controllers have no X button: NES and
// Genesis borrow their X wire bit for Start+Up, and N64 leaves Up empty.
NsoUp nsoUpAction(outputMode_t output) {
  if (!switchDefaultsActive(output) || output != OUTPUT_SWITCHPRO || !is_nso_special_active())
    return NsoUpDefault;
  if (output_uses_switch_n64_profile()) return NsoUpNone;
  return output_uses_switch_nes_profile() || output_uses_switch_genesis_profile() ? NsoUpX : NsoUpDefault;
}
constexpr uint32_t kAllDirections = INPUT_PAD_U | INPUT_PAD_D | INPUT_PAD_L | INPUT_PAD_R;
// 1 + the direction of the shortcut each port holds, 0 for none.
uint8_t engagedDirection[MAX_USB_OUT] = {};
switch_hotkey::Profile switchHotkeyProfile(outputMode_t output) {
  if (output != OUTPUT_SWITCHPRO) return switch_hotkey::Pro;
  return switch_hotkey::profile(output_uses_switch_nes_profile(), output_uses_switch_snes_profile(),
                               output_uses_switch_genesis_profile(), output_uses_switch_n64_profile());
}

#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
struct ControllerShortcutSlot {
  Modern2UsbHotkeyProfile profile{};
  uint32_t known = 0, present = 0, token = 0, connection = 0, revision = 0;
  bool valid = false;
};
ControllerShortcutSlot controllerSlots[MAX_USB_OUT]{};
uint32_t nextEditToken = 0;

ControllerShortcutSlot* refreshControllerShortcuts(uint8_t port) {
  if (port >= MAX_USB_OUT) return nullptr;
  auto& slot = controllerSlots[port];
  Modern2UsbHotkeyProfileKey key{};
  uint32_t known = 0, present = 0, token = 0;
  if (port >= max_devices || !controllerFrameConst(port).connected ||
      !modern2usbHotkeyKeyForController(port, key) ||
      !modern2usbHotkeyControllerCapabilities(port, known, present, token) || !token) {
    slot = ControllerShortcutSlot{};
    return nullptr;
  }
  const auto revision = modern2usbHotkeyProfilesRevision();
  const bool changedConnection = !slot.valid || token != slot.connection ||
      std::memcmp(&key, &slot.profile.key, sizeof(key)) != 0;
  if (changedConnection || revision != slot.revision) {
    slot.profile = Modern2UsbHotkeyProfile{};
    slot.profile.key = key;
    modern2usbHotkeyProfileRead(key, slot.profile);
    slot.profile.key = key;
    slot.valid = true;
    slot.connection = token;
    if (changedConnection) {
      if (++nextEditToken == 0) ++nextEditToken;
      slot.token = nextEditToken;
    }
    slot.revision = revision;
  }
  slot.known = known;
  slot.present = present;
  return &slot;
}
bool controllerCanChord(const ControllerShortcutSlot& slot) {
  const uint32_t required = INPUT_START | INPUT_PAD_U | INPUT_PAD_D | INPUT_PAD_L | INPUT_PAD_R;
  return (slot.known & slot.present & required) == required;
}
#endif
Action controllerAction(uint8_t port, Kind kind, uint8_t d) {
#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
  (void)kind;
  const auto* slot = refreshControllerShortcuts(port);
  if (!slot || !controllerCanChord(*slot) ||
      !uniqueModern(slot->profile.actions, slot->known, slot->present)) return Off;
  return resolveModern(slot->profile.actions[d], d, slot->known, slot->present);
#else
  (void)port;
  const outputMode_t output = get_effective_output_mode();
#if ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
  return menu_home_hotkey ? defaultAction(kind, d, switchDefaultsActive(output), nsoUpAction(output)) : Off;
#else
  return resolveProfile(active_direction_hotkeys, kind, d, switchDefaultsActive(output), nsoUpAction(output));
#endif
#endif
}
}

Action directionalHotkeyDefault(Kind kind, uint8_t d, outputMode_t output, const uint8_t* actions) {
  const bool useSwitchDefaults = switchDefaultsActive(output);
  const NsoUp nsoUp = nsoUpAction(output);
#if ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
  (void)actions;
  return defaultAction(kind, d, useSwitchDefaults, nsoUp);
#else
  if (!actions || d >= Directions) return defaultAction(kind, d, useSwitchDefaults, nsoUp);
  uint8_t candidate[Directions]; std::memcpy(candidate, actions, Directions);
  candidate[d] = Auto;
  return resolveProfile(candidate, kind, d, useSwitchDefaults, nsoUp);
#endif
}

bool directionalHotkeyAvailable(Action a, outputMode_t output) {
  if (a <= Off || a == QuickMenu || a == SystemMenu) return true;
  if (a == NsoMenu) return output == OUTPUT_SWITCH || output == OUTPUT_SWITCHPRO ||
                          output == OUTPUT_SWITCH2PRO;
  if (a == Capture) return output == OUTPUT_SWITCH || output == OUTPUT_SWITCHPRO ||
                          output == OUTPUT_SWITCH2PRO ||
                          output == OUTPUT_PS4 || output == OUTPUT_PS5;
  // Native NSO controllers expose a smaller report, not a full Pro controller.
  // Select is the Minus bit on all of them; native Genesis/N64 have no Minus
  // control, and native N64 reads that bit as C-Right.
  // The NES app opens its menu with L+R; N64 uses the extra ZR. X closes
  // software from HOME on identities without an X button; on N64 it is the
  // C-Down/host X on N64, on Genesis the same bit as Y.
  if (output == OUTPUT_SWITCHPRO && is_nso_special_active()) {
    if (output_uses_switch_genesis_profile()) return a == Select || a == X;
    if (output_uses_switch_nes_profile()) return a == Select || a == X || a == LB || a == RB || a == LbRb;
    if (output_uses_switch_snes_profile())
      return a == Select || (a >= LB && a <= RT) || a == LbRb || a == LtRt;
    if (output_uses_switch_n64_profile())
      return a == Select || a == X || (a >= LB && a <= RT) || a == LbRb || a == LtRt;
    return false;
  }
  switch (canonicalizeOutputMode(output)) {
    case OUTPUT_HID: case OUTPUT_MISTER: case OUTPUT_MISTER_JOGCON: case OUTPUT_MISTER_NEGCON:
    case OUTPUT_MISTER_GUNCON: case OUTPUT_MISTER_PADDLE: case OUTPUT_MISTER_SPINNER:
    case OUTPUT_XINPUT: case OUTPUT_XINPUTW: case OUTPUT_XINPUT2P:
    case OUTPUT_XID: case OUTPUT_PS3: case OUTPUT_PS4: case OUTPUT_PS5:
#if defined(ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT)
    case OUTPUT_XBOXONE:
#endif
    case OUTPUT_SWITCH: case OUTPUT_SWITCHPRO: case OUTPUT_SWITCH2PRO: return true;
    default: return false;
  }
}
static const char* actionName(Action action, outputMode_t output) {
  if (action == Off) return "Off";
  if (action == QuickMenu) return "Quick Menu";
  if (action == SystemMenu) return "System Menu";
  if (!directionalHotkeyAvailable(action,output)) return "N/A";
  if (action == NsoMenu) return "NSO Menu";
  if (action == Capture) return output == OUTPUT_PS4 || output == OUTPUT_PS5 ? "TPad" : "Capture";
  if (action == LbRb || action == LtRt) {
    // One buffer per action: uniqueness keeps the two from sharing a row.
    static char pair[2][24];
    char* text = pair[action == LtRt];
    const uint8_t first = action == LtRt ? uint8_t(LT - A) : uint8_t(LB - A);
    snprintf(text, sizeof(pair[0]), "%s+%s", remapOutputButtonName(output, first),
             remapOutputButtonName(output, uint8_t(first + 1)));
    return text;
  }
  return remapOutputButtonName(output, action == Select ? 11 : uint8_t(action-A));
}
const char* directionalHotkeyName(uint8_t a, DeviceEnum mode, const char* type,
                                uint8_t d, outputMode_t output) {
#if ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
  (void)a;
  return actionName(directionalHotkeyDefault(directionalHotkeyKind(mode,type),d,output),output);
#else
  uint8_t actions[Directions]; std::memcpy(actions, active_direction_hotkeys, Directions);
  if (d >= Directions) return "N/A";
  actions[d] = a;
  return actionName(resolveProfile(actions,directionalHotkeyKind(mode,type),d,switchDefaultsActive(output),
                                   nsoUpAction(output)), output);
#endif
}
void cycleDirectionalHotkey(uint8_t* a, uint8_t d, DeviceEnum mode, const char* type,
                           outputMode_t output, bool forward) {
#if ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
  (void)a; (void)d; (void)mode; (void)type; (void)output; (void)forward;
  return;
#else
  if (d>=3) return;
  const uint8_t previous = a[d];
  for (uint8_t tries=0; tries<Count; ++tries) {
    a[d] = (a[d] + (forward ? 1 : Count-1)) % Count;
    if (directionalHotkeysValid(mode,a) && (a[d]==Auto ||
        directionalHotkeyAvailable(resolve(a[d],directionalHotkeyKind(mode,type),d),output))) return;
  }
  a[d]=previous;
#endif
}
uint32_t directionalMenuCombo(Action action) {
  if (!max_devices || !controllerFrameConst(0).connected) return 0;
  const Kind kind = directionalHotkeyKind(deviceMode,controllerFrameConst(0).controller_type_name);
  for (uint8_t d=0;d<3;++d)
    if (controllerAction(0,kind,d)==action) return directions[d]|INPUT_START;
  return 0;
}
void updateDirectionalHotkeys() {
  const auto output = get_effective_output_mode();
  for (uint8_t i=0;i<MAX_USB_OUT;++i) {
    const uint32_t old = outputs[i], oldSuppressed = suppressed[i];
    outputs[i]=0; suppressed[i]=0; readyOutputs[i]=0;
    const uint8_t engaged = engagedDirection[i];
    engagedDirection[i] = 0;
    if (i<max_devices && controllerFrameConst(i).connected && !menuOwnsControllerInput()) {
      const auto& frame = controllerFrameConst(i);
      const Kind kind = directionalHotkeyKind(deviceMode,frame.controller_type_name);
      const uint32_t physical = pre_transform_hotkey_buttons[i];
      uint32_t chord = physical;
#if ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
      // Match Home's one-poll edge bridge without extending a held shortcut.
      // A complete chord still needs exactly one direction and no extra button.
      const uint32_t previous = previousPhysical[i];
      if (!oldSuppressed && physical && !(physical & (physical - 1u)) &&
          previous && !(previous & (previous - 1u))) chord |= previous;
      previousPhysical[i] = physical;
#endif
      if (kind != None) for (uint8_t d=0;d<3;++d) {
        // A fresh diagonal is ambiguous and never starts a shortcut, but a
        // held one survives the thumb rolling onto a diagonal: Start and its
        // direction stay down and only directions were added. Those are
        // consumed with it.
        const bool sustained = engaged == d + 1 && (physical & INPUT_START) &&
            (physical & directions[d]) && !(physical & ~(INPUT_START | kAllDirections));
        if (!sustained && chord != (directions[d]|INPUT_START)) continue;
        const Action action=controllerAction(i,kind,d);
        // Adapter menus have one navigation owner. P2+ never consumes a
        // shortcut that can only open a menu for P1.
        if (i != 0 && (action == QuickMenu || action == SystemMenu)) continue;
        if (action==Off ||
            !directionalHotkeyAvailable(action,output)) continue;
        // Preferred console personas on a PC must not consume Capture chords.
        if ((action==Capture || action==NsoMenu) && is_auto_output_mode_selected() &&
            autoDetectState!=AUTO_STATE_SWITCH && autoDetectState!=AUTO_STATE_PS4 &&
            autoDetectState!=AUTO_STATE_PLAYSTATION) continue;
        suppressed[i]=INPUT_START | (sustained ? (physical & kAllDirections) : directions[d]);
        outputs[i]=action == NsoMenu ? switch_hotkey::menuButtons(switchHotkeyProfile(output)) : button(action);
        engagedDirection[i] = d + 1;
        break; // A diagonal never generates two destinations.
      }
    }
#if ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
    else previousPhysical[i] = 0;
#endif
    if (i<max_devices && suppressed[i]) controllerFrame(i).digital_buttons &= ~suppressed[i];
    if (old!=outputs[i] || oldSuppressed!=suppressed[i]) requestControllerFrameDelivery(i);
  }
}
bool directionalHomeHotkeyEnabled(uint8_t port) {
  if (!menu_home_hotkey) return false;
#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
  const auto* slot = refreshControllerShortcuts(port);
  return slot && controllerCanChord(*slot) &&
         modernHomeEnabled(slot->profile.home_preference, slot->known, slot->present);
#else
  (void)port;
  return true;
#endif
}
#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
uint32_t directionalHotkeyEditIdentity(uint8_t port) {
  const auto* slot = refreshControllerShortcuts(port);
  return slot ? slot->token : 0;
}
bool loadDirectionalHotkeySelection(uint8_t port, uint8_t* actions, uint8_t& home) {
  if (!actions) return false;
  std::memset(actions, 0, Directions);
  home = HomeAuto;
  const auto* slot = refreshControllerShortcuts(port);
  if (!slot) return false;
  std::memcpy(actions, slot->profile.actions, Directions);
  home = slot->profile.home_preference;
  return true;
}
bool saveDirectionalHotkeySelection(uint8_t port, uint32_t capturedIdentity,
                                   const uint8_t* actions, uint8_t home) {
  auto* slot = refreshControllerShortcuts(port);
  if (!actions || !slot || !capturedIdentity || slot->token != capturedIdentity ||
      home > HomeOn || !uniqueModern(actions, slot->known, slot->present)) return false;
  auto profile = slot->profile;
  std::memcpy(profile.actions, actions, Directions);
  profile.home_preference = home;
  return modern2usbHotkeyProfileUpsert(profile);
}
const char* directionalControllerHotkeyName(uint8_t stored, uint8_t d,
                                           uint8_t port, outputMode_t output) {
  const auto* slot = refreshControllerShortcuts(port);
  if (!slot || d >= Directions) return "N/A";
  const Action action = resolveModern(stored, d, slot->known, slot->present);
  if (port != 0 && (action == QuickMenu || action == SystemMenu)) return "N/A";
  if (stored == Auto) {
    static char automaticLabel[20];
    std::snprintf(automaticLabel, sizeof(automaticLabel), "Auto:%s", actionName(action, output));
    return automaticLabel;
  }
  return actionName(action, output);
}
void cycleDirectionalControllerHotkey(uint8_t* actions, uint8_t d, uint8_t port,
                                      outputMode_t output, bool forward) {
  const auto* slot = refreshControllerShortcuts(port);
  if (!actions || d >= Directions || !slot) return;
  const uint8_t previous = actions[d];
  for (uint8_t tries = 0; tries < Count; ++tries) {
    actions[d] = (actions[d] + (forward ? 1 : Count-1)) % Count;
    if (port != 0 && (actions[d] == QuickMenu || actions[d] == SystemMenu)) continue;
    if (uniqueModern(actions, slot->known, slot->present) &&
        (actions[d] == Auto || directionalHotkeyAvailable(Action(actions[d]), output))) return;
  }
  actions[d] = previous;
}
#endif
uint32_t directionalHotkeySuppression(uint8_t port) {
  return port<MAX_USB_OUT ? suppressed[port] : 0;
}
uint32_t directionalHotkeyOutputButtons(uint8_t port) {
  return port<MAX_USB_OUT ? readyOutputs[port] : 0;
}
void applyDirectionalHotkeysToOutput(bool enabled) {
#if ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
  enabled = enabled && menu_home_hotkey;
#endif
  for (uint8_t i=0;i<MAX_USB_OUT;++i) {
    readyOutputs[i]=enabled && i<max_devices && controllerFrameConst(i).connected ? outputs[i] : 0;
    if (!readyOutputs[i]) continue;
    const auto output=get_effective_output_mode();
    // Plain DInput emits the neutral bitset. Console transports apply their
    // overlay after report construction, bypassing every physical-input map.
    if (output!=OUTPUT_HID && output!=OUTPUT_MISTER && output!=OUTPUT_MISTER_JOGCON &&
        output!=OUTPUT_MISTER_NEGCON && output!=OUTPUT_MISTER_GUNCON &&
        output!=OUTPUT_MISTER_PADDLE) continue;
    auto& frame=controllerFrame(i);
    const uint32_t buttons=applyNintendoPositionButtonMap(readyOutputs[i],effectivePositionButtonMap(
        deviceMode,output,is_nso_special_active(),button_map_mode));
    frame.digital_buttons |= buttons;
    if (buttons&INPUT_L2) frame.ANALOG_L2=255;
    if (buttons&INPUT_R2) frame.ANALOG_R2=255;
  }
}
#endif
