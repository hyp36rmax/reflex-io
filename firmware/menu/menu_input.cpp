#include "../product_config.h"

#include "menu_input.h"
#include "menu.h"
#include "../product_config.h"
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  #include "../features/usb2db15/usb2db15_selector_navigation.h"
#endif

#include <Arduino.h>
#include <cstring>

#include "../platform/buzzer.h"
#include "../input/shared/input_button_bits.h"

namespace {

uint32_t menu_controller_input_suppress_until_ms = 0;
bool menu_controller_input_suppress_until_release = false;
uint32_t menu_controller_input_release_since_ms = 0;
uint32_t pending_menu_action_buttons = 0;
uint32_t queued_menu_controller_buttons = 0;
constexpr uint8_t kAnalogButtonWakeThreshold = 16;
constexpr uint32_t kMenuInputReleaseStableMs = 100;
constexpr uint32_t kCtrlUp = 0x01;
constexpr uint32_t kCtrlDown = 0x02;
constexpr uint32_t kCtrlLeft = 0x04;
constexpr uint32_t kCtrlRight = 0x08;
constexpr uint32_t kCtrlConfirm = 0x10;
constexpr uint32_t kCtrlBack = 0x20;
constexpr uint32_t kCtrlQuickHotkey = 0x40;
constexpr uint32_t kCtrlSystemHotkey = 0x80;
constexpr uint32_t kCtrlStart = 0x100;
constexpr uint32_t kCtrlDirectionMask = kCtrlUp | kCtrlDown | kCtrlLeft | kCtrlRight;
constexpr uint32_t kCtrlMenuActionMask =
    kCtrlDirectionMask | kCtrlConfirm | kCtrlBack | kCtrlStart;
constexpr uint32_t kCtrlDeferredActionMask = kCtrlConfirm | kCtrlBack | kCtrlStart;
constexpr int16_t kMenuAnalogNavigationThreshold8Bit = 32;
constexpr int16_t kMenuAnalogNavigationThreshold12Bit = 512;
constexpr int16_t kMenuAnalogNavigationThreshold16Bit = 8192;
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
Usb2Db15MenuPressGate usb2db15ConfirmGate;
Usb2Db15MenuPressGate usb2db15BackGate;
Usb2Db15MenuPressGate usb2db15StartGate;
#endif

int16_t menuAnalogNavigationThreshold(const controller_state_t& report) {
  switch (report.sticks_precision_bits) {
    case ANALOG_STICK_PRECISION_16:
      return kMenuAnalogNavigationThreshold16Bit;
    case ANALOG_STICK_PRECISION_12:
      return kMenuAnalogNavigationThreshold12Bit;
    case ANALOG_STICK_PRECISION_8:
    default:
      return kMenuAnalogNavigationThreshold8Bit;
  }
}

bool usesNintendoModernFaceOrder(const controller_state_t& report) {
  const char* type = report.controller_type_name;
  const size_t length = std::strlen(type);
  return std::strncmp(type, "Switch", 6) == 0 ||
         std::strcmp(type, "Joy-Con") == 0 ||
         (length >= 4 && std::strcmp(type + length - 4, " NSO") == 0) ||
         std::strcmp(type, "WiiU Pro") == 0 ||
         std::strcmp(type, "Wii U Pro") == 0 ||
         std::strcmp(type, "MF WiiU Pro") == 0;
}

bool usesNintendoClassicFaceOrder(DeviceEnum mode) {
  switch (mode) {
#ifdef ENABLE_INPUT_NES
    case RZORD_NES:
#endif
#ifdef ENABLE_INPUT_SNES
    case RZORD_SNES:
#endif
#ifdef ENABLE_INPUT_N64
    case RZORD_N64:
#endif
#ifdef ENABLE_INPUT_GAMECUBE
    case RZORD_GAMECUBE:
#endif
#ifdef ENABLE_INPUT_WII
    case RZORD_WII:
#endif
#ifdef ENABLE_INPUT_VBOY
    case RZORD_VBOY:
#endif
      return true;
    default: return false;
  }
}

bool menuControllerSwapConfirmBack(const controller_state_t& report, DeviceEnum mode) {
  if (menu_ok_back != MENU_OK_BACK_EAST && menu_ok_back != MENU_OK_BACK_SOUTH) return false;
  // Arcade and non-diamond button labels retain their existing menu controls.
  switch (mode) {
#ifdef ENABLE_INPUT_JVS
    case RZORD_JVS: return false;
#endif
#ifdef ENABLE_INPUT_PCE
    case RZORD_PCE: return false;
#endif
#ifdef ENABLE_INPUT_JAGUAR
    case RZORD_JAGUAR: return false;
#endif
    default: break;
  }
  // A one-button controller has nothing to swap with: its button must stay OK.
  if (getRemapButtonCount(mode) < 2) return false;
  if (usesNintendoModernFaceOrder(report) || usesNintendoClassicFaceOrder(mode)) {
    return menu_ok_back == MENU_OK_BACK_SOUTH;
  }
  return menu_ok_back == MENU_OK_BACK_EAST;
}

uint32_t menuControllerButtonBits(const controller_state_t& report, DeviceEnum mode) {
  const uint32_t buttons = report.digital_buttons;
  if (!menuControllerSwapConfirmBack(report, mode)) return buttons;
  return (buttons & ~(INPUT_A | INPUT_B)) |
         ((buttons & INPUT_A) ? INPUT_B : 0) |
         ((buttons & INPUT_B) ? INPUT_A : 0);
}

const char* menuButtonNameForSlot(DeviceEnum mode, uint8_t slot) {
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  // A cabinet session indexes slots by arcade control; keep the pad names.
  return getRemapButtonName(mode, slot);
#else
  // Display order is not slot order on every input: Virtual Boy and neGcon
  // list other controls first, so index 0 is not the button that confirms.
  const uint8_t index = getRemapButtonDisplayIndex(mode, slot);
  return getRemapButtonName(mode, index != 0xFF ? index : slot);
#endif
}

}  // namespace

