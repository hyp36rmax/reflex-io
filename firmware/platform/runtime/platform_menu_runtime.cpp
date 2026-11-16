#include "../../product_config.h"
#include "../../core/directional_hotkeys.h"

#include "platform_menu_runtime.h"
#include "platform_menu_input_runtime.h"

#include <Arduino.h>

#include "../../core/controller_frame_state.h"
#include "../../core/controller_output_cache_state.h"
#include "../../core/controller_selection_state.h"
#include "../../core/device_runtime_state.h"
#include "../../core/deferred_mode_saves.h"
#include "../../core/firmware_support.h"
#include "../../core/hotkey_combo.h"
#include "../../core/settings_store.h"
#include "../../firmware_platform_config.h"
#include "../../features/feature_module.h"
#include "../../input/shared/input_button_bits.h"
#include "../../input/runtime/input_mode_runtime.h"
#include "../../menu/menu_bridge.h"
#include "../../menu/menu_helpers.h"
#include "../../menu/menu_idle_runtime.h"
#if defined(ADAPT_FEATURE_DINPUT_MAPPING_WIZARD)
  #include "../../features/dinput_mapping/dinput_mapping_wizard.h"
#endif
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  #include "../../features/usb2db15/usb2db15_arcade_mapping.h"
  #include "../../features/usb2db15/usb2db15_arcade_session.h"
  #include "../../features/usb2db15/usb2db15_selector_navigation.h"
#endif
#include "../../menu/menu_input.h"
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
#include "../../menu/menu_jvs_analog.h"
#include "../../menu/menu_jvs_panel.h"
#endif
#include "../../menu/menu_product_pages.h"
#include "../../menu/menu_runtime_state.h"
#include "../../menu/menu_setup_wizard.h"
#include "../../menu/menu_system_screens.h"
#include "../../menu/menu_ui_state.h"
#include "../../menu/quick_config.h"
#include "../../output/output_runtime_state.h"
#include "../buzzer.h"
#include "../display_runtime_state.h"
#include "platform_menu_product_events.h"

