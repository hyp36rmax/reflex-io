#include "../product_config.h"
#include "directional_hotkeys.h"
#include "controller_runtime_internal.h"

#include <Arduino.h>

#include "controller_frame_state.h"
#include "controller_output_cache_state.h"
#include "controller_settings_state.h"
#include "controller_delivery_state.h"
#include "button_remap.h"
#include "device_runtime_state.h"
#include "dpad_mode.h"
#include "firmware_support.h"
#include "firmware_hotkeys.h"
#include "hotkey_combo.h"
#include "../input/shared/input_button_bits.h"
#include "../menu/menu_runtime_state.h"
#include "../menu/menu_ui_state.h"
#include "../output/output_runtime_state.h"

namespace controller_runtime_internal {

namespace {

struct HeldVirtualHotkeyState {
  bool active = false;
};

// Hold Start threshold comes from global Guide Hold setting (500ms steps).
// A normal Start press remains native; Menu Hold has a separate default.
// Home is then a tap while Start is still held: MiSTer Main opens its OSD on
// the menu key's release (menu.cpp, KEY_F12 | UPSTROKE), and a held one is a
// long press there (user_io.cpp opens joystick setup), so holding Home until
// Start's release made the OSD wait for the release.
constexpr uint32_t kHoldStartHomeTapMs = 100;

enum class HoldStartAction : uint8_t { None, Home, HideStart };

struct HoldStartHotkeyState {
  bool blocked = false;  // Start joined a chord, or was held as a menu closed.
  bool timing = false;
  bool fired = false;
  uint32_t started_at = 0;
  uint32_t fired_at = 0;
};

uint32_t virtualHotkeyOutputButtons[MAX_USB_OUT] = {};
uint32_t virtualHotkeySuppressedButtons[MAX_USB_OUT] = {};

bool holdStartHomeEnabled() {
#if ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
  // Fixed-preset products have one Hotkeys toggle for every Start shortcut.
  return menu_home_hotkey != 0;
#else
  return menu_hold_start_hotkey != 0;
#endif
}

// Home for one tap once Start has been held alone long enough, then Start
// stays hidden until it is released, so one hold sends one Home. Only a press
// of Start on its own, held on its own, counts; anything else waits for
// Start's release.
HoldStartAction __not_in_flash_func(updateHoldStartHome)(HoldStartHotkeyState& state, bool enabled,
                                                         uint32_t physicalButtons, uint32_t now,
                                                         uint32_t holdMs) {
  const uint32_t buttons = physicalButtons & HOTKEY_ALLOWED_BUTTONS;
  if ((buttons & INPUT_START) == 0) {
    state.blocked = false;
  } else if (buttons != INPUT_START) {
    state.blocked = true;
  }
  if (!enabled || state.blocked || buttons != INPUT_START) {
    state.timing = false;
    state.fired = false;
    return HoldStartAction::None;
  }
  if (!state.timing) {
    state.timing = true;
    state.started_at = now;
  }
  if (!state.fired) {
    if ((uint32_t)(now - state.started_at) < holdMs) {
      return HoldStartAction::None;
    }
    // Timed from the first pass that sees the hold complete, so a slow pass
    // can't skip the tap.
    state.fired = true;
    state.fired_at = now;
  }
  return (uint32_t)(now - state.fired_at) < kHoldStartHomeTapMs ? HoldStartAction::Home
                                                               : HoldStartAction::HideStart;
}

bool captureHotkeySupportedByCurrentOutput() {
  const outputMode_t mode = get_effective_output_mode();
  const bool isSwitch = mode == OUTPUT_SWITCH || mode == OUTPUT_SWITCHPRO ||
                        mode == OUTPUT_SWITCH2PRO;
  if (!isSwitch && mode != OUTPUT_PS4 && mode != OUTPUT_PS5) {
    return false;
  }

  // A PC may request a console persona through its preferred-output setting.
  // Do not consume Up + Start for Windows, generic/MiSTer fallback or an
  // unresolved AUTO probe. Manual console output explicitly opts into its
  // controls; without AUTO we cannot distinguish that console from a PC.
  if (!is_auto_output_mode_selected()) {
    return true;
  }
  return isSwitch ? autoDetectState == AUTO_STATE_SWITCH
                  : (autoDetectState == AUTO_STATE_PS4 ||
                     autoDetectState == AUTO_STATE_PLAYSTATION);
}

void __not_in_flash_func(clearHotkeyComboFromFrame)(controller_state_t& frame, uint32_t combo) {
  // PAD_* shares storage with digital_buttons, so these also clear INPUT_PAD_* bits.
  frame.digital_buttons &= ~combo;
}

uint32_t __not_in_flash_func(physicalHotkeyButtonsForController)(uint8_t index) {
  if (index >= max_devices || index >= MAX_USB_OUT) {
    return 0;
  }
  const controller_state_t& frame = controllerFrameConst(index);
  if (!frame.connected) {
    return 0;
  }

  return pre_transform_hotkey_buttons[index] & HOTKEY_ALLOWED_BUTTONS;
}

bool __not_in_flash_func(applyHeldVirtualButtonHotkey)(uint8_t index,
                                                       controller_state_t& frame,
                                                       HeldVirtualHotkeyState& state,
                                                       uint32_t combo,
                                                       uint32_t outputButton,
                                                       uint32_t physicalButtons,
                                                       uint32_t& outputButtons) {
  if (combo == 0 || outputButton == 0) {
    state = HeldVirtualHotkeyState{};
    return false;
  }

  // An exact chord starts the hotkey. Once held, a chord with a direction
  // survives the thumb rolling onto a diagonal: the added directions are
  // consumed with it.
  constexpr uint32_t kDirections = INPUT_PAD_U | INPUT_PAD_D | INPUT_PAD_L | INPUT_PAD_R;
  const uint32_t buttons = physicalButtons & HOTKEY_ALLOWED_BUTTONS;
  uint32_t consumed = 0;
  if (hotkeyComboMatches(physicalButtons, combo)) {
    consumed = combo;
  } else if (state.active && (combo & kDirections) && (buttons & combo) == combo &&
             (buttons & ~(combo | kDirections)) == 0) {
    consumed = combo | (buttons & kDirections);
  }
  if (consumed == 0) {
    state = HeldVirtualHotkeyState{};
    return false;
  }

  if (!state.active) {
    state.active = true;
  }

  // Suppress the physical combo before virtual output so games do not see the chord.
  clearHotkeyComboFromFrame(frame, consumed);
  virtualHotkeySuppressedButtons[index] = consumed;
  outputButtons |= outputButton;

  return true;
}

}  // namespace

void __not_in_flash_func(handleHeldBootloaderHotkey)() {
#if defined(ADAPT_PRIMARY_EGRESS_DB15)
  return;
#endif
  if (hotkey_bootsel == (uint32_t)-1) {
    return;
  }

  static uint32_t hotkey_bootsel_pressedAt = 0;
  static bool hotkey_bootsel_btnStateOld = false;
  bool hotkey_bootsel_btnStateNow =
      hotkeyComboMatches(physicalHotkeyButtonsForController(0), hotkey_bootsel);
  if (hotkey_bootsel_btnStateNow != hotkey_bootsel_btnStateOld) {
    hotkey_bootsel_btnStateOld = hotkey_bootsel_btnStateNow;
    hotkey_bootsel_pressedAt = hotkey_bootsel_btnStateNow ? millis() : 0;
  }
  if (hotkey_bootsel_btnStateNow && millis() - hotkey_bootsel_pressedAt > 5000) {
    rebootToBootloader();
  }
}

void __not_in_flash_func(handleHeldDpadModeHotkey)() {
#if defined(ADAPT_PRIMARY_EGRESS_DB15)
  return;
#endif
  if (hotkey_dpad_as_buttons == (uint32_t)-1) {
    return;
  }

  static uint32_t hotkey_dpad_as_buttons_pressedAt = 0;
  static bool hotkey_dpad_as_buttons_btnStateOld = false;
  bool hotkey_dpad_as_buttons_btnStateNow =
      hotkeyComboMatches(physicalHotkeyButtonsForController(0), hotkey_dpad_as_buttons);
  if (hotkey_dpad_as_buttons_btnStateNow != hotkey_dpad_as_buttons_btnStateOld) {
    hotkey_dpad_as_buttons_btnStateOld = hotkey_dpad_as_buttons_btnStateNow;
    hotkey_dpad_as_buttons_pressedAt = hotkey_dpad_as_buttons_btnStateNow ? millis() : 0;
  }
  if (hotkey_dpad_as_buttons_btnStateNow && millis() - hotkey_dpad_as_buttons_pressedAt > 5000) {
    dpad_mode = (dpad_mode + 1) % 4;
    hotkey_dpad_as_buttons_pressedAt = millis();
  }
}

void __not_in_flash_func(updateHeldControllerHotkeysBeforeTransforms)() {
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
  updateDirectionalHotkeys();
#endif
#if defined(ADAPT_PRIMARY_EGRESS_DB15)
  return;
#endif
  static HeldVirtualHotkeyState homeHotkeyState[MAX_USB_OUT];
  static HeldVirtualHotkeyState captureHotkeyState[MAX_USB_OUT];
#if ADAPT_POLICY_START_UP_COIN
  static HeldVirtualHotkeyState coinHotkeyState[MAX_USB_OUT];
#endif
  static HoldStartHotkeyState holdStartState[MAX_USB_OUT];
  static uint32_t lastVirtualHotkeyOutputButtons[MAX_USB_OUT] = {};
  static uint32_t previousPhysicalButtons[MAX_USB_OUT] = {};
#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
  static uint32_t previousConnection[MAX_USB_OUT] = {};
#endif
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
#if !defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
  const uint32_t homeCombo = menu_home_hotkey ? (INPUT_START | INPUT_PAD_D) : 0;
#endif
  const uint32_t captureCombo = 0; // Up is now a per-input action, not a global chord.
#else
  const uint32_t homeCombo = menu_home_hotkey != 0
    ? effectiveHomeHotkeyComboForMode(deviceMode, menu_home_hotkey_combo)
    : 0;
  const uint32_t captureCombo = menu_capture_hotkey != 0 && captureHotkeySupportedByCurrentOutput()
    ? menu_capture_hotkey_combo : 0;
#endif
#if ADAPT_POLICY_START_UP_COIN
  // Start+Up adds a credit (owner, 2026-10-18). A console output whose
  // Capture shortcut is Up+Start keeps it; checking Capture first is not
  // enough, because the one-poll release bridge would then press Coin.
  constexpr uint32_t kStartUpCoinCombo = INPUT_START | INPUT_PAD_U;
  const uint32_t coinCombo = captureCombo != kStartUpCoinCombo ? kStartUpCoinCombo : 0;
#endif

  if (menuOwnsControllerInput()) {
    for (uint8_t i = 0; i < MAX_USB_OUT; ++i) {
      homeHotkeyState[i] = HeldVirtualHotkeyState{};
      captureHotkeyState[i] = HeldVirtualHotkeyState{};
#if ADAPT_POLICY_START_UP_COIN
      coinHotkeyState[i] = HeldVirtualHotkeyState{};
#endif
      holdStartState[i] = HoldStartHotkeyState{};
      holdStartState[i].blocked = true;
      virtualHotkeyOutputButtons[i] = 0;
      virtualHotkeySuppressedButtons[i] = 0;
      lastVirtualHotkeyOutputButtons[i] = 0;
      previousPhysicalButtons[i] = 0;
    }
    return;
  }

  const uint32_t now = millis();
  const bool holdStartOn = holdStartHomeEnabled();
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    controller_state_t& frame = controllerFrame(i);
#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
    const uint32_t connection = directionalHotkeyEditIdentity(i);
    if (connection != previousConnection[i]) {
      previousConnection[i] = connection;
      previousPhysicalButtons[i] = 0;
      homeHotkeyState[i] = HeldVirtualHotkeyState{};
      captureHotkeyState[i] = HeldVirtualHotkeyState{};
      holdStartState[i] = HoldStartHotkeyState{};
      virtualHotkeyOutputButtons[i] = 0;
      virtualHotkeySuppressedButtons[i] = 0;
    }
    const uint32_t homeCombo = directionalHomeHotkeyEnabled(i) ? (INPUT_START | INPUT_PAD_D) : 0;
    // Hold Start follows the controller's own Home choice, as Down+Start does.
    const bool holdStartEnabled = holdStartOn && directionalHomeHotkeyAllowed(i);
#else
    const bool holdStartEnabled = holdStartOn;
#endif
    if (!frame.connected) {
      if (homeHotkeyState[i].active || captureHotkeyState[i].active ||
          virtualHotkeyOutputButtons[i] != 0 || virtualHotkeySuppressedButtons[i] != 0) {
        virtualHotkeyOutputButtons[i] = 0;
        virtualHotkeySuppressedButtons[i] = 0;
      }
      homeHotkeyState[i] = HeldVirtualHotkeyState{};
      captureHotkeyState[i] = HeldVirtualHotkeyState{};
#if ADAPT_POLICY_START_UP_COIN
      coinHotkeyState[i] = HeldVirtualHotkeyState{};
#endif
      holdStartState[i] = HoldStartHotkeyState{};
      previousPhysicalButtons[i] = 0;
      continue;
    }

    const uint32_t physicalButtons = physicalHotkeyButtonsForController(i);
    const HoldStartAction holdStart =
        updateHoldStartHome(holdStartState[i], holdStartEnabled, physicalButtons, now,
#if defined(PRODUCT_CLASSIC2USB)
                            hotkeyHoldMsForSetting(menu_guide_hold_time)
#else
                            1000u
#endif
                            );
    // Some serial controller protocols can expose the two edges of a chord in
    // adjacent polls. Bridge only that one-poll transition so Down->Start and
    // Start->Down are equivalent without turning sequential taps into a latch.
    const uint32_t transitionButtons = physicalButtons | previousPhysicalButtons[i];
    previousPhysicalButtons[i] = physicalButtons;
    const bool canTriggerHome = hotkeyComboMatches(transitionButtons, homeCombo);
    const bool canTriggerCapture = hotkeyComboMatches(transitionButtons, captureCombo);
#if ADAPT_POLICY_START_UP_COIN
    const bool canTriggerCoin = hotkeyComboMatches(transitionButtons, coinCombo);
#else
    const bool canTriggerCoin = false;
#endif
    const bool hasActiveVirtualHotkey =
      homeHotkeyState[i].active ||
      captureHotkeyState[i].active ||
#if ADAPT_POLICY_START_UP_COIN
      coinHotkeyState[i].active ||
#endif
      virtualHotkeyOutputButtons[i] != 0 ||
      virtualHotkeySuppressedButtons[i] != 0;
    if (!hasActiveVirtualHotkey && !canTriggerHome && !canTriggerCapture && !canTriggerCoin &&
        holdStart == HoldStartAction::None) {
      continue;
    }
    virtualHotkeyOutputButtons[i] = 0;
    virtualHotkeySuppressedButtons[i] = 0;

    const uint32_t homeButtons = homeHotkeyState[i].active ? physicalButtons : transitionButtons;
    const uint32_t captureButtons = captureHotkeyState[i].active ? physicalButtons : transitionButtons;
    if ((homeHotkeyState[i].active || canTriggerHome) &&
        applyHeldVirtualButtonHotkey(i, frame, homeHotkeyState[i], homeCombo,
                                     INPUT_HOME, homeButtons, virtualHotkeyOutputButtons[i])) {
      continue;
    }

    if ((captureHotkeyState[i].active || canTriggerCapture) &&
        applyHeldVirtualButtonHotkey(i, frame, captureHotkeyState[i], captureCombo,
                                     INPUT_CAPTURE, captureButtons, virtualHotkeyOutputButtons[i])) {
      continue;
    }
#if ADAPT_POLICY_START_UP_COIN
    const uint32_t coinButtons = coinHotkeyState[i].active ? physicalButtons : transitionButtons;
    if ((coinHotkeyState[i].active || canTriggerCoin) &&
        applyHeldVirtualButtonHotkey(i, frame, coinHotkeyState[i], coinCombo,
                                     INPUT_SELECT, coinButtons, virtualHotkeyOutputButtons[i])) {
      continue;
    }
#endif
    if (holdStart != HoldStartAction::None) {
      clearHotkeyComboFromFrame(frame, INPUT_START);
      virtualHotkeySuppressedButtons[i] = INPUT_START;
      if (holdStart == HoldStartAction::Home) {
        virtualHotkeyOutputButtons[i] |= INPUT_HOME;
      }
    }
  }