bool menuControllerConfirmPressed(const controller_state_t& report, DeviceEnum mode) {
  const uint32_t buttons = menuControllerButtonBits(report, mode);

  // USB/Bluetooth Nintendo reports use the shared positional neutral frame:
  // INPUT_A is south (printed B), INPUT_B is east (printed A).
  if (usesNintendoModernFaceOrder(report)) {
    return (buttons & INPUT_B) != 0;
  }

  switch (mode) {
    #ifdef ENABLE_INPUT_JVS
    case RZORD_JVS:
      return (buttons & (INPUT_A | INPUT_START)) != 0;
    #endif
    #ifdef ENABLE_INPUT_PCE
    case RZORD_PCE:
      return (buttons & INPUT_B) != 0;
    #endif
    #ifdef ENABLE_INPUT_PSX_JOG
    case RZORD_PSX_JOG:
      return (buttons & INPUT_A) != 0;
    #endif
    #ifdef ENABLE_INPUT_JAGUAR
    case RZORD_JAGUAR:
      return (buttons & INPUT_B) != 0;
    #endif
    default:
      return (buttons & INPUT_A) != 0;
  }
}

bool menuControllerBackPressed(const controller_state_t& report, DeviceEnum mode) {
  const uint32_t buttons = menuControllerButtonBits(report, mode);

  if (usesNintendoModernFaceOrder(report)) {
    return (buttons & INPUT_A) != 0;
  }

  switch (mode) {
    #ifdef ENABLE_INPUT_JVS
    case RZORD_JVS:
      return (buttons & (INPUT_B | INPUT_SELECT)) != 0;
    #endif
    #ifdef ENABLE_INPUT_PCE
    case RZORD_PCE:
      return (buttons & INPUT_A) != 0;
    #endif
    #ifdef ENABLE_INPUT_PSX_JOG
    case RZORD_PSX_JOG:
      return (buttons & INPUT_B) != 0;
    #endif
    #ifdef ENABLE_INPUT_JAGUAR
    case RZORD_JAGUAR:
      return (buttons & INPUT_A) != 0;
    #endif
    default:
      return (buttons & INPUT_B) != 0;
  }
}