namespace {

#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
// Buttons are sampled every loop, but JVS menus draw only between replies.
struct JvsMenuButtonEvents { ButtonEvent mode; ButtonEvent reset; uint32_t atMs; };
constexpr uint32_t kJvsMenuEventMaxAgeMs = 250;
constexpr uint8_t kJvsMenuQueueSize = 16;
JvsMenuButtonEvents jvsMenuQueue[kJvsMenuQueueSize]{};
uint8_t jvsMenuHead = 0, jvsMenuCount = 0;
bool jvsMenuInSafeWindow = false;
void queueJvsMenuEvents(ButtonEvent mode, ButtonEvent reset) {
  if (mode == BTN_EVENT_NONE && reset == BTN_EVENT_NONE) return;
  if (jvsMenuCount >= kJvsMenuQueueSize) return;
  const uint8_t tail = static_cast<uint8_t>((jvsMenuHead + jvsMenuCount) % kJvsMenuQueueSize);
  jvsMenuQueue[tail] = {mode, reset, millis()};
  ++jvsMenuCount;
}
#endif

constexpr uint16_t kMenuEntryHotkeyMinHoldMs = 1000;
constexpr uint16_t kMenuEntryInputSuppressMs = 500;
bool returnQuickConfigToCombinedMenu = false;
// Set when a Mode long press opened the System Menu; cleared on release.
bool modeHoldOpenedSystemMenu = false;
bool productUsesControllerSelectorMenu() {
#if ADAPT_POLICY_QUICK_SELECTED_CONTROLLER
  return true;
#else
  return false;
#endif
}

void resetSystemMenuOpenState() {
  menu_navigation_state = 255;
  menu_selected_visible = 0;
  menu_scroll_offset = 0;
  menu_bottom_right_selected = false;
}

void maybeTriggerHeldMenuHotkey(
    uint32_t combo,
    uint32_t& startTime,
    bool& active,
    void (*action)()) {
  if (combo == 0) {
    active = false;
    return;
  }

  uint16_t holdMs = hotkeyHoldMsForSetting(menu_hotkey_hold_time);
  if (holdMs < kMenuEntryHotkeyMinHoldMs) {
    holdMs = kMenuEntryHotkeyMinHoldMs;
  }
  const bool held = hotkeyComboMatches(pre_transform_hotkey_buttons[0], combo);
  if (!held) {
    active = false;
    return;
  }

  if (!active) {
    active = true;
    startTime = millis();
    return;
  }

  if (millis() - startTime >= holdMs) {
    active = false;
    action();
  }
}

void openQuickConfigMenu() {
  suppressMenuControllerInputForMs(kMenuEntryInputSuppressMs);
  isQuickConfigOpen = true;
  quickConfig.open();
  buzzer.playMenuEnter();
}

void openQuickConfigMenuFromHotkey() {
  openQuickConfigMenu();
}

void openSystemOnlyMenu() {
  resetSystemMenuOpenState();
  suppressMenuControllerInputForMs(kMenuEntryInputSuppressMs);
  system_menu_only = true;
  isMenuOpen = true;
  buzzer.playMenuEnter();
}

void openSystemOnlyMenuFromHotkey() {
  openSystemOnlyMenu();
}

void openControllerSelectorMenu() {
#if ADAPT_POLICY_QUICK_SELECTED_CONTROLLER
  suppressMenuControllerInputForMs(kMenuEntryInputSuppressMs);
  system_menu_only = false;
  isMenuOpen = true;
  openProductControllerSelectorPage();
  buzzer.playMenuEnter();
#endif
}

void openControllerSelectorMenuFromHotkey() {
  openControllerSelectorMenu();
}

#ifdef ENABLE_INPUT_AUTODETECT
void activateAutoDetectModeFromResetHold() {
#if ADAPT_POLICY_RESET_HOLD_AUTO_OUTPUT
  // Save pending controller edits against the current mode before changing it.
  deferredModeSavesFlushNow();
  outputMode = get_effective_output_mode();
  configuredOutputMode = OUTPUT_AUTO;
  // Keep the current USB personality until the release-triggered reboot.
  autoDetectState = AUTO_STATE_IDLE;
  autoDetectProbeStage = AUTO_PROBE_GENERIC;
  // A release-triggered reboot must not restore the previous resolved host.
  auto_detect_clear_scratch_state();
#endif
  #ifdef USE_I2C_DISPLAY
    beginDisplayWire();
    display.clear();
    display.setCol(0);
    display.set2X();
    #if ADAPT_POLICY_RESET_HOLD_AUTO_OUTPUT
    display.println(F("AUTO"));
    display.set1X();
    display.println(F("Input+Output: Auto"));
    display.println(F("Release LEFT"));
    #else
    display.println(F("AUTO DETECT"));
    display.set1X();
    display.println(F("Release RESET"));
    #endif
    endDisplayWire();
  #endif

  buzzer.playSave();
  armAutoDetectReboot();
}
#endif

void handleHomeButtonEvents(ButtonEvent modeEvent, ButtonEvent resetEvent) {
  applyKioskButtonGate(modeEvent, resetEvent);

  if (productHandleHomeButtonChord(modeButton.isPressed(),
                                   resetButton.isPressed())) {
    return;
  }

  const bool modeEventConsumedByFeature =
    productHandleHomeButtonEvent(modeEvent, modeButton.isPressed());
  const bool resetEventConsumedByFeature =
    productHandleHomeNavigationButtonEvent(resetEvent);

  switch (modeEvent) {
    case BTN_EVENT_SINGLE:
    case BTN_EVENT_DOUBLE:
      if (modeEventConsumedByFeature) {
        break;
      }
      if (productUsesControllerSelectorMenu()) {
        openControllerSelectorMenu();
      } else {
        openQuickConfigMenu();
      }
      break;

    case BTN_EVENT_LONG:
      if (modeEventConsumedByFeature) {
        break;
      }
      openSystemOnlyMenu();
      // The press that opened the menu must be released before a new Mode
      // hold can count toward hold-to-save (which would close it again).
      modeButton.suppressUntilRelease();
      modeHoldOpenedSystemMenu = true;
      break;

    case BTN_EVENT_LONG_RELEASE:
      break;

    default:
      break;
  }

  switch (resetEvent) {
    case BTN_EVENT_SINGLE:
    case BTN_EVENT_DOUBLE:
      if (resetEventConsumedByFeature) {
        break;
      }
      buzzer.playSave();
      waitWithBuzzerUpdates(100);
      reboot();
      break;

    case BTN_EVENT_LONG:
      if (resetEventConsumedByFeature) {
        break;
      }
      #ifdef ENABLE_INPUT_AUTODETECT
      activateAutoDetectModeFromResetHold();
      #endif
      break;

    default:
      break;
  }
}

void handleQuickConfigUi(ButtonEvent modeEvent, ButtonEvent resetEvent, SerialMenuAction serialAction) {
  #ifdef USE_I2C_DISPLAY
    beginDisplayWire();
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
    if (analogTestActive) {
      MenuControllerInput input = readMenuControllerInput();
      input.aJust = input.aJust || serialAction == SerialMenuAction::Ok ||
                    serialAction == SerialMenuAction::Right;
      input.bJust = input.bJust || serialAction == SerialMenuAction::Back ||
                    serialAction == SerialMenuAction::Left;
      const bool select = modeEvent == BTN_EVENT_SINGLE || modeEvent == BTN_EVENT_DOUBLE;
      const bool next = resetEvent == BTN_EVENT_SINGLE || resetEvent == BTN_EVENT_DOUBLE ||
                        serialAction == SerialMenuAction::Down;
      renderJvsAnalogMenu(select, next, input);
      if (analogTestActive && resetEvent == BTN_EVENT_DOUBLE)
        renderJvsAnalogMenu(false, true, {});
      if (!analogTestActive) {
        suppressMenuControllerInputForMs(kMenuEntryInputSuppressMs);
        quickConfig.refreshJvsCabinet();
      }
      endDisplayWire();
      return;
    }
#endif

    static bool prev_up = false, prev_down = false, prev_left = false, prev_right = false;
    static bool prev_a = false, prev_b = false, prev_start = false;
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
    static Usb2Db15MenuPressGate confirmGate;
    static Usb2Db15MenuPressGate backGate;
    static Usb2Db15MenuPressGate startGate;
#endif

    static uint32_t last_nav_time = 0;
    static uint32_t hold_start_time = 0;
    static uint8_t last_direction = 0;
    const uint32_t DEBOUNCE_MS = 50;
    const uint32_t REPEAT_DELAY_MS = 400;
    const uint32_t REPEAT_RATE_MS = 100;

    controller_state_t primary = selectedControllerFrame();
    primary.digital_buttons &= ~quickConfig.analogTestMeasuredButtons();
    bool cur_up = menuControllerUpPressed(primary);
    bool cur_down = menuControllerDownPressed(primary);
    bool cur_left = menuControllerLeftPressed(primary);
    bool cur_right = menuControllerRightPressed(primary);
    bool cur_a = menuControllerConfirmPressed(primary, deviceMode);
    bool cur_b = menuControllerBackPressed(primary, deviceMode);
    bool cur_start = (primary.digital_buttons & INPUT_START) != 0;

    const bool suppressControllerInput = isMenuControllerInputSuppressed();
    if (suppressControllerInput) {
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
      confirmGate.reset();
      backGate.reset();
      startGate.reset();
#endif
      prev_up = cur_up;
      prev_down = cur_down;
      prev_left = cur_left;
      prev_right = cur_right;
      prev_a = cur_a;
      prev_b = cur_b;
      prev_start = cur_start;
      cur_up = false;
      cur_down = false;
      cur_left = false;
      cur_right = false;
      cur_a = false;
      cur_b = false;
      cur_start = false;
      if (serialAction == SerialMenuAction::None
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
          // Suppression guards against a held controller press. A tap on the
          // adapter's own buttons is always a new press.
          && modeEvent == BTN_EVENT_NONE && resetEvent == BTN_EVENT_NONE
#endif
      ) {
        quickConfig.render(display);
        endDisplayWire();
        return;
      }
    }

    bool up_pressed = cur_up && !prev_up;
    bool down_pressed = cur_down && !prev_down;
    bool left_pressed = cur_left && !prev_left;
    bool right_pressed = cur_right && !prev_right;
    bool a_pressed = cur_a && !prev_a;
    bool b_pressed = cur_b && !prev_b;
    bool start_pressed = cur_start && !prev_start;

    up_pressed = up_pressed || serialAction == SerialMenuAction::Up;
    down_pressed = down_pressed || serialAction == SerialMenuAction::Down;
    left_pressed = left_pressed || serialAction == SerialMenuAction::Left;
    right_pressed = right_pressed || serialAction == SerialMenuAction::Right;
    a_pressed = a_pressed || serialAction == SerialMenuAction::Ok;
    b_pressed = b_pressed || serialAction == SerialMenuAction::Back;

    uint32_t now = millis();
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
    // Confirm/back are edge-only in this product UI: raw held USB reports and
    // bounce cannot repeatedly toggle a setting or close a page.
    a_pressed = confirmGate.sample(cur_a, now) || serialAction == SerialMenuAction::Ok;
    b_pressed = backGate.sample(cur_b, now) || serialAction == SerialMenuAction::Back;
    start_pressed = startGate.sample(cur_start, now);
#endif
    bool up_repeat = false, down_repeat = false, left_repeat = false, right_repeat = false;
    if (cur_up && last_direction == 1 && (now - hold_start_time > REPEAT_DELAY_MS)) {
      if (now - last_nav_time >= REPEAT_RATE_MS) up_repeat = true;
    }
    if (cur_down && last_direction == 2 && (now - hold_start_time > REPEAT_DELAY_MS)) {
      if (now - last_nav_time >= REPEAT_RATE_MS) down_repeat = true;
    }
    if (cur_left && last_direction == 3 && (now - hold_start_time > REPEAT_DELAY_MS)) {
      if (now - last_nav_time >= REPEAT_RATE_MS) left_repeat = true;
    }
    if (cur_right && last_direction == 4 && (now - hold_start_time > REPEAT_DELAY_MS)) {
      if (now - last_nav_time >= REPEAT_RATE_MS) right_repeat = true;
    }

    if (up_pressed) { last_direction = 1; hold_start_time = now; }
    else if (down_pressed) { last_direction = 2; hold_start_time = now; }
    else if (left_pressed) { last_direction = 3; hold_start_time = now; }
    else if (right_pressed) { last_direction = 4; hold_start_time = now; }
    else if (!cur_up && !cur_down && !cur_left && !cur_right) { last_direction = 0; }

    prev_up = cur_up;
    prev_down = cur_down;
    prev_left = cur_left;
    prev_right = cur_right;
    prev_a = cur_a;
    prev_b = cur_b;
    prev_start = cur_start;

    handleIdleUiActivity(
      modeEvent != BTN_EVENT_NONE ||
      resetEvent != BTN_EVENT_NONE ||
      serialAction != SerialMenuAction::None ||
      up_pressed || down_pressed || left_pressed || right_pressed ||
      a_pressed || b_pressed || start_pressed ||
      up_repeat || down_repeat || left_repeat || right_repeat
    );

    bool can_nav = (now - last_nav_time >= DEBOUNCE_MS);
    bool start_consumed = false;

    if (can_nav && start_pressed) {
      const bool stayedOpen = quickConfig.isOpen();
      if (quickConfig.saveShortcut()) {
        if (stayedOpen && quickConfig.isOpen()) {
          buzzer.playSave();
        }
        last_nav_time = now;
        start_consumed = true;
      }
    }

    if (!start_consumed && can_nav &&
        (resetEvent == BTN_EVENT_SINGLE || resetEvent == BTN_EVENT_DOUBLE ||
         down_pressed || down_repeat)) {
      const uint8_t steps = resetEvent == BTN_EVENT_DOUBLE ? 2 : 1;
      for (uint8_t step = 0; step < steps; ++step) {
        quickConfig.navigateDown();
      }
      buzzer.playMenuNav();
      last_nav_time = now;
    }
    if (!start_consumed && can_nav && (up_pressed || up_repeat)) {
      quickConfig.navigateUp();
      buzzer.playMenuNav();
      last_nav_time = now;
    }
    if (!start_consumed && can_nav && (right_pressed || right_repeat)) {
      quickConfig.cycleValueNext(false);
      buzzer.playMenuNav();
      last_nav_time = now;
    }
    if (!start_consumed && can_nav &&
        (modeEvent == BTN_EVENT_SINGLE || modeEvent == BTN_EVENT_DOUBLE ||
         a_pressed)) {
      quickConfig.cycleValueNext(true);
      if (quickConfig.takeActionError()) {
        buzzer.playError();
      } else {
        buzzer.playMenuNav();
      }
      last_nav_time = now;
    }
    if (!start_consumed && can_nav && b_pressed) {
      quickConfig.back();
      if (quickConfig.isOpen()) {
        buzzer.playMenuNav();
      }
      last_nav_time = now;
    }
    if (!start_consumed && can_nav && (left_pressed || left_repeat)) {
      quickConfig.cycleValuePrev();
      buzzer.playMenuNav();
      last_nav_time = now;
    }

    if (!quickConfig.isOpen()) {
      isQuickConfigOpen = false;
      // Clear the old menu before a hand-off draws its first screen.
      display.clear();
      uint8_t resetDinputDevice = 0xFF;
      const bool resetDinputMapping =
          quickConfig.takeResetDinputDevice(&resetDinputDevice);
      if (resetDinputMapping) {
        returnQuickConfigToCombinedMenu = false;
        isMenuOpen = false;
#if defined(ADAPT_FEATURE_DINPUT_MAPPING_WIZARD)
        if (restartHidMappingWizard(resetDinputDevice)) {
          buzzer.playSave();
        } else {
          buzzer.playError();
        }
#else
        buzzer.playError();
#endif
      } else if (quickConfig.shouldSave()) {
        saveModeSettings();
        buzzer.playSave();
        showMenuActionFeedback(
          quickConfig.defaultsWereRestored()
            ? MenuActionFeedback::DefaultsRestored
            : MenuActionFeedback::Saved
        );
      } else {
        showMenuActionFeedback(MenuActionFeedback::Cancelled);
      }
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
      // Finish common Quick Save first, then transfer ownership to the product
      // selector. Reopening the combined menu would keep output disarmed after
      // the user finished saving/loading a profile.
      if (usb2db15ArcadeTakeProfileSaveRequest()) {
        returnQuickConfigToCombinedMenu = false;
        isMenuOpen = false;
        if (!usb2db15ArcadeOfferProfileSave()) buzzer.playError();
      }
#endif
      if (!resetDinputMapping && returnQuickConfigToCombinedMenu) {
        returnQuickConfigToCombinedMenu = false;
        system_menu_only = false;
        isMenuOpen = true;
#if ADAPT_POLICY_QUICK_SELECTED_CONTROLLER
        openProductControllerSelectorPage();
#else
        renderMenu(menu_selected_visible, menu_scroll_offset,
                   menu_bottom_right_selected);
#endif
      } else {
        forceMainDisplayRefresh();
      }
      prev_up = prev_down = prev_left = prev_right = false;
      prev_a = prev_b = prev_start = false;
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
      confirmGate.reset();
      backGate.reset();
      startGate.reset();
#endif
    } else {
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
      if (analogTestActive) {
        // Seed edge detection without delivering the press that opened the overlay.
        (void)readMenuControllerInput();
      } else
#endif
      quickConfig.render(display);
    }

    endDisplayWire();
  #else
    (void)modeEvent;
    (void)resetEvent;
    (void)serialAction;
  #endif
}

#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
bool handleJvsMenuHolds() {
  static uint32_t started = 0;
  static uint8_t gesture = 0;
  static bool tracking = false, consumed = false;
  const bool left = resetButton.isPressed(), right = modeButton.isPressed();
  if (!left && !right) {
    tracking = consumed = false;
    gesture = 0;
    modeHoldOpenedSystemMenu = false;
    return false;
  }
  if (consumed) return true;
  if (!menuScreensOwnControllerInput() || (right && !left && modeHoldOpenedSystemMenu)) {
    tracking = false;
    return false;
  }
  const uint8_t next = left && right ? 3 : left ? 1 : 2;
  const uint32_t now = millis();
  if (!tracking || gesture != next) { started = now; gesture = next; tracking = true; }
  if (static_cast<uint32_t>(now - started) < BTN_SYSTEM_MENU_HOLD_MS) return false;
  consumed = true;
  tracking = false;
  resetButton.suppressUntilRelease();
  modeButton.suppressUntilRelease();
  jvsMenuHead = jvsMenuCount = 0;
  (void)takeSerialMenuAction();
  if (left && right) {
    // Leave the child draft without saving; the explicit confirmation owns
    // the buttons next. Never approve Defaults through the hold itself.
    (void)finishSystemMenuEditorForHold(false);
    if (jvsPanelActive) (void)finishJvsPanelMenuForHold(false);
    if (analogTestActive) closeJvsAnalogWizard();
    jvsStatusActive = jvsStatusInitialized = false;
    if (isQuickConfigOpen) quickConfig.openDefaultConfirmation();
    else if (isMenuOpen) {
      closeMenu();
      isMenuOpen = true;
      openFactoryResetConfirmation();
    }
    forceMainDisplayRefresh();
    return true;
  }
  if (left) {
    (void)finishSystemMenuEditorForHold(false);
    (void)quickConfig.finishEditorForHold(false);
    closeMenusFromSerial();
    showMenuActionFeedback(MenuActionFeedback::Cancelled);
    forceMainDisplayRefresh();
    return true;
  }
  if (jvsPanelActive && !finishJvsPanelMenuForHold(true)) return true;
  if (analogTestActive && !finishJvsAnalogMenuForHold(true)) return true;
  if (isQuickConfigOpen) {
    if (!quickConfig.finishEditorForHold(true)) { buzzer.playError(); return true; }
    (void)quickConfig.saveOrConfirmNsoSinglePlayer();
    if (quickConfig.isOpen()) return true;
    isQuickConfigOpen = false;
    if (quickConfig.shouldSave()) {
      saveModeSettings();
      buzzer.playSave();
      showMenuActionFeedback(MenuActionFeedback::Saved);
    }
  } else if (isMenuOpen) {
    if (!finishSystemMenuEditorForHold(true)) return true;
    saveAndCloseMenu();
  }
  jvsStatusActive = jvsStatusInitialized = false;
  returnQuickConfigToCombinedMenu = false;
  forceMainDisplayRefresh();
  return true;
}
#endif

void openCombinedControllerSettingsMenuInternal() {
  suppressMenuControllerInputForMs(kMenuEntryInputSuppressMs);
  returnQuickConfigToCombinedMenu = true;
  isMenuOpen = false;
  isQuickConfigOpen = true;
  quickConfig.open();
#ifdef USE_I2C_DISPLAY
  display.clear();
#endif
  buzzer.playMenuEnter();
}

void handleSystemMenuUi(ButtonEvent modeEvent, ButtonEvent resetEvent, SerialMenuAction serialAction) {
  #ifdef USE_I2C_DISPLAY
    beginDisplayWire();
    const uint32_t injectedButtons = serialMenuActionToButtons(serialAction);
    if (injectedButtons != 0) {
      queueMenuControllerButtons(injectedButtons);
    }
    // The two physical MODERN2USB controls mirror their placement: the left
    // GPIO6 button advances the cursor and the right GPIO2 button selects.
    // Accept a rapid double tap as well so the button handler never turns a
    // user's short press into a discarded event.
    const bool modeBtnJustPressed =
        modeEvent == BTN_EVENT_SINGLE || modeEvent == BTN_EVENT_DOUBLE;
    const uint8_t navigateCount =
        resetEvent == BTN_EVENT_DOUBLE ? 2 :
        resetEvent == BTN_EVENT_SINGLE ? 1 : 0;
    bool resetBtnJustPressed = navigateCount != 0;
    showMenu(modeBtnJustPressed, resetBtnJustPressed);
    if (navigateCount > 1) {
      showMenu(false, true);
    }
    endDisplayWire();
  #else
    (void)modeEvent;
    (void)resetEvent;
    (void)serialAction;
  #endif
}

}  // namespace