  // Vacated logical players must not retain a held chord when the next device
  // takes that slot. Keep the established Classic2USB path unchanged.
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS) && !defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  for (uint8_t i = max_devices; i < MAX_USB_OUT; ++i) {
    homeHotkeyState[i] = HeldVirtualHotkeyState{};
    captureHotkeyState[i] = HeldVirtualHotkeyState{};
    holdStartState[i] = HoldStartHotkeyState{};
    previousPhysicalButtons[i] = 0;
    virtualHotkeyOutputButtons[i] = 0;
    virtualHotkeySuppressedButtons[i] = 0;
    lastVirtualHotkeyOutputButtons[i] = 0;
#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
    previousConnection[i] = 0;
#endif
  }
#endif
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    if (virtualHotkeyOutputButtons[i] != lastVirtualHotkeyOutputButtons[i]) {
      requestControllerFrameDelivery(i);
      lastVirtualHotkeyOutputButtons[i] = virtualHotkeyOutputButtons[i];
    }
  }
}

bool __not_in_flash_func(virtualControllerHotkeysRequireService)() {
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
  return true; // Detect presses AND releases even with all legacy flags off.
#endif
#if defined(ADAPT_PRIMARY_EGRESS_DB15)
  return false;
#endif
#if ADAPT_POLICY_START_UP_COIN
  return true; // Start+Up Coin has no setting; detect presses and releases.
#endif
  if (holdStartHomeEnabled()) {
    return true; // Hold Start times Start on its own, change or no change.
  }
  if ((menu_home_hotkey != 0 && menu_home_hotkey_combo != 0) ||
      (menu_capture_hotkey != 0 && menu_capture_hotkey_combo != 0 &&
       captureHotkeySupportedByCurrentOutput())) {
    return true;
  }

  for (uint8_t i = 0; i < MAX_USB_OUT; ++i) {
    if (virtualHotkeyOutputButtons[i] != 0 ||
        virtualHotkeySuppressedButtons[i] != 0) {
      return true;
    }
  }
  return false;
}

void __not_in_flash_func(applyVirtualControllerHotkeyOutputs)() {
#if defined(ADAPT_PRIMARY_EGRESS_DB15)
  return;
#endif
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    controller_state_t& frame = controllerFrame(i);
    if (frame.connected) {
      frame.digital_buttons &= ~virtualHotkeySuppressedButtons[i];
      frame.digital_buttons |= virtualHotkeyOutputButtons[i];
    }
  }
}

uint32_t __not_in_flash_func(suppressedControllerHotkeySourceButtons)(uint8_t index) {
#if defined(ADAPT_PRIMARY_EGRESS_DB15)
  (void)index;
  return 0;
#endif
  return ((index < MAX_USB_OUT) ? virtualHotkeySuppressedButtons[index] : 0)
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
      | directionalHotkeySuppression(index)
#endif
      ;
}

}  // namespace controller_runtime_internal