const char* getMenuConfirmButtonName(DeviceEnum mode) {
  const bool swap = menuControllerSwapConfirmBack(controller_state_t{}, mode);
  switch (mode) {
    #ifdef ENABLE_INPUT_JVS
    case RZORD_JVS:
      return "P2/Start";
    #endif
    #ifdef ENABLE_INPUT_PCE
    case RZORD_PCE:
      return "I";
    #endif
    #ifdef ENABLE_INPUT_PSX_JOG
    case RZORD_PSX_JOG:
      return swap ? "O" : "X";
    #endif
    #ifdef ENABLE_INPUT_JAGUAR
    case RZORD_JAGUAR:
      return "A";
    #endif
    default:
      return menuButtonNameForSlot(mode, swap ? 1 : 0);
  }
}

const char* getMenuBackButtonName(DeviceEnum mode) {
  const bool swap = menuControllerSwapConfirmBack(controller_state_t{}, mode);
  switch (mode) {
    #ifdef ENABLE_INPUT_JVS
    case RZORD_JVS:
      return "P3/Coin";
    #endif
    #ifdef ENABLE_INPUT_PCE
    case RZORD_PCE:
      return "II";
    #endif
    #ifdef ENABLE_INPUT_PSX_JOG
    case RZORD_PSX_JOG:
      return swap ? "X" : "O";
    #endif
    #ifdef ENABLE_INPUT_JAGUAR
    case RZORD_JAGUAR:
      return "B";
    #endif
    default:
      if (getRemapButtonCount(mode) < 2) return "--";
      return menuButtonNameForSlot(mode, swap ? 0 : 1);
  }
}

bool menuControllerUpPressed(const controller_state_t& report) {
  const int16_t threshold = menuAnalogNavigationThreshold(report);
  return report.PAD_U ||
         (report.HAS_ANALOG_STICK_MAIN &&
          report.LY <= -threshold);
}

bool menuControllerDownPressed(const controller_state_t& report) {
  const int16_t threshold = menuAnalogNavigationThreshold(report);
  return report.PAD_D ||
         (report.HAS_ANALOG_STICK_MAIN &&
          report.LY >= threshold);
}

bool menuControllerLeftPressed(const controller_state_t& report) {
  return report.PAD_L;
}

bool menuControllerRightPressed(const controller_state_t& report) {
  return report.PAD_R;
}
uint32_t getMenuWakeButtons(const controller_state_t& report, DeviceEnum mode) {
  uint32_t ctrlButtons = 0;

  if (menuControllerUpPressed(report)) ctrlButtons |= kCtrlUp;
  if (menuControllerDownPressed(report)) ctrlButtons |= kCtrlDown;
  if (menuControllerLeftPressed(report)) ctrlButtons |= kCtrlLeft;
  if (menuControllerRightPressed(report)) ctrlButtons |= kCtrlRight;
  if (menuControllerConfirmPressed(report, mode)) ctrlButtons |= kCtrlConfirm;
  if (menuControllerBackPressed(report, mode)) ctrlButtons |= kCtrlBack;

  const uint32_t buttons = report.digital_buttons;
  if (buttons & INPUT_START) ctrlButtons |= kCtrlStart;
  if ((buttons & (INPUT_PAD_L | INPUT_START)) == (INPUT_PAD_L | INPUT_START)) ctrlButtons |= kCtrlQuickHotkey;
  if ((buttons & (INPUT_PAD_R | INPUT_START)) == (INPUT_PAD_R | INPUT_START)) ctrlButtons |= kCtrlSystemHotkey;

  return ctrlButtons;
}
uint32_t currentMenuActionButtons() {
  uint32_t buttons = 0;
  const uint8_t playerCount =
      max_devices < MAX_USB_OUT ? max_devices : MAX_USB_OUT;
  for (uint8_t player = 0; player < playerCount; ++player) {
    const controller_state_t& frame = controllerFrameConst(player);
    if (frame.connected) {
      buttons |= getMenuWakeButtons(frame, deviceMode);
    }
  }
  return buttons & kCtrlMenuActionMask;
}