void openCombinedControllerSettingsMenu() {
  openCombinedControllerSettingsMenuInternal();
}

void closeMenusFromSerial() {
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  if (jvsPanelActive) (void)finishJvsPanelMenuForHold(false);
  if (analogTestActive) closeJvsAnalogWizard();
  jvsStatusActive = jvsStatusInitialized = false;
#endif
  #ifdef USE_I2C_DISPLAY
    beginDisplayWire();
  #endif
  if (isQuickConfigOpen) {
    quickConfig.discard();
    isQuickConfigOpen = false;
  }
  returnQuickConfigToCombinedMenu = false;
  if (isMenuOpen) {
    discardAndCloseMenu();
  } else {
    closeMenu();
  }
  #ifdef USE_I2C_DISPLAY
    endDisplayWire();
  #endif
}

#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
// Player setup owns the OLED and both adapter buttons, so a menu or tool
// screen left open when the controller dropped can no longer be driven or
// closed. Left flagged open it holds the release gate shut for good, and its
// draft, made for the last player, would come back over the new mapping.
// Discard it, as a held Left does.
static void closeMenusForArcadeSetup() {
  if (!menuScreensOwnControllerInput()) return;
  closeMenusFromSerial();
  // closeMenu() leaves the tool screens flagged; none runs without a menu.
  if (buttonRemapActive) remapMenu.close();
  buttonRemapActive = false;
  padTestActive = padTestInitialized = false;
  analogTestActive = analogTestInitialized = false;
  pinDebugActive = pinDebugInitialized = false;
  jvsStatusActive = jvsStatusInitialized = false;
}
#endif