uint32_t getScreensaverWakeButtons(const controller_state_t& report) {
  uint32_t buttons = report.digital_buttons;

  if (report.ANALOG_A > kAnalogButtonWakeThreshold) buttons |= INPUT_A;
  if (report.ANALOG_B > kAnalogButtonWakeThreshold) buttons |= INPUT_B;
  if (report.ANALOG_X > kAnalogButtonWakeThreshold) buttons |= INPUT_X;
  if (report.ANALOG_Y > kAnalogButtonWakeThreshold) buttons |= INPUT_Y;
  if (report.ANALOG_L1 > kAnalogButtonWakeThreshold) buttons |= INPUT_L1;
  if (report.ANALOG_R1 > kAnalogButtonWakeThreshold) buttons |= INPUT_R1;
  if (report.ANALOG_L2 > kAnalogButtonWakeThreshold) buttons |= INPUT_L2;
  if (report.ANALOG_R2 > kAnalogButtonWakeThreshold) buttons |= INPUT_R2;
  if (report.ANALOG_PAD_U > kAnalogButtonWakeThreshold) buttons |= INPUT_PAD_U;
  if (report.ANALOG_PAD_D > kAnalogButtonWakeThreshold) buttons |= INPUT_PAD_D;
  if (report.ANALOG_PAD_L > kAnalogButtonWakeThreshold) buttons |= INPUT_PAD_L;
  if (report.ANALOG_PAD_R > kAnalogButtonWakeThreshold) buttons |= INPUT_PAD_R;

  return buttons;
}

void suppressMenuControllerInputForMs(uint16_t ms) {
  menu_controller_input_suppress_until_ms = millis() + ms;
  menu_controller_input_suppress_until_release = true;
  menu_controller_input_release_since_ms = 0;
  pending_menu_action_buttons = 0;
}

bool isMenuControllerInputSuppressed() {
  const uint32_t now = millis();
  if (menu_controller_input_suppress_until_ms != 0 &&
      (int32_t)(now - menu_controller_input_suppress_until_ms) < 0) {
    return true;
  }
  menu_controller_input_suppress_until_ms = 0;
  if (menu_controller_input_suppress_until_release) {
    if (currentMenuActionButtons() != 0) {
      menu_controller_input_release_since_ms = 0;
      return true;
    }
    if (menu_controller_input_release_since_ms == 0) {
      menu_controller_input_release_since_ms = now;
      return true;
    }
    if ((uint32_t)(now - menu_controller_input_release_since_ms) < kMenuInputReleaseStableMs) {
      return true;
    }
    menu_controller_input_suppress_until_release = false;
    menu_controller_input_release_since_ms = 0;
  }
  return false;
}

void queueMenuControllerButtons(uint32_t buttons) {
  queued_menu_controller_buttons |= buttons & kCtrlMenuActionMask;
}