void openControllerMenuFromSerial() {
  closeMenusFromSerial();
  if (productUsesControllerSelectorMenu()) {
    openControllerSelectorMenu();
  } else {
    openQuickConfigMenu();
  }
}

void openSystemMenuFromSerial() {
  closeMenusFromSerial();
  openSystemOnlyMenu();
}

bool wakeFromVisibleScreensaver(ButtonEvent modeEvent, ButtonEvent resetEvent) {
  const bool modePressed = modeButton.isPressed();
  const bool resetPressed = resetButton.isPressed();
  const bool consumedWake = handleIdleUiActivity(
    modePressed || resetPressed ||
    modeEvent != BTN_EVENT_NONE || resetEvent != BTN_EVENT_NONE,
    IdleWakeSource::Buttons
  );
  if (!consumedWake) {
    return false;
  }

  // A physical press should wake immediately after debounce, but must not
  // become a menu/reboot action when that same press is released.
  if (modePressed) {
    modeButton.suppressUntilRelease();
  }
  if (resetPressed) {
    resetButton.suppressUntilRelease();
  }
  return true;
}

bool isJvsMenuSafeWindowActive() {
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  return jvsMenuInSafeWindow;
#else
  return false;
#endif
}

void serviceJvsSafeWindowMenuUi() {
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  if (deviceMode != RZORD_JVS) return;
  ButtonEvent mode = BTN_EVENT_NONE, reset = BTN_EVENT_NONE;
  while (jvsMenuCount != 0 &&
         static_cast<uint32_t>(millis() - jvsMenuQueue[jvsMenuHead].atMs) > kJvsMenuEventMaxAgeMs) {
    jvsMenuHead = static_cast<uint8_t>((jvsMenuHead + 1) % kJvsMenuQueueSize);
    --jvsMenuCount;
  }
  if (jvsMenuCount != 0) {
    mode = jvsMenuQueue[jvsMenuHead].mode;
    reset = jvsMenuQueue[jvsMenuHead].reset;
    jvsMenuHead = static_cast<uint8_t>((jvsMenuHead + 1) % kJvsMenuQueueSize);
    --jvsMenuCount;
  }
  jvsMenuInSafeWindow = true;
  serviceDeferredIdleDisplayRestore();
  handleMenuAndButtonUi(mode, reset);
  jvsMenuInSafeWindow = false;
#endif
}