MenuControllerInput readMenuControllerInput() {
  MenuControllerInput input = {false, false, false, false, false, false, false, 0};

  static uint32_t lastCtrlButtons = 0;
  static uint32_t ctrlRepeatTimer = 0;
  static uint32_t lastActionTime = 0;

  const uint32_t CTRL_REPEAT_DELAY = 400;
  const uint32_t CTRL_REPEAT_RATE = 100;
  const uint32_t CTRL_DEBOUNCE_MS = 50;

  const uint32_t ctrlButtons = currentMenuActionButtons();
  const uint32_t injectedButtons = queued_menu_controller_buttons & kCtrlMenuActionMask;
  queued_menu_controller_buttons = 0;

  input.buttons = ctrlButtons | injectedButtons;

  uint32_t now = millis();
  if (isMenuControllerInputSuppressed()) {
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
    usb2db15ConfirmGate.reset();
    usb2db15BackGate.reset();
    usb2db15StartGate.reset();
#endif
    lastCtrlButtons = ctrlButtons;
    ctrlRepeatTimer = now + CTRL_REPEAT_DELAY;
    lastActionTime = now;
    pending_menu_action_buttons = 0;
    input.buttons = injectedButtons;
    return input;
  }

  bool canAct = (now - lastActionTime >= CTRL_DEBOUNCE_MS);

  uint32_t ctrlPressed = ctrlButtons & ~lastCtrlButtons;
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  // USB reports can contain a one-frame release or bounce.  Menu-only
  // confirm/back must see a stable neutral interval before another edge;
  // directional scrolling keeps the normal deliberate repeat policy below.
  const bool confirmPressed = usb2db15ConfirmGate.sample(
      (ctrlButtons & kCtrlConfirm) != 0, now);
  const bool backPressed = usb2db15BackGate.sample(
      (ctrlButtons & kCtrlBack) != 0, now);
  const bool startPressed = usb2db15StartGate.sample(
      (ctrlButtons & kCtrlStart) != 0, now);
  ctrlPressed &= ~kCtrlDeferredActionMask;
  if (confirmPressed) ctrlPressed |= kCtrlConfirm;
  if (backPressed) ctrlPressed |= kCtrlBack;
  if (startPressed) ctrlPressed |= kCtrlStart;
#endif
  uint32_t ctrlReleased = ~ctrlButtons & lastCtrlButtons;

  if (ctrlPressed) {
    ctrlRepeatTimer = now + CTRL_REPEAT_DELAY;
  }
  if (ctrlReleased) {
  }

  bool ctrlRepeat = false;
  if (canAct && (ctrlButtons & kCtrlDirectionMask)) {
    if ((int32_t)(now - ctrlRepeatTimer) >= 0) {
      ctrlRepeat = true;
      ctrlRepeatTimer = now + CTRL_REPEAT_RATE;
    }
  }

  if (!canAct) {
    // Queue non-repeat action edges through the short debounce window.
    // Otherwise a quick confirm/save after moving to Save/Default can be
    // pressed and released before the menu is allowed to act on it.
    pending_menu_action_buttons |=
        ((ctrlPressed | injectedButtons) & kCtrlDeferredActionMask);
    lastCtrlButtons = ctrlButtons;
    return input;
  }

  const uint32_t actionPressed = ctrlPressed | injectedButtons;
  input.upJust    = (actionPressed & kCtrlUp) || ((ctrlButtons & kCtrlUp) && ctrlRepeat);
  input.downJust  = (actionPressed & kCtrlDown) || ((ctrlButtons & kCtrlDown) && ctrlRepeat);
  input.leftJust  = (actionPressed & kCtrlLeft) || ((ctrlButtons & kCtrlLeft) && ctrlRepeat);
  input.rightJust = (actionPressed & kCtrlRight) || ((ctrlButtons & kCtrlRight) && ctrlRepeat);
  input.aJust     = ((actionPressed | pending_menu_action_buttons) & kCtrlConfirm) != 0;
  input.bJust     = ((actionPressed | pending_menu_action_buttons) & kCtrlBack) != 0;
  input.startJust = ((actionPressed | pending_menu_action_buttons) & kCtrlStart) != 0;
  pending_menu_action_buttons = 0;

  if (input.upJust || input.downJust || input.leftJust || input.rightJust ||
      input.aJust || input.bJust || input.startJust) {
    lastActionTime = now;
  }

  lastCtrlButtons = ctrlButtons;

  return input;
}