void handleMenuAndButtonUi(ButtonEvent modeEvent, ButtonEvent resetEvent) {
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  // Once per loop, before any early return: the session's own queries only
  // read, so this is the only place connections and setup advance.
  usb2db15ArcadeSessionTick();
#endif
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  if (deviceMode == RZORD_JVS && !jvsMenuInSafeWindow) {
    queueJvsMenuEvents(modeEvent, resetEvent);
    return;
  }
#endif
  if (firmwareUpdateScreenActive()) {
    return;
  }
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  if (deviceMode == RZORD_JVS && handleJvsMenuHolds()) return;
#endif
  static uint32_t resetMenuDiscardHoldStart = 0;
  static uint32_t modeMenuSaveHoldStart = 0;
  static uint32_t bothMenuDefaultsHoldStart = 0;
  static bool bothMenuHoldConsumed = false;
  if (setupWizardActive()) {
    beginDisplayWire();
    const bool tutorialOwnsButtons = handleSetupWizardTutorialButtons(
        resetButton.isPressed(), modeButton.isPressed(),
        resetEvent == BTN_EVENT_SINGLE || resetEvent == BTN_EVENT_DOUBLE,
        modeEvent == BTN_EVENT_SINGLE || modeEvent == BTN_EVENT_DOUBLE, millis());
    endDisplayWire();
    if (tutorialOwnsButtons) {
      resetMenuDiscardHoldStart = modeMenuSaveHoldStart = bothMenuDefaultsHoldStart = 0;
      bothMenuHoldConsumed = false;
      handleIdleUiActivity(true);
      return;
    }
  }
  bool cabinetWizardOwnsButtons = false;
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  cabinetWizardOwnsButtons = deviceMode == RZORD_JVS;
#endif
  // JVS resolves its active editor before the outer hold action above.
  if ((isMenuOpen || isQuickConfigOpen) && !cabinetWizardOwnsButtons) {
    const uint32_t now = millis();
    const bool resetPressed = resetButton.isPressed();
    const bool modePressed = modeButton.isPressed();
    bool suppressIndividualHolds = false;

    if (bothMenuHoldConsumed) {
      if (!resetPressed && !modePressed) {
        bothMenuHoldConsumed = false;
      } else {
        suppressIndividualHolds = true;
      }
    }

    if (!bothMenuHoldConsumed && resetPressed && modePressed) {
      suppressIndividualHolds = true;
      resetMenuDiscardHoldStart = 0;
      modeMenuSaveHoldStart = 0;
      if (bothMenuDefaultsHoldStart == 0) bothMenuDefaultsHoldStart = now;
      if ((uint32_t)(now - bothMenuDefaultsHoldStart) >= BTN_SYSTEM_MENU_HOLD_MS) {
        if (isQuickConfigOpen) {
          quickConfig.openDefaultConfirmation();
        } else {
          openFactoryResetConfirmation();
        }
        buzzer.playMenuEnter();
        bothMenuHoldConsumed = true;
        bothMenuDefaultsHoldStart = 0;
        resetButton.suppressUntilRelease();
        modeButton.suppressUntilRelease();
      }
    } else if (!bothMenuHoldConsumed) {
      bothMenuDefaultsHoldStart = 0;
    }

    if (!suppressIndividualHolds && resetPressed) {
      if (resetMenuDiscardHoldStart == 0) resetMenuDiscardHoldStart = now;
      if ((uint32_t)(now - resetMenuDiscardHoldStart) >= 1500) {
        closeMenusFromSerial();
        showMenuActionFeedback(MenuActionFeedback::Cancelled);
        resetButton.suppressUntilRelease();
        resetMenuDiscardHoldStart = 0;
        forceMainDisplayRefresh();
        return;
      }
    } else if (!suppressIndividualHolds) {
      resetMenuDiscardHoldStart = 0;
    }
    if (modeHoldOpenedSystemMenu && !modePressed) {
      modeHoldOpenedSystemMenu = false;
    }
    if (!suppressIndividualHolds && modePressed && !modeHoldOpenedSystemMenu) {
      if (modeMenuSaveHoldStart == 0) modeMenuSaveHoldStart = now;
      if ((uint32_t)(now - modeMenuSaveHoldStart) >= BTN_SYSTEM_MENU_HOLD_MS) {
        if (isQuickConfigOpen) {
          // Same path as Start/Save: enabling native NSO first asks for the
          // 1P confirmation, and a rejected save leaves the menu open.
          (void)quickConfig.saveOrConfirmNsoSinglePlayer();
          if (quickConfig.isOpen()) {
            modeButton.suppressUntilRelease();
            modeMenuSaveHoldStart = 0;
            return;
          }
          isQuickConfigOpen = false;
          if (quickConfig.shouldSave()) {
            saveModeSettings();
            buzzer.playSave();
            showMenuActionFeedback(MenuActionFeedback::Saved);
          }
        } else {
          saveAndCloseMenu();
        }
        modeButton.suppressUntilRelease();
        modeMenuSaveHoldStart = 0;
        display.clear();
        forceMainDisplayRefresh();
        return;
      }
    } else if (!suppressIndividualHolds) {
      modeMenuSaveHoldStart = 0;
    }
  } else {
    resetMenuDiscardHoldStart = 0;
    modeMenuSaveHoldStart = 0;
    bothMenuDefaultsHoldStart = 0;
    bothMenuHoldConsumed = false;
    if (!modeButton.isPressed()) modeHoldOpenedSystemMenu = false;
  }
  if (firmwareRollbackNoticeActive()) {
    if (modeEvent == BTN_EVENT_SINGLE || modeEvent == BTN_EVENT_DOUBLE) {
      (void)acknowledgeFirmwareRollbackNotice();
    }
    return;
  }


  const SerialMenuAction serialAction = takeSerialMenuAction();
  if (serialAction == SerialMenuAction::Menu && !isMenuOpen && !isQuickConfigOpen) {
    modeEvent = BTN_EVENT_SINGLE;
  }

  #if defined(ADAPT_FEATURE_DINPUT_MAPPING_WIZARD)
  if (!isMenuOpen && !isQuickConfigOpen && hidMappingWizardPending()
  #if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
      && !usb2db15GameSetupActive()
  #endif
  ) {
    beginDisplayWire();
    const bool mappingOwnsUi = handleHidMappingWizard(modeEvent, resetEvent);
    endDisplayWire();
    if (mappingOwnsUi) {
      return;
    }
  }
  #endif

  // An unknown generic HID must finish the shared mapper before a product UI
  // can consume its normalized buttons. USB2DB15 otherwise waits forever on
  // the controller-profile splash while the hidden HID wizard waits behind it.
  #if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  if (usb2db15ArcadeUiActive()) {
    closeMenusForArcadeSetup();
    beginDisplayWire();
    const bool mappingOwnsUi =
        handleUsb2db15ArcadeMapping(modeEvent, resetEvent);
    endDisplayWire();
    if (mappingOwnsUi) {
      return;
    }
  }
  #endif

  // Factory defaults offer first-boot setup once the home screen is up. It
  // runs as a System menu page, so the menu owns the controller meanwhile.
  if (!isMenuOpen && !isQuickConfigOpen && setupWizardShouldOpen()) {
    resetSystemMenuOpenState();
    suppressMenuControllerInputForMs(kMenuEntryInputSuppressMs);
    system_menu_only = true;
    isMenuOpen = true;
    beginSetupWizard();
    buzzer.playMenuEnter();
  }

  if (!isMenuOpen && !isQuickConfigOpen) {
    handleHomeButtonEvents(modeEvent, resetEvent);
  } else if (isQuickConfigOpen) {
    handleQuickConfigUi(modeEvent, resetEvent, serialAction);
  } else if (isMenuOpen) {
    handleSystemMenuUi(modeEvent, resetEvent, serialAction);
  }
}

void runPlatformMenuHotkeys() {
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
  const uint32_t quickCombo = directionalMenuCombo(direction_hotkey::QuickMenu);
  const uint32_t systemCombo = directionalMenuCombo(direction_hotkey::SystemMenu);
#else
  const uint32_t quickCombo = menu_menu_hotkey != 0 ? menu_hotkey_combo : 0;
  const uint32_t systemCombo = menu_system_menu_hotkey != 0 ? menu_system_hotkey_combo : 0;
#endif
  const bool controllerSelectorMenu = productUsesControllerSelectorMenu();
  const uint32_t activeSystemCombo = systemCombo;

  if ((quickCombo == 0 && activeSystemCombo == 0) || isMenuOpen ||
      isQuickConfigOpen || featureModulesOwnControllerInput()) {
    return;
  }

  static uint32_t menuHotkeyStartTime = 0;
  static uint32_t systemHotkeyStartTime = 0;
  static bool menuHotkeyActive = false;
  static bool systemHotkeyActive = false;

  if (!(max_devices > 0 && controllerFrameConst(0).connected)) {
    menuHotkeyActive = false;
    systemHotkeyActive = false;
    return;
  }

  const uint32_t physicalHotkeyButtons = pre_transform_hotkey_buttons[0] & HOTKEY_ALLOWED_BUTTONS;
  const bool canTriggerQuick = quickCombo != 0 && physicalHotkeyButtons == quickCombo;
  const bool canTriggerSystem = activeSystemCombo != 0 && physicalHotkeyButtons == activeSystemCombo;
  if (!menuHotkeyActive && !systemHotkeyActive && !canTriggerQuick && !canTriggerSystem) {
    return;
  }

  if (controllerSelectorMenu) {
    maybeTriggerHeldMenuHotkey(
        quickCombo, menuHotkeyStartTime, menuHotkeyActive,
        openControllerSelectorMenuFromHotkey);
    maybeTriggerHeldMenuHotkey(
        activeSystemCombo, systemHotkeyStartTime, systemHotkeyActive,
        openSystemOnlyMenuFromHotkey);
  } else {
    maybeTriggerHeldMenuHotkey(quickCombo, menuHotkeyStartTime, menuHotkeyActive, openQuickConfigMenuFromHotkey);
    maybeTriggerHeldMenuHotkey(activeSystemCombo, systemHotkeyStartTime, systemHotkeyActive, openSystemOnlyMenuFromHotkey);
  }
}

void saveModeSettings() {
  uint8_t tempRates[TURBO_BTN_COUNT];
  PerModeQuickSettings selectedSettings;
  DeviceEnum newInputMode = quickConfig.getTempInputMode();
  outputMode_t newOutputMode = quickConfig.getTempOutputMode();

  quickConfig.getTempRates(tempRates);
  quickConfig.getTempQuickSettings(newInputMode, selectedSettings);
  bool saved = quickConfig.saveStagedRemaps();
  saved = quickConfig.saveStagedMouseSettings() && saved;
  saveModeSettings(newInputMode, newOutputMode, tempRates, selectedSettings);
  if (!saved) noteMenuSaveFailed();
}
