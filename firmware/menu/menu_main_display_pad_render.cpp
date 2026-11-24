#include "../product_config.h"

#include "menu.h"
#include "menu_mode_state.h"
#include "menu_main_display_internal.h"
#include "menu_home_analog_gauge.h"
#include "menu_home_pad_geometry.h"
#include "menu_classic_wheel_art.h"
#include "menu_pad_layouts_internal.h"
#ifdef ENABLE_INPUT_DREAMCAST
#include "../core/dreamcast_twin_stick.h"
#include "menu_dreamcast_twin_stick_home.h"
#endif
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
#include "../features/usb2db15/usb2db15_arcade_mapping.h"
#include "../features/usb2db15/usb2db15_arcade_session.h"
#include "../output/db15/out_db15.h"
#include "menu_usb2db15_arcade.h"
#include "menu_usb2db15_home.h"
#endif
#ifdef ENABLE_INPUT_SNES
#include "menu_ntt_data_home.h"
#endif
#ifdef ENABLE_INPUT_PSX
#include "menu_jogcon_dial.h"
#include "../input/psx/input_psx_jogcon_runtime_state.h"
#include "../input/psx/input_psx_runtime_state.h"
#endif
#include "pad_display.h"

#include "../core/controller_settings_state.h"
#include "../core/hid_mapping_profiles.h"
#include "../core/controller_output_cache_state.h"
#include "../core/turbo.h"
#include "../input/runtime/input_adapter_runtime.h"
#include "../input/runtime/input_frame_runtime.h"
#include "../input/shared/input_button_bits.h"
#ifdef ENABLE_INPUT_USB_BT
#include "../input/usb_bt_composite/Input_UsbBt.h"
#endif
#ifdef ENABLE_INPUT_USB
#include "../input/usb_host/input_usb/input_usb.h"
#endif
#include "../output/auth/auth_storage.h"
#include "../output/auth/auth_status.h"
#if defined(ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT)
#include "../output/auth/xbone_auth_passthrough.h"
#endif
#include "../output/output_capabilities.h"
#include "menu_runtime_state.h"

#ifdef ENABLE_INPUT_JVS
#include "../input/jvs/jvs_host_runtime.h"
#include "menu_jvs_driving_home.h"
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
#include "../input/jvs/jvs_analog_range_runtime.h"
#include "../input/jvs/jvs_panel_map.h"
#endif
#endif

#include <cstdio>
#include <cstring>

namespace menu_main_display_internal {

namespace {

constexpr uint32_t kJoystickDirectionMask =
  GPAD_UP | GPAD_DOWN | GPAD_LEFT | GPAD_RIGHT;
constexpr uint8_t kPadGlyphWidth = 6;
constexpr uint8_t kWideCircleRightHalfOffset = 5;
constexpr uint8_t kHomePadRowOffset = 3;
constexpr uint8_t kSecondPadColOffset = 66;
constexpr uint8_t kCenteredPadColOffset = 34;
constexpr uint8_t kAnalogTriggerGaugeMaxWidth = 18;
constexpr uint8_t kAnalogTriggerGaugeMinWidth = 6;
constexpr uint8_t kAnalogTriggerGaugeEmpty = 0x22;
constexpr uint8_t kAnalogTriggerGaugeFilled = 0x3E;
constexpr uint8_t kAnalogTriggerGaugeRedrawMs = 16;
constexpr uint8_t kDreamcastWheelTriggerGaugeWidth = 26;
constexpr uint8_t kDreamcastWheelButtonPlusCol = 54;
constexpr uint8_t kDreamcastWheelRightEdge =
  kDreamcastWheelButtonPlusCol + kPadGlyphWidth - 1;
constexpr uint8_t kDreamcastWheelPanelWidth = kDreamcastWheelRightEdge + 1;
constexpr uint8_t kSpinnerPaddlePanelWidth =
  kDreamcastWheelPanelWidth - kPadGlyphWidth;
constexpr uint8_t kDreamcastWheelRightTriggerCol =
  kDreamcastWheelRightEdge + 1 - kDreamcastWheelTriggerGaugeWidth;
constexpr uint8_t kDreamcastWheelAxisRow = 1;
constexpr uint8_t kDreamcastWheelButtonsRow = 2;
constexpr uint8_t kSaturnWheelPanelWidth = 62;
constexpr uint8_t kSaturnWheelTriggerGaugeWidth = kDreamcastWheelTriggerGaugeWidth;
constexpr uint8_t kSaturnWheelRightTriggerCol =
  kSaturnWheelPanelWidth - kSaturnWheelTriggerGaugeWidth;
constexpr uint8_t kSaturnWheelAxisRow = 1;
constexpr uint8_t kSaturnWheelButtonsRow = 2;
constexpr uint8_t kNeGconTwistGaugeRow = 7;
constexpr uint8_t kNeGconTwistGaugeWidth = 60;
constexpr uint8_t kNeGconPressureGaugePages = 2;
constexpr uint8_t kNeGconPressureGaugeWidth = 6;
constexpr uint8_t kNeGconPressureGaugeHeight = kNeGconPressureGaugePages * 8 - 1;
constexpr uint8_t kTriggerBothGlyphInsetChars = 2;
constexpr uint16_t kTurboVisualSlowMs = 9 * FRAME_MS;
constexpr uint16_t kTurboVisualMediumMs = 6 * FRAME_MS;
constexpr uint16_t kTurboVisualFastMs = 3 * FRAME_MS;
constexpr uint16_t kTurboVisualMaxMs = 2 * FRAME_MS;

#ifdef ENABLE_INPUT_SNES
bool renderNttDataHome(bool forceRedraw, bool* exited) {
  static bool active = false;
  static uint8_t lastPlayer = 0xFF;
  static uint32_t previous = 0;
  uint8_t count = 0;
  uint8_t player = 0xFF;
  *exited = false;
  if (deviceMode == RZORD_SNES) {
    for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
      if (controllerFrameConst(i).connected) {
        ++count;
        player = i;
      }
    }
  }
  const bool show = count == 1 &&
      std::strcmp(controllerFrameConst(player).controller_type_name, "NTT Data") == 0;
  if (!show) {
    *exited = active;
    active = false;
    lastPlayer = 0xFF;
    return false;
  }
  // Use all physical digital bits, including EXTRA5..9 which are not part of
  // the compact pad mask. Never depend on USB output mapping or axis activity.
  const uint32_t state = controllerFrameConst(player).digital_buttons;
  display.setFont(ReflexPad5x7);
  ntt_data_home::draw(display, state, previous, forceRedraw || !active || lastPlayer != player);
  display.setFont(System5x7);
  previous = state;
  active = true;
  lastPlayer = player;
  return true;
}

// The HVC-051 (input/snes/famicom_network_reader.h) in NES or SNES mode,
// drawn like the NTT panel with its own layout.
bool renderFamicomNetworkHome(bool forceRedraw, bool* exited) {
  static bool active = false;
  static uint8_t lastPlayer = 0xFF;
  static uint32_t previous = 0;
  uint8_t count = 0;
  uint8_t player = 0xFF;
  *exited = false;
  if (deviceMode == RZORD_NES || deviceMode == RZORD_SNES) {
    for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
      if (controllerFrameConst(i).connected) {
        ++count;
        player = i;
      }
    }
  }
  const bool show = count == 1 &&
      std::strcmp(controllerFrameConst(player).controller_type_name, "FC Network") == 0;
  if (!show) {
    *exited = active;
    active = false;
    lastPlayer = 0xFF;
    return false;
  }
  const uint32_t state = controllerFrameConst(player).digital_buttons;
  display.setFont(ReflexPad5x7);
  ntt_data_home::drawFamicomNetwork(display, state, previous,
                                    forceRedraw || !active || lastPlayer != player);
  display.setFont(System5x7);
  previous = state;
  active = true;
  lastPlayer = player;
  return true;
}
#endif

#ifdef ENABLE_INPUT_DREAMCAST
// Full-width HKT-7500 panel; retain normal multi-controller presentation.
bool renderDreamcastTwinStickHome(bool forceRedraw, bool* exited) {
  static bool active = false;
  static uint8_t lastPlayer = 0xFF;
  static uint16_t previous = 0;
  *exited = false;
  uint8_t count = 0;
  uint8_t player = 0xFF;
  if (deviceMode == RZORD_DREAMCAST) {
    for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
      if (!controllerFrameConst(i).connected) continue;
      ++count;
      player = i;
    }
  }
  const bool show = count == 1 && player != 0xFF &&
      controllerFrameIsDreamcastTwinStick(controllerFrameConst(player));
  if (!show) {
    *exited = active;
    active = false;
    lastPlayer = 0xFF;
    previous = 0;
    return false;
  }
  const uint16_t state = dreamcast_twin_stick_home::read(controllerFrameConst(player));
  display.setFont(ReflexPad5x7);
  dreamcast_twin_stick_home::draw(display, state, previous,
                                 forceRedraw || !active || lastPlayer != player);
  display.setFont(System5x7);
  previous = state;
  active = true;
  lastPlayer = player;
  return true;
}
#endif

#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
bool renderUsb2db15ArcadeHome(bool forceRedraw) {
  static bool active = false;
  static uint32_t previousState = 0;
  static bool previousConnected = false;
  static const char* previousTitle = nullptr;
  static char previousName[16] = {};
  static const char* previousHint = nullptr;
  const bool connected = controllerFrameConst(0).connected;
  const char* name = connected ? controllerFrameConst(0).controller_type_name : "";
  // While play waits for every control to be released (after a menu, a
  // resting trigger, a drifting stick), the name row says which input holds it.
  const char* hint = connected ? usb2db15ArcadeReleaseHint() : nullptr;
  const uint32_t state = connected
      ? buildButtonMaskFromDigitalButtons(db15_output_button_state())
      : 0;
  const Usb2Db15ArcadeView& view = usb2db15ArcadeView();
  const bool identityChanged = connected != previousConnected ||
      std::strncmp(name, previousName, sizeof(previousName)) != 0 ||
      hint != previousHint;
  if (!forceRedraw && active && state == previousState &&
      view.title == previousTitle && !identityChanged) {
    return true;
  }

  // Input/turbo updates change only the composed panel. Redrawing the text
  // clears these rows on the physical OLED and made the whole header flash.
  if (forceRedraw || !active || identityChanged) {
    // Preserve the shared tournament badge when that safety policy is active.
    if (menu_kiosk_mode == 0) usb2db15_arcade_home::drawTitle(display);
    if (hint != nullptr) usb2db15_arcade_home::drawControllerName(display, hint);
    else usb2db15_arcade_home::drawControllerName(display, name);
  }
  usb2db15_arcade_home::draw(display, state, view,
                              usb2db15_arcade_home::kLivePanelFirstPage);
  previousState = state;
  previousConnected = connected;
  previousTitle = view.title;
  previousHint = hint;
  std::strncpy(previousName, name, sizeof(previousName) - 1);
  previousName[sizeof(previousName) - 1] = '\0';
  active = true;
  return true;
}
#endif

bool isJoystickDirectionMask(uint32_t mask) {
  return mask == GPAD_UP ||
         mask == GPAD_DOWN ||
         mask == GPAD_LEFT ||
         mask == GPAD_RIGHT;
}

bool isCapabilityFilteredGenericPad(const controller_state_t* frame) {
  if (frame == nullptr || !frame->connected) {
    return false;
  }
  const char* name = frame->controller_type_name;
  return std::strncmp(name, "USB ", 4) == 0 ||
         std::strncmp(name, "OctaPro", 7) == 0 ||
         std::strcmp(name, "Dinput") == 0 ||
         std::strcmp(name, "USB") == 0 ||
         std::strcmp(name, "BT Pad") == 0;
}

bool classifiedGenericLayoutForFrame(const controller_state_t* frame,
                                     uint8_t& layout) {
#ifdef ENABLE_INPUT_USB
  if (frame != nullptr &&
      (std::strncmp(frame->controller_type_name, "USB ", 4) == 0 ||
       std::strncmp(frame->controller_type_name, "OctaPro", 7) == 0 ||
       std::strcmp(frame->controller_type_name, "Dinput") == 0)) {
    for (uint8_t player = 0; player < MAX_USB_OUT; ++player) {
      if (frame != &controllerFrameConst(player)) continue;
      uint8_t sourceSlot = player;
#ifdef ENABLE_INPUT_USB_BT
      const UsbBtSlotOwner owner = usbBtLogicalPlayerOwner(player);
      if (owner.source != USB_BT_INPUT_USB) return false;
      sourceSlot = owner.source_slot;
#endif
      bool classified = false;
      layout = inputusb_generic_mapping_layout(sourceSlot, &classified);
      return classified;
    }
  }
#else
  (void)frame;
#endif
  layout = HID_MAPPING_LAYOUT_UNSPECIFIED;
  return false;
}

bool suppressUnavailableGenericPadGlyph(const PadButton& button,
                                        const controller_state_t* frame) {
  if (!isCapabilityFilteredGenericPad(frame)) {
    return false;
  }
  if (button.mask == GPAD_HOME && !frame->HAS_BTN_HOME) {
    return true;
  }
  const bool octaPro =
      std::strncmp(frame->controller_type_name, "OctaPro", 7) == 0;
  // OCTA exposes L3 both as the left-stick click and a rear shoulder, and R3
  // only as a rear shoulder. Do not suppress those physical glyphs based on
  // the generic analog-stick capability heuristic.
  if (!octaPro && button.mask == GPAD_L3 &&
      !frame->HAS_ANALOG_STICK_MAIN) {
    return true;
  }
  if (!octaPro && button.mask == GPAD_R3 &&
      !frame->HAS_ANALOG_STICK_AUX) {
    return true;
  }

  uint8_t layout = HID_MAPPING_LAYOUT_UNSPECIFIED;
  if (!classifiedGenericLayoutForFrame(frame, layout)) {
    return false;
  }
  const uint8_t style = hidMappingControllerStyle(layout);
  if (style == HID_MAPPING_LAYOUT_TWO_FACE &&
      (button.mask == GPAD_B || button.mask == GPAD_Y)) {
    return true;
  }
  if (style == HID_MAPPING_LAYOUT_THREE_FACE && button.mask == GPAD_Y) {
    return true;
  }
  if ((button.mask == GPAD_L1 || button.mask == GPAD_R1) &&
      !hidMappingLayoutHasShoulders(layout)) {
    return true;
  }
  if ((button.mask == GPAD_L2 || button.mask == GPAD_R2) &&
      hidMappingLayoutTriggerType(layout) == 0) {
    return true;
  }
  return (button.mask == GPAD_EXTRA2 || button.mask == GPAD_EXTRA3) &&
         !hidMappingLayoutHasGrips(layout);
}

bool isAnalogTriggerMask(uint32_t mask) {
  return mask == GPAD_L2 || mask == GPAD_R2;
}

bool suppressAnalogTriggerGaugeForFrame(const controller_state_t* frame) {
  // Every OCTA Pro personality exposes digital shoulder switches. Its PC
  // XInput report still carries conventional trigger-axis fields, but drawing
  // those 0/255 compatibility values as analog gauges misrepresents the pad.
  return frame != nullptr &&
         frame->controller_type_name[0] == 'O' &&
         frame->controller_type_name[4] == 'P';
}

uint32_t rawInputMaskForTurboButton(TurboButton btn) {
  switch (btn) {
    case TURBO_BTN_0: return INPUT_A;
    case TURBO_BTN_1: return INPUT_B;
    case TURBO_BTN_2: return INPUT_X;
    case TURBO_BTN_3: return INPUT_Y;
    case TURBO_BTN_4: return INPUT_L1;
    case TURBO_BTN_5: return INPUT_R1;
    case TURBO_BTN_6: return INPUT_L2;
    case TURBO_BTN_7: return INPUT_R2;
    case TURBO_BTN_8: return INPUT_START;
    case TURBO_BTN_9: return INPUT_SELECT;
    case TURBO_BTN_10: return INPUT_L3;
    case TURBO_BTN_11: return INPUT_R3;
    case TURBO_BTN_12: return INPUT_EXTRA1;
    case TURBO_BTN_13: return INPUT_EXTRA4;
    case TURBO_BTN_14: return INPUT_EXTRA2;
    case TURBO_BTN_15: return INPUT_EXTRA0;
    case TURBO_BTN_16: return INPUT_EXTRA3;
    default:          return 0;
  }
}

uint32_t gpadMaskForTurboButton(TurboButton btn) {
  switch (btn) {
    case TURBO_BTN_0: return GPAD_A;
    case TURBO_BTN_1: return GPAD_B;
    case TURBO_BTN_2: return GPAD_X;
    case TURBO_BTN_3: return GPAD_Y;
    case TURBO_BTN_4: return GPAD_L1;
    case TURBO_BTN_5: return GPAD_R1;
    case TURBO_BTN_6: return GPAD_L2;
    case TURBO_BTN_7: return GPAD_R2;
    case TURBO_BTN_8: return GPAD_START;
    case TURBO_BTN_9: return GPAD_SELECT;
    case TURBO_BTN_10: return GPAD_L3;
    case TURBO_BTN_11: return GPAD_R3;
    case TURBO_BTN_12: return GPAD_EXTRA1;
    case TURBO_BTN_13: return GPAD_EXTRA4;
    case TURBO_BTN_14: return GPAD_EXTRA2;
    case TURBO_BTN_15: return GPAD_EXTRA0;
    case TURBO_BTN_16: return GPAD_EXTRA3;
    default:          return 0;
  }
}

uint16_t turboVisualIntervalMs(TurboRate rate) {
  switch (getTurboBaseRate(rate)) {
    case TURBO_SLOW:   return kTurboVisualSlowMs;
    case TURBO_MEDIUM: return kTurboVisualMediumMs;
    case TURBO_FAST:   return kTurboVisualFastMs;
    case TURBO_MAX:    return kTurboVisualMaxMs;
    case TURBO_ULTRA:  return kTurboVisualMaxMs;
    default:           return 0;
  }
}

bool turboVisualPressed(TurboRate rate) {
  const uint16_t interval = turboVisualIntervalMs(rate);
  if (interval == 0) {
    return true;
  }
  return ((millis() / interval) & 1u) == 0u;
}

uint32_t applyTurboVisualState(uint8_t device, uint32_t state) {
  if (!turbo.hasAnyEnabled() || device >= MAX_USB_OUT) {
    return state;
  }

  const uint32_t held = turbo.getRawHeldButtons(device);
  if (held == 0) {
    return state;
  }

  for (uint8_t i = 0; i < TURBO_BTN_COUNT; ++i) {
    const TurboRate rate = turbo.getButtonRate((TurboButton)i);
    if (rate == TURBO_OFF) {
      continue;
    }

    const uint32_t rawMask = rawInputMaskForTurboButton((TurboButton)i);
    const uint32_t gpadMask = gpadMaskForTurboButton((TurboButton)i);
    if (rawMask == 0 || gpadMask == 0 || (held & rawMask) == 0) {
      continue;
    }

    if (turboVisualPressed(rate)) {
      state |= gpadMask;
    } else {
      state &= ~gpadMask;
    }
  }
  return state;
}

bool shouldDrawAnalogTriggerGauge(const controller_state_t* frame,
                                  uint32_t mask) {
  return frame != nullptr &&
         frame->connected &&
         frame->HAS_ANALOG_TRIGGERS &&
         trigger_mode != TRIGGER_MODE_DIGITAL &&
         !suppressAnalogTriggerGaugeForFrame(frame) &&
         isAnalogTriggerMask(mask);
}

bool isAnalogTriggerGaugeButton(const PadButton& button) {
  return isAnalogTriggerMask(button.mask) &&
         button.on == PAD_SHOULDER_ON &&
         button.off == PAD_SHOULDER_OFF;
}

bool hasSeparateDigitalAnalogTriggerButton(const PadButton& button,
                                           const PadButton* layout,
                                           uint8_t layoutCount) {
  if (!isAnalogTriggerGaugeButton(button) || layout == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < layoutCount; ++i) {
    const PadButton& candidate = layout[i];
    if (&candidate != &button &&
        candidate.mask == button.mask &&
        !isAnalogTriggerGaugeButton(candidate)) {
      return true;
    }
  }
  return false;
}

uint8_t analogTriggerValue(const controller_state_t* frame,
                           uint32_t mask) {
  if (!shouldDrawAnalogTriggerGauge(frame, mask)) {
    return 0;
  }
  return (mask == GPAD_L2) ? frame->ANALOG_L2 : frame->ANALOG_R2;
}

uint8_t analogTriggerGaugeInnerWidth(uint8_t gaugeWidth) {
  return (gaugeWidth > 2) ? (gaugeWidth - 2) : 1;
}

uint8_t analogTriggerGaugeColumns(const controller_state_t* frame,
                                  uint32_t mask,
                                  uint8_t gaugeWidth) {
  const uint8_t value = analogTriggerValue(frame, mask);
  if (value == 0) {
    return 0;
  }

  const uint8_t innerWidth = analogTriggerGaugeInnerWidth(gaugeWidth);
  if (value == 255) {
    return innerWidth;
  }
  uint8_t columns = ((uint16_t)value * innerWidth) / 256;
  return columns == 0 ? 1 : columns;
}

uint8_t analogTriggerGaugeWidthForButton(const PadButton* layout,
                                         uint8_t layoutCount,
                                         const PadButton& button) {
  uint8_t availableWidth = kAnalogTriggerGaugeMaxWidth;

  if (button.mask == GPAD_L2) {
    uint8_t nearestCol = 255;
    for (uint8_t i = 0; i < layoutCount; ++i) {
      if (layout[i].row == button.row && layout[i].col > button.col &&
          layout[i].col < nearestCol) {
        nearestCol = layout[i].col;
      }
    }
    if (nearestCol != 255) {
      availableWidth = nearestCol - button.col;
    }
  } else if (button.mask == GPAD_R2) {
    int16_t nearestEnd = -1;
    for (uint8_t i = 0; i < layoutCount; ++i) {
      if (layout[i].row == button.row && layout[i].col < button.col) {
        const int16_t candidateEnd = (int16_t)layout[i].col + kPadGlyphWidth;
        if (candidateEnd > nearestEnd) {
          nearestEnd = candidateEnd;
        }
      }
    }
    const int16_t buttonEnd = (int16_t)button.col + kPadGlyphWidth;
    if (nearestEnd >= 0 && nearestEnd < buttonEnd) {
      availableWidth = (uint8_t)(buttonEnd - nearestEnd);
    }
  }

  if (availableWidth > kAnalogTriggerGaugeMaxWidth) {
    availableWidth = kAnalogTriggerGaugeMaxWidth;
  }
#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
  // Leave room for the larger Z control between the two trigger meters.
  if (isGameCubePadLayout(layout) && availableWidth>14) availableWidth=14;
#endif
  if (availableWidth < kAnalogTriggerGaugeMinWidth) {
    availableWidth = kAnalogTriggerGaugeMinWidth;
  }
  return availableWidth;
}

bool isNeGconPressureGauge(const PadButton* layout, uint32_t mask) {
#ifdef ENABLE_INPUT_PSX
  return layout == menu_pad_layouts_internal::padLayoutPSXNeGcon &&
         isAnalogTriggerMask(mask);
#else
  // Do not retain the native PSX layout/renderer on USB-only products.
  (void)layout;
  (void)mask;
  return false;
#endif
}

uint8_t neGconPressureGaugeRows(const controller_state_t* frame, uint32_t mask) {
  if (frame == nullptr || !frame->connected ||
      !frame->HAS_ANALOG_MAIN_BUTTONS || !isAnalogTriggerMask(mask)) {
    return 0;
  }
  // Physical II / I pressure is always 0 released, 255 pressed. The USB
  // trigger fields can be inverted or disabled by the selected output mode.
  const uint8_t value = mask == GPAD_L2 ? frame->ANALOG_X : frame->ANALOG_A;
  return ((uint16_t)value * (kNeGconPressureGaugeHeight - 2) + 254) / 255;
}

uint8_t padTriggerGaugeFill(const controller_state_t* frame,
                           const PadButton* layout,
                           uint32_t mask,
                           uint8_t gaugeWidth) {
#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
  if (isNeGconPressureGauge(layout,mask) && oled_pixel::homeSurface() && oled_pixel::homeSurface()->active) {
    if (!frame || !frame->connected || !frame->HAS_ANALOG_MAIN_BUTTONS) return 0;
    const uint8_t pressure=mask==GPAD_L2?frame->ANALOG_X:frame->ANALOG_A;
    return (uint16_t(pressure)*20+254)/255;
  }
#endif
  return isNeGconPressureGauge(layout, mask)
    ? neGconPressureGaugeRows(frame, mask)
    : analogTriggerGaugeColumns(frame, mask, gaugeWidth);
}

uint8_t analogTriggerGaugeWidthForMask(const PadButton* layout,
                                       uint8_t layoutCount,
                                       uint32_t mask) {
  if (layout == nullptr) {
    return kAnalogTriggerGaugeMinWidth;
  }
  for (uint8_t i = 0; i < layoutCount; ++i) {
    if (layout[i].mask == mask && isAnalogTriggerGaugeButton(layout[i])) {
      return analogTriggerGaugeWidthForButton(layout, layoutCount, layout[i]);
    }
  }
  return kAnalogTriggerGaugeMinWidth;
}

bool renderPsxTriggerRawDebugLine(const controller_state_t* frame, bool force) {
#ifndef USE_I2C_DISPLAY
  (void)frame;
  (void)force;
  return false;
#else
#if defined(ENABLE_INPUT_PSX) && defined(ENABLE_PSX_TRIGGER_RAW_OLED_DEBUG)
  static bool lastVisible = false;
  static uint8_t lastL2 = 0xFF;
  static uint8_t lastR2 = 0xFF;

  const bool visible =
    deviceMode == RZORD_PSX &&
    frame != nullptr &&
    frame->connected &&
    frame->HAS_ANALOG_TRIGGERS;
  if (!visible) {
    if (lastVisible || force) {
      clearRow(7);
    }
    lastVisible = false;
    lastL2 = 0xFF;
    lastR2 = 0xFF;
    return false;
  }

  if (!force && lastVisible &&
      frame->ANALOG_L2 == lastL2 &&
      frame->ANALOG_R2 == lastR2) {
    return false;
  }

  char line[22];
  std::snprintf(line, sizeof(line), "RAW L2:%03u R2:%03u",
                (unsigned)frame->ANALOG_L2,
                (unsigned)frame->ANALOG_R2);
  clearRow(7);
  display.setFont(System5x7);
  display.setRow(7);
  display.setCol(0);
  display.print(line);

  lastVisible = true;
  lastL2 = frame->ANALOG_L2;
  lastR2 = frame->ANALOG_R2;
  return true;
#else
  (void)frame;
  (void)force;
  return false;
#endif
#endif
}

bool isWidePadLeftGlyph(char glyph) {
  const uint8_t value = static_cast<uint8_t>(glyph);
  return value == PAD_WIDE_CIRCLE_LEFT_ON ||
         value == PAD_WIDE_CIRCLE_LEFT_OFF ||
         value == PAD_WIDE_RECT_LEFT_ON ||
         value == PAD_WIDE_RECT_LEFT_OFF ||
         value == PAD_WIDE_TRIANGLE_LEFT_ON ||
         value == PAD_WIDE_TRIANGLE_LEFT_OFF;
}

char widePadRightGlyph(char leftGlyph, bool pressed) {
  const uint8_t value = static_cast<uint8_t>(leftGlyph);
  if (value == PAD_WIDE_RECT_LEFT_ON ||
      value == PAD_WIDE_RECT_LEFT_OFF) {
    return static_cast<char>(pressed
      ? PAD_WIDE_RECT_RIGHT_ON
      : PAD_WIDE_RECT_RIGHT_OFF);
  }
  if (value == PAD_WIDE_TRIANGLE_LEFT_ON ||
      value == PAD_WIDE_TRIANGLE_LEFT_OFF) {
    return static_cast<char>(pressed
      ? PAD_WIDE_TRIANGLE_RIGHT_ON
      : PAD_WIDE_TRIANGLE_RIGHT_OFF);
  }
  return static_cast<char>(pressed
    ? PAD_WIDE_CIRCLE_RIGHT_ON
    : PAD_WIDE_CIRCLE_RIGHT_OFF);
}

char displayLabelChar(outputMode_t outputMode, uint32_t mask, bool pressed) {
  return getVirtualOutputButtonGlyph(outputMode, mask, pressed);
}

void drawAnalogTriggerGauge(const PadButton& button,
                            const controller_state_t* frame,
                            uint8_t colOffset,
                            uint8_t rowOffset,
                            uint8_t gaugeWidth) {
  const uint8_t filledColumns =
    analogTriggerGaugeColumns(frame, button.mask, gaugeWidth);

  int16_t startCol = (int16_t)button.col + colOffset;
  if (button.mask == GPAD_R2) {
    startCol += (int16_t)kPadGlyphWidth - gaugeWidth;
  }
  if (startCol < 0) {
    startCol = 0;
  }
  if (startCol > (int16_t)(128 - gaugeWidth)) {
    startCol = 128 - gaugeWidth;
  }

  display.setCursor((uint8_t)startCol, button.row + rowOffset);
  const uint8_t innerWidth = analogTriggerGaugeInnerWidth(gaugeWidth);
  for (uint8_t i = 0; i < gaugeWidth; ++i) {
    uint8_t column = kAnalogTriggerGaugeEmpty;
    if (i == 0 || i == gaugeWidth - 1) {
      column = kAnalogTriggerGaugeFilled;
    } else {
      const uint8_t interiorIndex = i - 1;
      const bool fillFromRight = button.mask == GPAD_L2;
      const bool filled =
        fillFromRight
          ? ((innerWidth - 1 - interiorIndex) < filledColumns)
          : (interiorIndex < filledColumns);
      if (filled) {
        column = kAnalogTriggerGaugeFilled;
      }
    }
    display.ssd1306WriteRamBuf(column);
  }
}

void drawNeGconPressureGauge(const PadButton& button,
                            const controller_state_t* frame,
                            uint8_t colOffset,
                            uint8_t rowOffset) {
  const uint8_t filledRows = neGconPressureGaugeRows(frame, button.mask);
  for (uint8_t page = 0; page < kNeGconPressureGaugePages; ++page) {
    display.setCursor(button.col + colOffset, button.row + rowOffset + page);
    for (uint8_t x = 0; x < kNeGconPressureGaugeWidth; ++x) {
      uint8_t column = 0;
      // Five visible pixels plus the same blank column used by pad glyphs.
      if (x < kNeGconPressureGaugeWidth - 1) {
        for (uint8_t bit = 0; bit < 8; ++bit) {
          const uint8_t y = page * 8 + bit;
          const bool border = x == 0 || x == kNeGconPressureGaugeWidth - 2 ||
                              y == 0 || y == kNeGconPressureGaugeHeight - 1;
          const bool filled = y >= kNeGconPressureGaugeHeight - 1 - filledRows;
          if (y < kNeGconPressureGaugeHeight && (border || filled)) {
            column |= (1u << bit);
          }
        }
      }
      display.ssd1306WriteRamBuf(column);
    }
  }
}

#ifdef ENABLE_INPUT_PSX
void drawJogconDial(uint8_t position, uint8_t colOffset, uint8_t rowOffset) {
  // The complete 32px JogCon panel includes top controls, dial and D-pad.
  oled_pixel::Origin origin(oled_pixel::centerOffset(24, 55));
  for (uint8_t page = 0; page < jogcon_dial::kPages; ++page) {
    display.setCursor(colOffset + jogcon_dial::kCol, rowOffset + jogcon_dial::kRow + page);
    for (uint8_t x = 0; x < jogcon_dial::kSize; ++x) {
      uint8_t column = 0;
      for (uint8_t bit = 0; bit < 8; ++bit) {
        if (jogcon_dial::pixel(x, page * 8 + bit, position)) column |= (1u << bit);
      }
      display.ssd1306WriteRamBuf(column);
    }
  }
}
#endif

// A relative phase display: Driving's speed-scaled quadrature count wraps at
// 256. It is not a calibrated absolute angle or a physical center measurement.
#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
void drawRelativeRotaryDial(uint8_t phase, uint8_t colOffset, uint8_t rowOffset,
                            uint8_t xOffset=19, uint8_t yOffset=0,
                            int8_t panelOffset=oled_pixel::centerOffset(24,58)) {
  oled_pixel::Origin origin(panelOffset+yOffset);
  for (uint8_t page=0;page<jogcon_dial::kPages;++page) {
    display.setCursor(colOffset+xOffset,rowOffset+page);
    for (uint8_t x=0;x<jogcon_dial::kSize;++x) {
      uint8_t bits=0;
      for (uint8_t bit=0;bit<8;++bit)
        if (jogcon_dial::pixel(x,page*8+bit,phase)) bits|=1u<<bit;
      display.ssd1306WriteRamBuf(bits);
    }
  }
}
void drawDrivingDial(const controller_state_t* frame, uint8_t colOffset, uint8_t rowOffset) {
  drawRelativeRotaryDial(frame?frame->paddle:128,colOffset,rowOffset);
}
void drawJaguarRotaryDial(uint8_t phase, uint8_t colOffset, uint8_t rowOffset) {
  drawRelativeRotaryDial(phase,colOffset,rowOffset,5,8,oled_pixel::centerOffset(24,55));
}
#ifdef ENABLE_INPUT_PSX
// beatmania turntable: the JogCon dial, turned by psxTurntableDisplayPhase
// (Up clockwise, Down counter-clockwise). Its hub fills while it turns.
void drawBeatmaniaTurntable(uint8_t phase, bool turning, uint8_t colOffset, uint8_t rowOffset) {
  using namespace classic_pad_art;
  oled_pixel::Origin origin(oled_pixel::centerOffset(24,kBeatmaniaPanelLast)+kBeatmaniaDialY);
  for (uint8_t page=0;page<jogcon_dial::kPages;++page) {
    display.setCursor(colOffset+kBeatmaniaDialX,rowOffset+page);
    for (uint8_t x=0;x<jogcon_dial::kSize;++x) {
      uint8_t bits=0;
      for (uint8_t bit=0;bit<8;++bit) {
        const uint8_t y=page*8+bit;
        const int dx=int(x)-11, dy=int(y)-11;
        if (jogcon_dial::pixel(x,y,phase) || (turning && dx*dx+dy*dy<=4)) bits|=1u<<bit;
      }
      display.ssd1306WriteRamBuf(bits);
    }
  }
}
#endif
#endif

uint8_t wheelPanelClearEnd(uint8_t colOffset) {
  const uint16_t end =
    (uint16_t)colOffset + kDreamcastWheelPanelWidth + kPadGlyphWidth - 1;
  return end > 127 ? 127 : (uint8_t)end;
}

void clearWheelPanelRow(uint8_t colOffset, uint8_t row) {
  display.clear(colOffset, wheelPanelClearEnd(colOffset), row, row);
}

bool isDreamcastWheelFrame(const controller_state_t* frame) {
  if (frame == nullptr || !frame->connected) {
    return false;
  }
  return std::strncmp(frame->controller_type_name, "Whl", 3) == 0 ||
         std::strcmp(frame->controller_type_name, "Wheel") == 0;
}

bool isSaturnWheelFrame(const controller_state_t* frame) {
  return frame != nullptr &&
         frame->connected &&
         std::strcmp(frame->controller_type_name, "Wheel") == 0;
}

void drawDreamcastWheelLinearGauge(uint8_t col,
                                   uint8_t row,
                                   uint8_t gaugeWidth,
                                   uint8_t filledColumns,
                                   bool fillFromRight) {
  display.setCursor(col, row);
  const uint8_t innerWidth = analogTriggerGaugeInnerWidth(gaugeWidth);
  if (filledColumns > innerWidth) {
    filledColumns = innerWidth;
  }
  for (uint8_t i = 0; i < gaugeWidth; ++i) {
    uint8_t column = kAnalogTriggerGaugeEmpty;
    if (i == 0 || i == gaugeWidth - 1) {
      column = kAnalogTriggerGaugeFilled;
    } else {
      const uint8_t interiorIndex = i - 1;
      const bool filled =
        fillFromRight
          ? ((innerWidth - 1 - interiorIndex) < filledColumns)
          : (interiorIndex < filledColumns);
      if (filled) {
        column = kAnalogTriggerGaugeFilled;
      }
    }
    display.ssd1306WriteRamBuf(column);
  }
}

void drawCenterOutSignedAxisGauge(int16_t delta,
                                  uint8_t col,
                                  uint8_t row,
                                  uint8_t panelWidth) {
  const uint16_t absDelta = delta < 0 ? (uint16_t)(-delta) : (uint16_t)delta;
  const uint16_t boundedDelta = absDelta > 128 ? 128 : absDelta;
  const uint8_t centerLeftColumn = (panelWidth - 1) / 2;
  const uint8_t centerRightColumn = panelWidth / 2;
  const uint8_t halfSpan = centerLeftColumn > 0 ? (uint8_t)(centerLeftColumn - 1) : 0;
  uint8_t magnitude = (uint8_t)((boundedDelta * (uint16_t)halfSpan + 63) / 128);
  if (magnitude > halfSpan) {
    magnitude = halfSpan;
  }

  display.setCursor(col, row);
  for (uint8_t i = 0; i < panelWidth; ++i) {
    uint8_t column = kAnalogTriggerGaugeEmpty;
    if (i == 0 || i == panelWidth - 1 || i == centerLeftColumn || i == centerRightColumn) {
      column = kAnalogTriggerGaugeFilled;
    } else if (delta < 0 && i < centerLeftColumn && i >= (centerLeftColumn - magnitude)) {
      column = kAnalogTriggerGaugeFilled;
    } else if (delta > 0 && i > centerRightColumn && i <= (centerRightColumn + magnitude)) {
      column = kAnalogTriggerGaugeFilled;
    }
    display.ssd1306WriteRamBuf(column);
  }
}

void drawCenterOutPaddleGauge(const controller_state_t* frame,
                              uint8_t col,
                              uint8_t row,
                              uint8_t panelWidth) {
  const uint8_t raw = frame != nullptr ? frame->paddle : 128;
  drawCenterOutSignedAxisGauge((int16_t)raw - 128,
                               col,
                               row,
                               panelWidth);
}

#ifdef ENABLE_INPUT_PSX
bool isNeGconFrame(const controller_state_t* frame) {
  return frame != nullptr &&
         frame->connected &&
         std::strcmp(frame->controller_type_name, "neGcon") == 0;
}

void drawNeGconTwistGauge(const controller_state_t* frame,
                          uint8_t colOffset) {
  home_pad_geometry::Origin origin(menu_pad_layouts_internal::padLayoutPSXNeGcon,
      menu_pad_layouts_internal::PAD_LAYOUT_PSX_NEGCON_COUNT, ReflexPad5x7);
  const uint8_t endCol = (uint8_t)(colOffset + kNeGconTwistGaugeWidth - 1);
  display.clear(colOffset, endCol, kNeGconTwistGaugeRow, kNeGconTwistGaugeRow);
  drawCenterOutSignedAxisGauge(frame != nullptr ? frame->LX : 0,
                               colOffset,
                               kNeGconTwistGaugeRow,
                               kNeGconTwistGaugeWidth);
}
#endif

void drawWheelAxisGauge(const controller_state_t* frame,
                        uint8_t colOffset,
                        uint8_t rowOffset,
                        uint8_t panelWidth) {
  oled_pixel::Origin origin(oled_pixel::centerOffset(33, 46), 25, 63, true);
  const uint8_t row = rowOffset + kDreamcastWheelAxisRow;
  clearWheelPanelRow(colOffset, row);
  drawCenterOutPaddleGauge(frame, colOffset, row, panelWidth);
}

void drawDreamcastWheelAxisGauge(const controller_state_t* frame,
                                 uint8_t colOffset,
                                 uint8_t rowOffset) {
  drawWheelAxisGauge(frame, colOffset, rowOffset, kDreamcastWheelPanelWidth);
}

void drawSaturnWheelAxisGauge(const controller_state_t* frame,
                              uint8_t colOffset,
                              uint8_t rowOffset) {
  drawWheelAxisGauge(frame, colOffset, rowOffset, kSaturnWheelPanelWidth);
}

void printDreamcastWheelButtonLabel(const char* label, bool pressed) {
  display.setInvertMode(pressed);
  display.print(label);
  display.setInvertMode(false);
}

void drawWheelFaceButton(uint8_t col, uint8_t row, bool pressed, char label, bool useLabels) {
  display.setCursor(col, row);
  if (useLabels) {
    char text[2] = { label, '\0' };
    display.setFont(System5x7);
    printDreamcastWheelButtonLabel(text, pressed);
    return;
  }
  display.setFont(ReflexPad5x7);
  display.print((char)(pressed ? PAD_FACE_ON : PAD_FACE_OFF));
  display.setFont(System5x7);
}

void drawDreamcastWheelStartGlyph(uint8_t col, uint8_t row, bool pressed) {
  display.setFont(ReflexPad5x7);
  display.setCursor(col, row);
  display.print((char)(pressed ? PAD_WIDE_TRIANGLE_LEFT_ON : PAD_WIDE_TRIANGLE_LEFT_OFF));
  display.setCursor(col + kWideCircleRightHalfOffset, row);
  display.print((char)(pressed ? PAD_WIDE_TRIANGLE_RIGHT_ON : PAD_WIDE_TRIANGLE_RIGHT_OFF));
  display.setFont(System5x7);
}

void drawSaturnWheelStartGlyph(uint8_t col, uint8_t row, bool pressed) {
  display.setFont(ReflexPad5x7);
  display.setCursor(col, row);
  display.print((char)(pressed ? PAD_SATURN_START_LEFT_ON : PAD_SATURN_START_LEFT_OFF));
  display.setCursor(col + kWideCircleRightHalfOffset, row);
  display.print((char)(pressed ? PAD_SATURN_START_RIGHT_ON : PAD_SATURN_START_RIGHT_OFF));
  display.setFont(System5x7);
}

void drawDreamcastWheelButtonRow(uint32_t state,
                                 uint8_t colOffset,
                                 uint8_t rowOffset,
                                 bool useLabels) {
  const uint8_t row = rowOffset + kDreamcastWheelButtonsRow;
  clearWheelPanelRow(colOffset, row);
  drawWheelFaceButton(colOffset, row, (state & GPAD_DOWN) != 0, '-', useLabels);
  drawWheelFaceButton(colOffset + 12, row, (state & GPAD_B) != 0, 'B', useLabels);
  drawDreamcastWheelStartGlyph(colOffset + 24, row, (state & GPAD_START) != 0);
  drawWheelFaceButton(colOffset + 42, row, (state & GPAD_A) != 0, 'A', useLabels);
  drawWheelFaceButton(colOffset + kDreamcastWheelButtonPlusCol, row,
                      (state & GPAD_UP) != 0, '+', useLabels);
}

void drawSaturnWheelPaddleGauges(uint32_t state,
                                 uint8_t colOffset,
                                 uint8_t rowOffset) {
  const uint8_t row = rowOffset;
  clearWheelPanelRow(colOffset, row);
  const uint8_t fullColumns =
    analogTriggerGaugeInnerWidth(kSaturnWheelTriggerGaugeWidth);
  const uint8_t leftColumns = (state & GPAD_UP) != 0 ? fullColumns : 0;
  const uint8_t rightColumns = (state & GPAD_DOWN) != 0 ? fullColumns : 0;
  drawDreamcastWheelLinearGauge(colOffset,
                                row,
                                kSaturnWheelTriggerGaugeWidth,
                                leftColumns,
                                true);
  drawDreamcastWheelLinearGauge(colOffset + kSaturnWheelRightTriggerCol,
                                row,
                                kSaturnWheelTriggerGaugeWidth,
                                rightColumns,
                                false);
}

void drawSaturnWheelButtonRow(uint32_t state,
                              uint8_t colOffset,
                              uint8_t rowOffset,
                              bool useLabels) {
  const uint8_t row = rowOffset + kSaturnWheelButtonsRow;
  clearWheelPanelRow(colOffset, row);
  drawWheelFaceButton(colOffset, row, (state & GPAD_L1) != 0, 'Z', useLabels);
  drawWheelFaceButton(colOffset + 8, row, (state & GPAD_Y) != 0, 'Y', useLabels);
  drawWheelFaceButton(colOffset + 16, row, (state & GPAD_X) != 0, 'X', useLabels);
  drawSaturnWheelStartGlyph(colOffset + 26, row, (state & GPAD_START) != 0);
  drawWheelFaceButton(colOffset + 40, row, (state & GPAD_A) != 0, 'A', useLabels);
  drawWheelFaceButton(colOffset + 48, row, (state & GPAD_B) != 0, 'B', useLabels);
  drawWheelFaceButton(colOffset + 56, row, (state & GPAD_R1) != 0, 'C', useLabels);
}

void drawDreamcastWheelHome(const controller_state_t* frame,
                            uint32_t state,
                            uint8_t colOffset,
                            uint8_t rowOffset,
                            bool redrawTriggers,
                            bool redrawButtons,
                            bool redrawAxis,
                            bool useLabels,
                            bool fullWidth = false) {
#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
  if (redrawTriggers || redrawButtons || redrawAxis)
    classic_wheel_art::drawDreamcast(display,frame?frame->paddle:128,state,
        analogTriggerGaugeColumns(frame,GPAD_L2,fullWidth?29:25),
        analogTriggerGaugeColumns(frame,GPAD_R2,fullWidth?29:25),
        colOffset,rowOffset,useLabels,System5x7,fullWidth);
#else
  (void)fullWidth;
  oled_pixel::Origin origin(oled_pixel::centerOffset(25, 46));
  if (redrawTriggers) {
    clearWheelPanelRow(colOffset, rowOffset);
    const uint8_t leftColumns =
      analogTriggerGaugeColumns(frame, GPAD_L2, kDreamcastWheelTriggerGaugeWidth);
    const uint8_t rightColumns =
      analogTriggerGaugeColumns(frame, GPAD_R2, kDreamcastWheelTriggerGaugeWidth);
    drawDreamcastWheelLinearGauge(colOffset,
                                  rowOffset,
                                  kDreamcastWheelTriggerGaugeWidth,
                                  leftColumns,
                                  true);
    drawDreamcastWheelLinearGauge(colOffset + kDreamcastWheelRightTriggerCol,
                                  rowOffset,
                                  kDreamcastWheelTriggerGaugeWidth,
                                  rightColumns,
                                  false);
  }
  if (redrawAxis) {
    drawDreamcastWheelAxisGauge(frame, colOffset, rowOffset);
  }
  if (redrawButtons) {
    drawDreamcastWheelButtonRow(state, colOffset, rowOffset, useLabels);
  }
#endif
}

void drawSaturnWheelHome(const controller_state_t* frame,
                         uint32_t state,
                         uint8_t colOffset,
                         uint8_t rowOffset,
                         bool redrawPaddles,
                         bool redrawButtons,
                         bool redrawAxis,
                         bool useLabels,
                         bool fullWidth = false) {
#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
  classic_wheel_art::draw(display,frame?frame->paddle:128,state,colOffset,rowOffset,
                         redrawPaddles,redrawButtons,redrawAxis,useLabels,System5x7,fullWidth);
#else
  (void)fullWidth;
  oled_pixel::Origin origin(oled_pixel::centerOffset(25, 46));
  clearWheelPanelRow(colOffset, rowOffset + 3);
  if (redrawPaddles) {
    drawSaturnWheelPaddleGauges(state, colOffset, rowOffset);
  }
  if (redrawAxis) {
    drawSaturnWheelAxisGauge(frame, colOffset, rowOffset);
  }
  if (redrawButtons) {
    drawSaturnWheelButtonRow(state, colOffset, rowOffset, useLabels);
  }
#endif
}

bool findJoystickCenter(const PadButton* layout,
                        uint8_t layoutCount,
                        uint8_t* centerRow,
                        uint8_t* centerCol) {
  if (layout == nullptr || centerRow == nullptr || centerCol == nullptr) {
    return false;
  }

  for (uint8_t i = 0; i < layoutCount; ++i) {
    if (layout[i].mask == GPAD_UP) {
      *centerRow = layout[i].row + 1;
      *centerCol = layout[i].col;
      return true;
    }
  }
  return false;
}

void drawJoystickBallForLayout(const PadButton* layout,
                               uint8_t layoutCount,
                               uint32_t state,
                               uint8_t colOffset,
                               uint8_t rowOffset) {
  home_pad_geometry::Origin origin(layout, layoutCount, ReflexPad5x7, true);
  uint8_t centerRow = 0;
  uint8_t centerCol = 0;
  if (!findJoystickCenter(layout, layoutCount, &centerRow, &centerCol)) {
    return;
  }

  int8_t x = 0;
  int8_t y = 0;
  const bool left = (state & GPAD_LEFT) != 0;
  const bool right = (state & GPAD_RIGHT) != 0;
  const bool up = (state & GPAD_UP) != 0;
  const bool down = (state & GPAD_DOWN) != 0;
  if (left != right) {
    x = left ? -1 : 1;
  }
  if (up != down) {
    y = up ? -1 : 1;
  }

  display.setFont(ReflexPad5x7);
  for (int8_t dy = -1; dy <= 1; ++dy) {
    for (int8_t dx = -1; dx <= 1; ++dx) {
      const int16_t col = (int16_t)centerCol + ((int16_t)dx * kPadGlyphWidth) + colOffset;
      const int16_t row = (int16_t)centerRow + dy + rowOffset;
      if (col < 0 || col > 127 || row < 0 || row > 7) {
        continue;
      }
      display.setCursor((uint8_t)col, (uint8_t)row);
      display.print(' ');
    }
  }

  const int16_t ballCol = (int16_t)centerCol + ((int16_t)x * kPadGlyphWidth) + colOffset;
  const int16_t ballRow = (int16_t)centerRow + y + rowOffset;
  if (ballCol >= 0 && ballCol <= 127 && ballRow >= 0 && ballRow <= 7) {
    display.setCursor((uint8_t)ballCol, (uint8_t)ballRow);
    display.print((char)PAD_FACE_ON);
  }
}

int16_t padButtonColumnForFrame(const PadButton& button,
                               const PadButton* layout,
                               const controller_state_t* frame) {
#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
  if (layout==menu_pad_layouts_internal::padLayoutSnes) {
    if (button.mask==GPAD_SELECT) return button.col-2;
    if (button.mask==GPAD_START) return button.col+2;
  }
  if (home_pad_geometry::dpad(button))
    return button.col+classic_pad_art::directionShift(layout);
#endif
#ifdef ENABLE_INPUT_PSX
  if (layout == menu_pad_layouts_internal::padLayoutPSXDualShock &&
      frame != nullptr && !frame->HAS_ANALOG_TRIGGERS) {
    if (button.mask == GPAD_L1) return button.col - kPadGlyphWidth;
    if (button.mask == GPAD_R1) return button.col + kPadGlyphWidth;
  }
#else
  (void)layout;
  (void)frame;
#endif
  return button.col;
}

void drawPadSlotGlyph(const PadButton& button,
                      const PadButton* layout,
                      uint8_t layoutCount,
                      uint32_t state,
                      uint8_t colOffset,
                      uint8_t rowOffset,
                      bool useJoystickDirections,
                      bool useButtonLabels,
                      outputMode_t labelOutputMode,
                      const controller_state_t* analogFrame) {
  home_pad_geometry::Origin origin(layout, layoutCount, ReflexPad5x7,
                                   home_pad_geometry::dpad(button));
  if (suppressUnavailableGenericPadGlyph(button, analogFrame)) {
    return;
  }
  if (useJoystickDirections && isJoystickDirectionMask(button.mask)) {
    return;
  }

#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
  colOffset=classic_pad_art::column(layout,colOffset);
  oled_button_art::Button art;
  if (oled_pixel::homeSurface() && oled_pixel::homeSurface()->active &&
      classic_pad_art::get(layout,button,art)) {
    bool pressed=(state&button.mask)!=0;
    uint8_t fill=0;
#ifdef ENABLE_INPUT_PSX
    if (isNeGconPressureGauge(layout,button.mask)) {
      fill=padTriggerGaugeFill(analogFrame,layout,button.mask,art.w);
      pressed=false; // Physical pressure, not the USB digital threshold.
    }
#endif
    const uint8_t* label=nullptr;
    // Mouse clicks stay pictorial; controller letters obscure the animal's face.
    if (useButtonLabels && !mouse_art::selected(layout)) {
      uint32_t labelMask=button.mask;
      const uint8_t glyph=uint8_t(displayLabelChar(labelOutputMode,labelMask,false));
      if (glyph>=ReflexPad5x7[4] && glyph<ReflexPad5x7[4]+ReflexPad5x7[5])
        label=&ReflexPad5x7[6+(glyph-ReflexPad5x7[4])*5];
    }
    if (art.shape==oled_button_art::Shape::Glyph && !useButtonLabels) {
      const uint8_t glyph=uint8_t(pressed?button.on:button.off);
      label=&ReflexPad5x7[6+(glyph-ReflexPad5x7[4])*5];
      pressed=false; // Exact shared pressed/released artwork, not an inversion.
    }
    oled_button_art::draw(display,art,pressed,colOffset,
        home_pad_geometry::placement(layout,layoutCount,ReflexPad5x7).offset,rowOffset,fill,label);
    return;
  }
#endif

  if (isNeGconPressureGauge(layout, button.mask)) {
    drawNeGconPressureGauge(button, analogFrame, colOffset, rowOffset);
    return;
  }

  const bool analogTriggerGaugeActive =
    isAnalogTriggerGaugeButton(button) &&
    shouldDrawAnalogTriggerGauge(analogFrame, button.mask);
  if (analogTriggerGaugeActive) {
    const uint8_t gaugeWidth =
      analogTriggerGaugeWidthForButton(layout, layoutCount, button);
    drawAnalogTriggerGauge(button, analogFrame, colOffset, rowOffset, gaugeWidth);
    if (trigger_mode != TRIGGER_MODE_BOTH ||
        hasSeparateDigitalAnalogTriggerButton(button, layout, layoutCount)) {
      return;
    }
  }

  const bool pressed = (state & button.mask) != 0;
  char glyph = pressed ? button.on : button.off;
  if (useButtonLabels) {
    const char label = displayLabelChar(labelOutputMode, button.mask, pressed);
    if (label != '\0') {
      glyph = label;
    }
  }

  display.setFont(ReflexPad5x7);
  int16_t glyphCol = padButtonColumnForFrame(button, layout, analogFrame) + colOffset;
  int16_t glyphRow = (int16_t)button.row + rowOffset;
  if (isGameCubePadLayout(layout) &&
      (button.mask == GPAD_L2 || button.mask == GPAD_R2)) {
    // GameCube's physical trigger click sits outside its analog travel gauge.
    glyphCol = colOffset + (button.mask == GPAD_L2 ? 0 : 9 * kPadGlyphWidth);
  } else if (analogTriggerGaugeActive && trigger_mode == TRIGGER_MODE_BOTH) {
    glyphRow += 1;
    if (button.mask == GPAD_L2) {
      glyphCol += (int16_t)kPadGlyphWidth * kTriggerBothGlyphInsetChars;
    } else if (button.mask == GPAD_R2) {
      glyphCol -= (int16_t)kPadGlyphWidth * kTriggerBothGlyphInsetChars;
    }
  }
  if (glyphCol < 0) {
    glyphCol = 0;
  }
  if (glyphCol > 122) {
    glyphCol = 122;
  }
  if (glyphRow < 0) {
    glyphRow = 0;
  }
  if (glyphRow > 7) {
    glyphRow = 7;
  }

  display.setCursor((uint8_t)glyphCol, (uint8_t)glyphRow);
  display.print(glyph);
  if (isWidePadLeftGlyph(glyph)) {
    const uint8_t rightCol =
      (glyphCol + kWideCircleRightHalfOffset > 127)
        ? 127
        : (uint8_t)(glyphCol + kWideCircleRightHalfOffset);
    display.setCursor(rightCol, (uint8_t)glyphRow);
    display.print(widePadRightGlyph(glyph, pressed));
  }
}

void drawPadLayoutState(const PadButton* layout,
                        uint8_t layoutCount,
                        uint32_t state,
                        uint8_t colOffset,
                        uint8_t rowOffset,
                        bool useJoystickDirections,
                        bool useButtonLabels = false,
                        outputMode_t labelOutputMode = OUTPUT_MISTER,
                        const controller_state_t* analogFrame = nullptr) {
  if (layout == nullptr) {
    return;
  }

#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
  classic_pad_art::decoration(display,layout,colOffset,
      home_pad_geometry::placement(layout,layoutCount,ReflexPad5x7).offset,rowOffset);
#endif
  for (uint8_t i = 0; i < layoutCount; ++i) {
    drawPadSlotGlyph(layout[i],
                     layout,
                     layoutCount,
                     state,
                     colOffset,
                     rowOffset,
                     useJoystickDirections,
                     useButtonLabels,
                     labelOutputMode,
                     analogFrame);
  }

  if (useJoystickDirections) {
    drawJoystickBallForLayout(layout, layoutCount, state, colOffset, rowOffset);
  } else {
    drawDpadCentersForLayoutState(layout, layoutCount, state, colOffset, rowOffset);
  }
}

bool shouldUseArcadeJoystickForPrimaryPad() {
  return deviceMode == RZORD_JVS;
}


bool shouldUseCenteredOutputButtonLabels() {
  return menu_home_button_labels != 0;
}

bool shouldUseSideBySideOutputButtonLabels() {
  return menu_home_button_labels != 0;
}

bool shouldUseArcadeJoystickForSecondaryPad(bool showOutputVirtualPad,
                                            bool showPhysicalSecondPad,
                                            outputMode_t outputMode) {
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  if (deviceMode == RZORD_JVS) {
    return showPhysicalSecondPad;
  }
#endif

  if (showOutputVirtualPad) {
    if (deviceMode == RZORD_JVS) {
      return true;
    }
    #ifdef ENABLE_OUTPUT_JVS
    if (outputMode == OUTPUT_JVS) {
      return true;
    }
    #endif
  }
  return showPhysicalSecondPad && deviceMode == RZORD_JVS;
}

bool renderPhysicalOnlyInputHomeHint(bool forceRedraw, const char* name) {
  if (name == nullptr || name[0] == '\0') {
    return false;
  }

  static bool hintShown = false;
  static char lastPhysicalOnlyName[16] = {0};
  const bool physicalOnlyNameChanged =
    std::strncmp(lastPhysicalOnlyName, name, sizeof(lastPhysicalOnlyName)) != 0;

  if (!forceRedraw && hintShown && !physicalOnlyNameChanged) {
    return true;
  }

  for (uint8_t row = 3; row <= 6; ++row) {
    clearRow(row);
  }
#if defined(PRODUCT_CLASSIC2USB)
  clearRow(7);
#endif

  display.setFont(System5x7);
  const size_t rawNameWidth = std::strlen(name) * 6u;
  const uint8_t nameWidth =
    (rawNameWidth > 126u) ? 126u : (uint8_t)rawNameWidth;
  display.setRow(4);
  display.setCol((uint8_t)((128 - nameWidth) / 2));
  display.print(name);
  display.setRow(5);
  display.setCol(25);
  display.print("No controller");

  std::strncpy(lastPhysicalOnlyName, name, sizeof(lastPhysicalOnlyName) - 1);
  lastPhysicalOnlyName[sizeof(lastPhysicalOnlyName) - 1] = '\0';
  hintShown = true;
  return true;
}

#if defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT)
enum class XboxAuthOverlayState : uint8_t {
  Hidden = 0,
  Missing,
  Starting,
  Authenticating,
  Unlocked,
};

constexpr uint8_t kXboxAuthGraphicX = 40;
constexpr uint8_t kXboxAuthGraphicWidth = 48;
constexpr uint8_t kXboxAuthGraphicRow = 5;
constexpr uint8_t kXboxAuthGraphicHeight = 16;

void setXboxAuthGraphicPixel(uint8_t* pixels, uint8_t x, uint8_t y) {
  if (x >= kXboxAuthGraphicWidth || y >= kXboxAuthGraphicHeight) return;
  pixels[(y / 8) * kXboxAuthGraphicWidth + x] |=
      static_cast<uint8_t>(1u << (y & 0x07));
}

void drawXboxAuthGraphicLine(uint8_t* pixels,
                             uint8_t x0, uint8_t y0,
                             uint8_t x1, uint8_t y1) {
  if (x0 == x1) {
    if (y1 < y0) {
      const uint8_t swap = y0;
      y0 = y1;
      y1 = swap;
    }
    for (uint8_t y = y0; y <= y1; ++y) {
      setXboxAuthGraphicPixel(pixels, x0, y);
    }
    return;
  }
  if (x1 < x0) {
    const uint8_t swap = x0;
    x0 = x1;
    x1 = swap;
  }
  for (uint8_t x = x0; x <= x1; ++x) {
    setXboxAuthGraphicPixel(pixels, x, y0);
  }
}

void drawXboxAuthGraphicFrame(uint8_t* pixels,
                              uint8_t x, uint8_t y,
                              uint8_t width, uint8_t height) {
  drawXboxAuthGraphicLine(pixels, x, y, x + width - 1, y);
  drawXboxAuthGraphicLine(
      pixels, x, y + height - 1, x + width - 1, y + height - 1);
  drawXboxAuthGraphicLine(pixels, x, y, x, y + height - 1);
  drawXboxAuthGraphicLine(
      pixels, x + width - 1, y, x + width - 1, y + height - 1);
}

void drawXboxAuthLock(bool unlocked, uint8_t keyPhase) {
  uint8_t pixels[kXboxAuthGraphicWidth * 2] = {};
  constexpr uint8_t lockX = 29;
  drawXboxAuthGraphicFrame(pixels, lockX, 7, 16, 9);

  if (unlocked) {
    drawXboxAuthGraphicLine(pixels, lockX + 4, 7, lockX + 4, 2);
    drawXboxAuthGraphicLine(pixels, lockX + 4, 2, lockX + 11, 2);
    drawXboxAuthGraphicLine(pixels, lockX + 11, 2, lockX + 11, 4);
  } else {
    drawXboxAuthGraphicFrame(pixels, lockX + 4, 1, 8, 8);
    const uint8_t keyX = static_cast<uint8_t>(4 + (keyPhase & 0x03) * 5);
    setXboxAuthGraphicPixel(pixels, keyX, 9);
    setXboxAuthGraphicPixel(pixels, keyX - 1, 10);
    setXboxAuthGraphicPixel(pixels, keyX + 1, 10);
    setXboxAuthGraphicPixel(pixels, keyX, 11);
    drawXboxAuthGraphicLine(pixels, keyX + 2, 10, keyX + 10, 10);
    drawXboxAuthGraphicLine(pixels, keyX + 8, 10, keyX + 8, 13);
    drawXboxAuthGraphicLine(pixels, keyX + 5, 10, keyX + 5, 12);
  }

  for (uint8_t page = 0; page < 2; ++page) {
    display.setCursor(kXboxAuthGraphicX, kXboxAuthGraphicRow + page);
    for (uint8_t x = 0; x < kXboxAuthGraphicWidth; ++x) {
      display.ssd1306WriteRamBuf(pixels[page * kXboxAuthGraphicWidth + x]);
    }
  }
}

void drawXboxAuthDownArrow(uint8_t col, bool visible) {
  static const uint8_t kDownArrow[8] = {
      0x02, 0x06, 0x0E, 0x3E, 0x0E, 0x06, 0x02, 0x00,
  };
  display.setCursor(col, 7);
  for (uint8_t x = 0; x < 8; ++x) {
    display.ssd1306WriteRamBuf(visible ? kDownArrow[x] : 0);
  }
}

void printXboxAuthCenteredLine(uint8_t row, const char* text) {
  const size_t rawWidth = std::strlen(text) * 6u;
  const uint8_t width = rawWidth > 128u ? 128u : (uint8_t)rawWidth;
  display.setCursor((uint8_t)((128u - width) / 2u), row);
  display.print(text);
}

bool renderXboxOneAuthOverlay(bool forceRedraw, bool* exited) {
  if (exited != nullptr) {
    *exited = false;
  }
#if !defined(USE_I2C_DISPLAY) || \
    !defined(ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT)
  (void)forceRedraw;
  return false;
#else
  static XboxAuthOverlayState lastState = XboxAuthOverlayState::Hidden;
  static uint8_t lastKeyPhase = 0xFF;
  static bool lastArrowsVisible = false;
  static bool lastAuthenticated = false;
  static uint32_t unlockStartedMs = 0;
  const uint32_t now = millis();
  const bool xboxOutput =
      canonicalizeOutputMode(get_effective_output_mode()) == OUTPUT_XBOXONE;
  const XboxOneAuthSidecarStatus auth = xbone_auth_passthrough_status();

  if (!xboxOutput || !authOutputModeRequiresAuth(OUTPUT_XBOXONE)) {
    if (lastState != XboxAuthOverlayState::Hidden && exited != nullptr) {
      *exited = true;
    }
    lastState = XboxAuthOverlayState::Hidden;
    lastKeyPhase = 0xFF;
    lastArrowsVisible = false;
    lastAuthenticated = false;
    unlockStartedMs = 0;
    return false;
  }

  if (auth.auth_completed && !lastAuthenticated) {
    unlockStartedMs = now;
  }
  lastAuthenticated = auth.auth_completed;

  XboxAuthOverlayState state = XboxAuthOverlayState::Missing;
  if (auth.auth_completed) {
    if (unlockStartedMs == 0 || now - unlockStartedMs >= 1600) {
      if (lastState != XboxAuthOverlayState::Hidden && exited != nullptr) {
        *exited = true;
      }
      lastState = XboxAuthOverlayState::Hidden;
      return false;
    }
    state = XboxAuthOverlayState::Unlocked;
  } else if (auth.mounted && auth.dongle_ready) {
    state = XboxAuthOverlayState::Authenticating;
  } else if (auth.mounted) {
    state = XboxAuthOverlayState::Starting;
  }

  const uint8_t keyPhase = state == XboxAuthOverlayState::Unlocked
      ? 0
      : static_cast<uint8_t>((now / 250) & 0x03);
  const bool arrowsVisible = state != XboxAuthOverlayState::Unlocked &&
      ((now / 400) & 0x01) == 0;
  const bool layoutChanged = forceRedraw || state != lastState;
  const bool animationChanged = keyPhase != lastKeyPhase ||
      arrowsVisible != lastArrowsVisible;
  if (!layoutChanged && !animationChanged) {
    return true;
  }

  if (layoutChanged) {
    for (uint8_t row = 3; row <= 7; ++row) {
      clearRow(row);
    }
    display.setFont(System5x7);
    if (state == XboxAuthOverlayState::Missing) {
      printXboxAuthCenteredLine(3, "Connect Xbox pad");
      printXboxAuthCenteredLine(4, "to either USB port");
    } else if (state == XboxAuthOverlayState::Starting) {
      printXboxAuthCenteredLine(3, "Xbox pad found");
      printXboxAuthCenteredLine(4, "Starting auth");
    } else if (state == XboxAuthOverlayState::Authenticating) {
      printXboxAuthCenteredLine(3, "Authenticating");
      printXboxAuthCenteredLine(4, "Keep pad connected");
    } else {
      printXboxAuthCenteredLine(3, "Xbox unlocked");
      printXboxAuthCenteredLine(4, "Donor can be removed");
    }
  }

  drawXboxAuthLock(state == XboxAuthOverlayState::Unlocked, keyPhase);
  drawXboxAuthDownArrow(0, arrowsVisible);
  drawXboxAuthDownArrow(120, arrowsVisible);
  lastState = state;
  lastKeyPhase = keyPhase;
  lastArrowsVisible = arrowsVisible;
  return true;
#endif
}

bool renderModern2UsbNoControllerHint(bool forceRedraw) {
  static bool hintShown = false;
  if (!forceRedraw && hintShown) {
    return true;
  }

  for (uint8_t row = 3; row <= 6; ++row) {
    clearRow(row);
  }

  display.setFont(System5x7);
  display.setRow(4);
  display.setCol(10);
  display.print("Connect controller");
  display.setRow(5);
  display.setCol(7);
  display.print("Hold RESET: BT Pair");
  hintShown = true;
  return true;
}
#endif

}  // namespace

void drawPadLayoutPreview(const PadButton* layout, uint8_t layoutCount,
                          uint32_t state, uint8_t colOffset,
                          uint8_t rowOffset) {
  drawPadLayoutState(layout, layoutCount, state, colOffset, rowOffset, false);
}

#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
namespace {

uint8_t getJvsioVisiblePanelButtons() {
#ifdef ENABLE_INPUT_JVS
  const uint8_t buttons = getJvsPhysicalButtonsPerPlayer(getJvsBoardInfo());
  if (buttons < 6 || buttons > 8) {
    return 8;
  }
  return buttons;
#else
  return 8;
#endif
}

uint32_t buildJvsioDisplayMaskFromButtons(uint32_t buttons) {
  uint32_t mask = 0;
  if (buttons & INPUT_PAD_U)    mask |= GPAD_UP;
  if (buttons & INPUT_PAD_D)    mask |= GPAD_DOWN;
  if (buttons & INPUT_PAD_L)    mask |= GPAD_LEFT;
  if (buttons & INPUT_PAD_R)    mask |= GPAD_RIGHT;
  if (buttons & INPUT_A)        mask |= GPAD_A;
  if (buttons & INPUT_B)        mask |= GPAD_B;
  if (buttons & INPUT_X)        mask |= GPAD_X;
  if (buttons & INPUT_Y)        mask |= GPAD_Y;
  if (buttons & INPUT_L1)       mask |= GPAD_L1;
  if (buttons & INPUT_R1)       mask |= GPAD_R1;
  if (buttons & INPUT_L2)       mask |= GPAD_L2;
  if (buttons & INPUT_R2)       mask |= GPAD_R2;
  if (buttons & INPUT_START)    mask |= GPAD_START;
  if (buttons & INPUT_SELECT)   mask |= GPAD_SELECT;
  if (buttons & INPUT_HOME)     mask |= GPAD_HOME;
  if (buttons & INPUT_CAPTURE)  mask |= GPAD_EXTRA0;
  return mask;
}

uint32_t getJvsioDisplayMaskForPlayer(uint8_t player, bool useOutputState) {
  if (player >= max_devices || player >= MAX_USB_OUT || !controllerFrameConst(player).connected) {
    return 0;
  }

  if (useOutputState) {
    return buildJvsioDisplayMaskFromButtons(post_remap_buttons[player]);
  }
  return buildJvsioDisplayMaskFromButtons(controllerFrameConst(player).digital_buttons);
}

bool isJvsioXbox360HostMode(outputMode_t outputMode) {
  return canonicalizeOutputMode(outputMode) == OUTPUT_XINPUT2P &&
         (configuredOutputMode == OUTPUT_XINPUT ||
          autoDetectState == AUTO_STATE_XBOX_360);
}

bool shouldShowJvsioSecondPlayerPad(outputMode_t outputMode) {
  if (isJvsioXbox360HostMode(outputMode)) {
    return false;
  }
  return output_has_secondary_player_slot(outputMode) && max_devices > 1;
}

bool hasJvsioBoardIdentity() {
#ifdef ENABLE_INPUT_JVS
  const JvsBoardInfo& info = getJvsBoardInfo();
  return info.id_received || info.function_check_received || isJvsHostSyncedRecently(1500);
#else
  return false;
#endif
}

constexpr uint8_t kJvsAnalogMaxVisibleChannels = 4;
constexpr uint8_t kJvsAnalogHeaderRow = 3;
constexpr uint8_t kJvsAnalogFirstChannelRow = 4;
constexpr uint8_t kJvsAnalogGaugeCol = 14;
constexpr uint8_t kJvsAnalogGaugeWidth = 48;
constexpr uint8_t kJvsAnalogGaugeCenterTick = 0x2A;

bool jvsAnalogActivityLatched = false;
bool lastJvsAnalogPanelActive = false;

uint8_t jvsAnalogBitsForDisplay(const JvsBoardInfo& board) {
  uint8_t bits = board.analog_bits;
  if (bits < 2 || bits > 16) {
    bits = 16;
  }
  return bits;
}

uint16_t jvsAnalogMovementThresholdRaw(const JvsBoardInfo& board) {
  // JVS always transports analog values as a 16-bit, MSB-first word. Boards
  // with lower advertised precision left-align their significant bits, so
  // thresholds and gauges must stay in the full 0..65535 transport range.
  const uint8_t bits = jvsAnalogBitsForDisplay(board);
  const uint32_t significantStep = 1UL << (16u - bits);
  uint32_t threshold = 0xFFFFUL / 128UL;
  if (threshold < significantStep) {
    threshold = significantStep;
  }
  if (threshold > 1024) {
    threshold = 1024;
  }
  return (uint16_t)threshold;
}

uint16_t jvsAnalogRawDelta(uint16_t a, uint16_t b) {
  return a > b ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

bool jvsAnalogMovementDetected(const JvsHostState& state, const JvsBoardInfo& board) {
  static bool initialized = false;
  static uint8_t lastCount = 0;
  static uint8_t lastBits = 0;
  static uint16_t lastValues[kJvsMaxAnalogChannels] = {};

  if (deviceMode != RZORD_JVS ||
      !isJvsHostSyncedRecently(1500) ||
      state.analog_channels == 0) {
    initialized = false;
    lastCount = 0;
    return false;
  }

  const uint8_t count = min(state.analog_channels, (uint8_t)kJvsMaxAnalogChannels);
  const uint8_t bits = jvsAnalogBitsForDisplay(board);
  bool moved = false;

  if (initialized && count == lastCount && bits == lastBits) {
    const uint16_t threshold = jvsAnalogMovementThresholdRaw(board);
    for (uint8_t i = 0; i < count; ++i) {
      if (jvsAnalogRawDelta(state.analog_values[i], lastValues[i]) > threshold) {
        moved = true;
      }
    }
  }

  for (uint8_t i = 0; i < count; ++i) {
    lastValues[i] = state.analog_values[i];
  }
  lastCount = count;
  lastBits = bits;
  initialized = true;
  return moved;
}

uint8_t jvsAnalogGaugePosition(uint16_t sample) {
  const uint8_t innerWidth = kJvsAnalogGaugeWidth - 2;
  if (innerWidth <= 1) {
    return 1;
  }
  return (uint8_t)(1u +
      (((uint32_t)sample * (uint32_t)(innerWidth - 1u) + 32767UL) /
       65535UL));
}

void drawJvsAnalogPositionGauge(uint16_t sample, uint8_t row) {
  const uint8_t position = jvsAnalogGaugePosition(sample);
  const uint8_t center = kJvsAnalogGaugeWidth / 2;

  display.setCursor(kJvsAnalogGaugeCol, row);
  for (uint8_t i = 0; i < kJvsAnalogGaugeWidth; ++i) {
    uint8_t column = kAnalogTriggerGaugeEmpty;
    if (i == 0 || i == kJvsAnalogGaugeWidth - 1 || i == position) {
      column = kAnalogTriggerGaugeFilled;
    } else if (i == center) {
      column = kJvsAnalogGaugeCenterTick;
    }
    display.ssd1306WriteRamBuf(column);
  }
}

bool renderJvsLiveAnalogHome(bool forceRedraw, bool* forceNormalRedraw) {
  if (forceNormalRedraw != nullptr) {
    *forceNormalRedraw = false;
  }

  const JvsHostState& state = getJvsHostState();
  const JvsBoardInfo& board = getJvsBoardInfo();
  const auto& analogConfig = jvsAnalogGetConfig();
  const bool analogMappingEnabled = jvsAnalogLayout(analogConfig) != JvsAnalogLayout::Off &&
      (analogConfig.profile == JVS_ANALOG_PROFILE_LEGACY ||
       jvsAnalogConfigMatchesBoard(analogConfig, board.io_id,
                                  state.analog_channels, board.analog_bits));
  const bool analogOnBus =
      deviceMode == RZORD_JVS &&
      isJvsHostSyncedRecently(1500) &&
      state.analog_channels != 0;
  // Without usable routes USB stays neutral, and the controls' own movement only
  // picks the picture. It reads them against the range swept so far.
  JvsAnalogConfig detectedConfig;
  const bool detected = analogOnBus && !analogMappingEnabled &&
                        jvsDetectedPanelConfig(detectedConfig);
  const bool analogAvailable = analogOnBus && (analogMappingEnabled || detected);

  const bool analogMoved = jvsAnalogMovementDetected(state, board);
  if (!analogAvailable) {
    jvsAnalogActivityLatched = false;
  } else if (analogMoved) {
    // Some digital-only JVS cabinets advertise unused analog channels. Do not
    // replace their normal control display until an axis actually moves.
    jvsAnalogActivityLatched = true;
  }

  const JvsAnalogConfig& shownConfig =
      detected ? detectedConfig : jvsEffectiveAnalogConfig(0);
  jvs_driving_home::Values drivingValues;
  const bool driving = analogAvailable && jvs_driving_home::read(
      shownConfig, board.io_id, state.analog_channels,
      board.analog_bits, state.analog_values, drivingValues);
  jvs_driving_home::StickValues stickValues;
  const bool stick = analogAvailable && !driving && jvs_driving_home::readStick(
      shownConfig, board.io_id, state.analog_channels,
      board.analog_bits, state.analog_values, stickValues);
  const bool pictured = driving || stick;

  // Wheel and stick routes select their display directly; other enabled layouts
  // retain the existing activity latch for unused-channel noise.
  const bool rawGauges = !detected &&
                         (state.analog_channels == 1 || jvsAnalogActivityLatched);
  const bool active = analogAvailable && (pictured || rawGauges);
  if (!active) {
    if (lastJvsAnalogPanelActive) {
      lastJvsAnalogPanelActive = false;
      if (forceNormalRedraw != nullptr) {
        *forceNormalRedraw = true;
      }
    }
    return false;
  }

  static bool lastRenderedValid = false;
  static uint8_t lastRenderedChannels = 0;
  static uint8_t lastRenderedBits = 0;
  static uint8_t lastRenderedPositions[kJvsAnalogMaxVisibleChannels] = {};
  static uint32_t lastRenderedControlState = 0;
  static uint8_t lastRenderedPanelButtons = 0;
  static const PadButton* lastRenderedLayout = nullptr;
  static bool lastRenderedDriving = false;
  static bool lastRenderedStick = false;
  static jvs_driving_home::Values lastDrivingValues;
  static jvs_driving_home::StickValues lastStickValues;

  const uint8_t totalChannels = min(state.analog_channels, (uint8_t)kJvsMaxAnalogChannels);
  const uint8_t displayChannels = min(totalChannels, kJvsAnalogMaxVisibleChannels);
  const uint8_t bits = jvsAnalogBitsForDisplay(board);
  const uint8_t panelButtons = getJvsioVisiblePanelButtons();
  const uint32_t controlState = applyTurboVisualState(
      0, getJvsioDisplayMaskForPlayer(0, false));
  const PadButton* controlLayout = nullptr;
  uint8_t controlLayoutCount = 0;
  getLayoutForPlayer(0, &controlLayout, &controlLayoutCount);
  // A wheel or stick cabinet shows its Start and Coin. Action buttons still
  // reach USB; a learned panel names real ones, so it keeps its own layout.
  if (pictured && !jvsPanelLearnedActive()) {
    controlLayout = menu_pad_layouts_internal::padLayoutJvsDriving;
    controlLayoutCount = menu_pad_layouts_internal::PAD_LAYOUT_JVS_DRIVING_COUNT;
  }
  const bool layoutChanged = forceRedraw ||
                             !lastJvsAnalogPanelActive ||
                             !lastRenderedValid ||
                             displayChannels != lastRenderedChannels ||
                             bits != lastRenderedBits ||
                             driving != lastRenderedDriving ||
                             stick != lastRenderedStick ||
                             panelButtons != lastRenderedPanelButtons ||
                             controlLayout != lastRenderedLayout;
  bool axisChanged[kJvsAnalogMaxVisibleChannels] = {};
  bool needsRedraw = layoutChanged || controlState != lastRenderedControlState;
  const bool drivingChanged = driving && (layoutChanged ||
      !jvs_driving_home::samePixels(drivingValues, lastDrivingValues));
  const bool stickChanged = stick && (layoutChanged ||
      !jvs_driving_home::samePixels(stickValues, lastStickValues));
  needsRedraw = needsRedraw || drivingChanged || stickChanged;
  for (uint8_t i = 0; !pictured && i < displayChannels; ++i) {
    axisChanged[i] = layoutChanged ||
                     jvsAnalogGaugePosition(state.analog_values[i]) !=
                         lastRenderedPositions[i];
    needsRedraw = needsRedraw || axisChanged[i];
  }

  if (!needsRedraw) {
    return true;
  }

  if (layoutChanged) {
    for (uint8_t row = 3; row <= 7; ++row) {
      clearRow(row);
    }
  }

  display.setFont(System5x7);
  if (drivingChanged) {
    jvs_driving_home::draw(display, drivingValues, System5x7);
  }
  if (stickChanged) {
    jvs_driving_home::drawStick(display, stickValues, System5x7);
  }
  if (!pictured && layoutChanged) {
    char header[16];
    snprintf(header, sizeof(header), "A0-A%u %ub",
             (unsigned)(displayChannels - 1),
             (unsigned)bits);
    display.setCursor(0, kJvsAnalogHeaderRow);
    display.print(header);
  }

  for (uint8_t i = 0; !pictured && i < displayChannels; ++i) {
    if (!axisChanged[i]) {
      continue;
    }
    const uint16_t sample = state.analog_values[i];
    const uint8_t row = kJvsAnalogFirstChannelRow + i;
    if (layoutChanged) {
      char label[4];
      snprintf(label, sizeof(label), "A%u", (unsigned)i);
      display.setCursor(0, row);
      display.print(label);
    }
    drawJvsAnalogPositionGauge(sample, row);
    lastRenderedPositions[i] = jvsAnalogGaugePosition(sample);
  }

  if (layoutChanged || controlState != lastRenderedControlState) {
    drawPadLayoutState(controlLayout,
                       controlLayoutCount,
                       controlState,
                       kSecondPadColOffset,
                       kHomePadRowOffset,
                       true,
                       false,
                       get_effective_output_mode(),
                       nullptr);
  }

  lastRenderedValid = true;
  lastRenderedChannels = displayChannels;
  lastRenderedBits = bits;
  lastRenderedControlState = controlState;
  lastRenderedPanelButtons = panelButtons;
  lastRenderedLayout = controlLayout;
  lastRenderedDriving = driving;
  lastRenderedStick = stick;
  lastDrivingValues = drivingValues;
  lastStickValues = stickValues;
  lastJvsAnalogPanelActive = true;
  return true;
}

void renderJvsioNoCabinetHint(bool forceRedraw) {
  static bool hintShown = false;
  if (!forceRedraw && hintShown) return;
  for (uint8_t row = 3; row <= 7; ++row) {
    clearRow(row);
  }

  display.setFont(System5x7);
  display.setRow(6);
  display.setCol(0);
  display.print("JVS IO");
  display.setCol(78);
  display.print("Auth USB");

  display.setRow(7);
  display.setCol(18);
  display.print('v');
  display.setCol(102);
  display.print('v');

  hintShown = true;
}

void getJvsioArcadePreviewLayout(outputMode_t outputMode,
                                 const PadButton** layout,
                                 uint8_t* layoutCount) {
  getVirtualOutputPadLayout(deviceMode, outputMode, layout, layoutCount);
}

void getJvsioHostPreviewLayout(outputMode_t outputMode,
                               const PadButton** layout,
                               uint8_t* layoutCount) {
  getVirtualOutputPadLayout(RZORD_NONE, outputMode, layout, layoutCount);
}

void renderJvsioHomeMapping(const PadButton* inputLayout,
                            uint8_t inputLayoutCount,
                            const PadButton* outputLayout,
                            uint8_t outputLayoutCount,
                            bool showSecondPlayerPad,
                            bool showDualHostPreview,
                            bool forceRedraw) {
  static uint32_t lastInputState = 0;
  static uint32_t lastOutputState = 0;
  static uint8_t lastButtonsPerPlayer = 0;
  static bool lastShowSecondPlayerPad = false;
  static bool lastShowDualHostPreview = false;

  const uint8_t buttonsPerPlayer = getJvsioVisiblePanelButtons();
  const bool splitDisplay = showSecondPlayerPad || showDualHostPreview;
  const uint32_t inputState = applyTurboVisualState(
    0,
    splitDisplay ? getJvsioDisplayMaskForPlayer(0, false) : 0);
  const uint8_t outputPlayer =
    showSecondPlayerPad ? 1 : 0;
  const uint32_t outputState = applyTurboVisualState(
    outputPlayer,
    showSecondPlayerPad ?
      getJvsioDisplayMaskForPlayer(1, false) :
      (splitDisplay ?
       getJvsioDisplayMaskForPlayer(0, true) :
       getJvsioDisplayMaskForPlayer(0, false)));
  const bool useCenteredButtonLabels =
    !splitDisplay && menu_home_button_labels != 0;
  const bool layoutChanged = forceRedraw ||
                             (buttonsPerPlayer != lastButtonsPerPlayer) ||
                             (showSecondPlayerPad != lastShowSecondPlayerPad) ||
                             (showDualHostPreview != lastShowDualHostPreview);
  const bool stateChanged = layoutChanged ||
                            (inputState != lastInputState) ||
                            (outputState != lastOutputState);

  if (!stateChanged ||
      (splitDisplay && (inputLayout == nullptr || outputLayout == nullptr)) ||
      (!splitDisplay && outputLayout == nullptr)) {
    return;
  }

  if (layoutChanged) {
    for (uint8_t row = 3; row <= 6; ++row) {
      clearRow(row);
    }
  }

  if (splitDisplay) {
    drawPadLayoutState(inputLayout,
                       inputLayoutCount,
                       inputState,
                       0,
                       kHomePadRowOffset,
                       true,
                       false,
                       get_effective_output_mode(),
                       nullptr);

    if (layoutChanged) {
      display.clear(kSecondPadColOffset, 127, 3, 6);
    }
    drawPadLayoutState(outputLayout,
                       outputLayoutCount,
                       outputState,
                       kSecondPadColOffset,
                       kHomePadRowOffset,
                       showSecondPlayerPad,
                       false,
                       get_effective_output_mode(),
                       nullptr);
  } else {
    drawPadLayoutState(outputLayout,
                       outputLayoutCount,
                       outputState,
                       kCenteredPadColOffset,
                       kHomePadRowOffset,
                       true,
                       useCenteredButtonLabels,
                       get_effective_output_mode(),
                       nullptr);
  }

  display.setFont(System5x7);
  lastInputState = inputState;
  lastOutputState = outputState;
  lastButtonsPerPlayer = buttonsPerPlayer;
  lastShowSecondPlayerPad = showSecondPlayerPad;
  lastShowDualHostPreview = showDualHostPreview;
}

}  // namespace
#endif

#ifdef ENABLE_INPUT_JVS
bool renderJvsDebugPanel(bool force) {
  (void)force;
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  return false;
#else
  if (deviceMode != RZORD_JVS) {
    return false;
  }

  const JvsHostState& state = getJvsHostState();
  const JvsBoardInfo& info = getJvsBoardInfo();
  const uint32_t frameMask = buildButtonMaskFromReport(0) & 0xFFFFFFu;
  static char lastLines[5][24] = {{0}};
  char lines[5][24];

  snprintf(lines[0], sizeof(lines[0]), "H:%02u SN:%lu R:%u",
           getJvsHostMachineState(),
           (unsigned long)getJvsHostSyncedCount(),
           state.ready ? 1 : 0);
  snprintf(lines[1], sizeof(lines[1]), "P1:%02X %02X C:%02X",
           state.sw_state0[0], state.sw_state1[0], state.coin_state);
  snprintf(lines[2], sizeof(lines[2]), "P2:%02X %02X JP:%u",
           state.sw_state0[1], state.sw_state1[1], state.players);
  snprintf(lines[3], sizeof(lines[3]), "F:%06lX CF:%u",
           (unsigned long)frameMask,
           controllerFrameConst(0).connected ? 1 : 0);
  snprintf(lines[4], sizeof(lines[4]), "ID:%u FC:%u C:%u P:%u",
           info.id_received ? 1 : 0,
           info.function_check_received ? 1 : 0,
           state.connected ? 1 : 0,
           info.players);

  for (uint8_t i = 0; i < 5; ++i) {
    if (!force && std::strcmp(lastLines[i], lines[i]) == 0) {
      continue;
    }

    clearRow(3 + i);
    display.setRow(3 + i);
    display.setCol(0);
    display.print(lines[i]);

    std::strncpy(lastLines[i], lines[i], sizeof(lastLines[i]) - 1);
    lastLines[i][sizeof(lastLines[i]) - 1] = '\0';
  }

  return true;
#endif
}
#endif

bool didControllerNamesChange() {
  static char lastDisplayedControllerType[MAX_USB_OUT][16] = {};
  bool changed = false;
  for (uint8_t player = 0; player < MAX_USB_OUT; ++player) {
    const char* currentType = "";
    if (player < max_devices && controllerFrameConst(player).connected) {
      currentType = controllerFrameConst(player).controller_type_name;
    }
    if (std::strncmp(lastDisplayedControllerType[player], currentType,
                     sizeof(lastDisplayedControllerType[player])) != 0) {
      std::strncpy(lastDisplayedControllerType[player], currentType,
                   sizeof(lastDisplayedControllerType[player]) - 1);
      lastDisplayedControllerType[player]
                                 [sizeof(lastDisplayedControllerType[player]) - 1] = '\0';
      changed = true;
    }
  }
  return changed;
}

#if defined(ENABLE_INPUT_PSX) && defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
// The real left stick replaces the DualShock D-pad. Its reel-speed gauge is
// to the left; page zero belongs to the four standard shoulder buttons.
void drawFishingIndicators(const controller_state_t& frame, const PadButton* layout,
                           uint8_t count, uint8_t colOffset) {
  // Origin applies the button layout's vertical offset to pixel-surface writes
  // and restores it on return; the indicators must follow the same placement.
  home_pad_geometry::Origin origin(layout, count, ReflexPad5x7);
  const int x = 13 + (int(frame.LX) * 4) / 128;
  const int y = 20 + (int(frame.LY) * 4) / 128;
  const int speed = int(frame.RX) + 128;
  const int fill = (speed * 20 + 254) / 255;
  for (uint8_t page = 1; page < 4; ++page) {
    display.setCursor(colOffset, kHomePadRowOffset + page);
    for (int column = 0; column < 20; ++column) {
      uint8_t bits = 0;
      for (int bit = 0; bit < 8; ++bit) {
        const int yy = int(page)*8 + bit;
        bool ink = false;
        if (column >= 7) {
          const int dx=column-13, dy=yy-20, d=dx*dx+dy*dy;
          ink = (d >= 28 && d <= 42) ||
                (column >= x-1 && column <= x+1 && yy >= y-1 && yy <= y+1);
        } else if (column <= 4 && yy >= 9 && yy <= 30) {
          ink = column==0 || column==4 || yy==9 || yy==30 ||
                (yy >= 30-fill && yy < 30);
        }
        if (ink) bits |= uint8_t(1u << bit);
      }
      display.ssd1306WriteRamBuf(bits);
    }
  }
}
#endif

void updateRealtimeButtons() {
  static uint32_t lastButtonState[2] = { 0, 0 };
  static const PadButton* currentLayout[2] = { nullptr, nullptr };
  static uint8_t currentLayoutCount[2] = { 0, 0 };
  static char lastControllerType[2][16] = { {0}, {0} };
  static DeviceEnum lastDeviceMode = RZORD_NONE;
  static outputMode_t lastOutputPadMode = OUTPUT_LAST;
  static bool lastShowOutputVirtualPad = false;
  static bool lastShowPhysicalSecondPad = false;
  static bool lastCenterOutputVirtualPad = false;
  static bool lastShowHomeAnalogGauge = false;
  static uint8_t lastButtonLabels = 0;
  static uint8_t lastAnalogL2[2] = { 0, 0 };
  static uint8_t lastAnalogR2[2] = { 0, 0 };
  static uint8_t lastWheelPaddle[2] = { 0xFF, 0xFF };
  static int16_t lastNeGconTwist[2] = { 0, 0 };
  static bool lastNeGconTwistValid[2] = { false, false };
  static uint8_t lastTriggerRawDebugL2 = 0xFF;
  static uint8_t lastTriggerRawDebugR2 = 0xFF;
  static uint32_t lastAnalogGaugeRedrawMs = 0;
  static bool lastPhysicalOnlyInput = false;
  static uint8_t lastPrimaryPhysicalPlayer = 0;
  static uint8_t lastSecondaryPhysicalPlayer = 1;

  outputMode_t effectiveOutputMode = get_effective_output_mode();
  const bool mergedSinglePadView = classic_dual_merge_enabled != 0;
  uint8_t primaryPhysicalPlayer = 0;
  uint8_t secondaryPhysicalPlayer = 1;
  const bool passivePadMode = is_passive_controller_mode(deviceMode);
  if (passivePadMode && max_devices > 1 &&
      !controllerFrameConst(0).connected && controllerFrameConst(1).connected) {
    primaryPhysicalPlayer = 1;
  }
#if defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT)
  uint8_t modern2usbVisiblePlayers[2] = { 0xFF, 0xFF };
  for (uint8_t logical = 0;
       logical < max_devices && logical < MAX_USB_OUT;
       ++logical) {
    if (!controllerFrameConst(logical).connected) {
      continue;
    }
    if (modern2usbVisiblePlayers[0] == 0xFF) {
      modern2usbVisiblePlayers[0] = logical;
    } else {
      modern2usbVisiblePlayers[1] = logical;
      break;
    }
  }
  const bool modern2usbPrimaryConnected = modern2usbVisiblePlayers[0] != 0xFF;
  const bool modern2usbSecondaryConnected = modern2usbVisiblePlayers[1] != 0xFF;
  if (modern2usbPrimaryConnected) {
    // Preserve host slot assignment while compacting whichever players are
    // present into the OLED's two physical-pad positions.
    primaryPhysicalPlayer = modern2usbVisiblePlayers[0];
  }
  if (modern2usbSecondaryConnected) {
    secondaryPhysicalPlayer = modern2usbVisiblePlayers[1];
  }
#endif
  bool showPhysicalSecondPad =
    !mergedSinglePadView &&
    (output_shown_player_count(effectiveOutputMode) > 1) &&
    controllerFrameConst(1).connected &&
    (!passivePadMode || controllerFrameConst(0).connected) &&
    !shouldHideEmptySecondPadInAutoInputMode();
  bool showOutputVirtualPad =
    !mergedSinglePadView &&
    controllerFrameConst(0).connected &&
    shouldShowVirtualOutputPad(deviceMode, effectiveOutputMode) &&
    ((max_devices == 1) || !showPhysicalSecondPad);
#if defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT)
  // MODERN2USB's home view represents stable logical players, not an input/output
  // comparison. Center a lone controller and move it left when any second
  // logical player is connected, including a mixed USB/BT slot above P2.
  showOutputVirtualPad = false;
  showPhysicalSecondPad = modern2usbPrimaryConnected && modern2usbSecondaryConnected;
#endif
  bool centerOutputVirtualPad =
    showOutputVirtualPad &&
    !showPhysicalSecondPad;
#if defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT)
  centerOutputVirtualPad =
      modern2usbPrimaryConnected && !showPhysicalSecondPad;
#endif
  const bool showHomeAnalogGauge = homeAnalogGaugeShouldShow(
    deviceMode, primaryPhysicalPlayer, centerOutputVirtualPad)
#ifdef ENABLE_INPUT_PSX
    && std::strcmp(controllerFrameConst(primaryPhysicalPlayer).controller_type_name, "Fishing") != 0
#endif
    ;
  const uint8_t primaryPadColOffset = showHomeAnalogGauge
    ? homeAnalogGaugePadColumnOffset(deviceMode,controllerFrameConst(primaryPhysicalPlayer))
    : (centerOutputVirtualPad ? kCenteredPadColOffset : 0);
  bool showSecondPad = (showOutputVirtualPad && !centerOutputVirtualPad) || showPhysicalSecondPad;
  bool forceRedraw = padDisplayNeedsRedraw;
  if (forceRedraw) {
    padDisplayNeedsRedraw = false;
  }
  static bool lastPassiveAwaitingActivity = false;
  const bool passiveAwaitingActivity = passivePadMode &&
      !anyInputFrameConnected(max_devices);
  if (passiveAwaitingActivity != lastPassiveAwaitingActivity) forceRedraw = true;
  lastPassiveAwaitingActivity = passiveAwaitingActivity;
  if (passiveAwaitingActivity) {
    if (forceRedraw || lastDeviceMode != deviceMode) {
      display.clear(0, 127, kHomePadRowOffset, 7);
      display.setFont(System5x7);
      display.setCursor(22, 5);
      display.print("Press a button");
    }
    lastDeviceMode = deviceMode;
    return;
  }
  const char* physicalOnlyName = activeInputAdapterPhysicalConnectionDisplayName();
  const bool physicalOnlyInput =
    physicalOnlyName != nullptr &&
    physicalOnlyName[0] != '\0' &&
    !anyInputFrameConnected(max_devices);
  if (lastPhysicalOnlyInput != physicalOnlyInput) {
    forceRedraw = true;
  }
  lastPhysicalOnlyInput = physicalOnlyInput;
  bool secondPadVisibilityChanged =
    (lastOutputPadMode != effectiveOutputMode) ||
    (lastShowOutputVirtualPad != showOutputVirtualPad) ||
    (lastShowPhysicalSecondPad != showPhysicalSecondPad) ||
    (lastCenterOutputVirtualPad != centerOutputVirtualPad) ||
    (lastShowHomeAnalogGauge != showHomeAnalogGauge) ||
    (lastButtonLabels != menu_home_button_labels) ||
    (lastPrimaryPhysicalPlayer != primaryPhysicalPlayer) ||
    (lastSecondaryPhysicalPlayer != secondaryPhysicalPlayer);

#ifdef ENABLE_INPUT_JVS
  if (renderJvsDebugPanel(forceRedraw)) {
    return;
  }
#endif

#if defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT)
  bool xboxAuthOverlayExited = false;
  if (renderXboxOneAuthOverlay(forceRedraw, &xboxAuthOverlayExited)) {
    lastDeviceMode = deviceMode;
    return;
  }
  if (xboxAuthOverlayExited) {
    forceRedraw = true;
  }

  if (!modern2usbPrimaryConnected &&
      renderModern2UsbNoControllerHint(forceRedraw)) {
    lastDeviceMode = deviceMode;
    return;
  }
#endif

  if (physicalOnlyInput &&
      renderPhysicalOnlyInputHomeHint(forceRedraw, physicalOnlyName)) {
    lastDeviceMode = deviceMode;
    return;
  }

#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  renderUsb2db15ArcadeHome(forceRedraw);
  lastDeviceMode = deviceMode;
  return;
#endif

#ifdef ENABLE_INPUT_DREAMCAST
  bool twinStickPanelExited = false;
  if (renderDreamcastTwinStickHome(forceRedraw, &twinStickPanelExited)) {
    lastDeviceMode = deviceMode;
    return;
  }
  if (twinStickPanelExited) forceRedraw = true;
#endif

#ifdef ENABLE_INPUT_SNES
  bool nttDataPanelExited = false;
  if (renderNttDataHome(forceRedraw, &nttDataPanelExited)) {
    return;
  }
  if (nttDataPanelExited) {
    // A second controller, disconnect, or mode change restores the compact
    // layout and clears the entire old wide panel, including held keypad keys.
    forceRedraw = true;
  }
  bool famicomNetworkPanelExited = false;
  if (renderFamicomNetworkHome(forceRedraw, &famicomNetworkPanelExited)) {
    return;
  }
  if (famicomNetworkPanelExited) {
    forceRedraw = true;
  }
#endif

#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  bool jvsAnalogPanelExited = false;
  if (renderJvsLiveAnalogHome(forceRedraw, &jvsAnalogPanelExited)) {
    return;
  }
  if (jvsAnalogPanelExited) {
    forceRedraw = true;
  }

  const bool jvsioBoardKnown = hasJvsioBoardIdentity();
  if (deviceMode == RZORD_JVS && !controllerFrameConst(0).connected && !jvsioBoardKnown) {
    renderJvsioNoCabinetHint(forceRedraw);
    lastDeviceMode = deviceMode;
    return;
  }

  if (deviceMode == RZORD_JVS && (controllerFrameConst(0).connected || jvsioBoardKnown)) {
    lastDeviceMode = deviceMode;
    const bool showJvsioSecondPlayerPad = shouldShowJvsioSecondPlayerPad(effectiveOutputMode);
    bool showJvsioDualHostPreview = false;
    getLayoutForPlayer(0, &currentLayout[0], &currentLayoutCount[0]);
    if (showJvsioSecondPlayerPad) {
      getLayoutForPlayer(1, &currentLayout[1], &currentLayoutCount[1]);
    } else if (showJvsioDualHostPreview) {
      getJvsioHostPreviewLayout(effectiveOutputMode,
                                &currentLayout[1], &currentLayoutCount[1]);
      if (currentLayout[1] == nullptr) {
        showJvsioDualHostPreview = false;
        getJvsioArcadePreviewLayout(effectiveOutputMode,
                                    &currentLayout[1], &currentLayoutCount[1]);
      }
    } else {
      getJvsioArcadePreviewLayout(effectiveOutputMode,
                                  &currentLayout[1], &currentLayoutCount[1]);
    }
    renderJvsioHomeMapping(currentLayout[0], currentLayoutCount[0],
                           currentLayout[1], currentLayoutCount[1],
                           showJvsioSecondPlayerPad,
                           showJvsioDualHostPreview,
                           forceRedraw);
    return;
  }
#endif

  bool controllerTypeChanged = false;
  const uint8_t displayedPhysicalPlayers[2] = {
    primaryPhysicalPlayer,
    secondaryPhysicalPlayer,
  };
  const uint8_t displayedPhysicalCount = showPhysicalSecondPad ? 2 : 1;
  for (uint8_t displayIndex = 0; displayIndex < displayedPhysicalCount; ++displayIndex) {
    const uint8_t player = displayedPhysicalPlayers[displayIndex];
    if (player < max_devices && controllerFrameConst(player).connected) {
      if (std::strncmp(lastControllerType[displayIndex],
                       controllerFrameConst(player).controller_type_name,
                       sizeof(lastControllerType[displayIndex])) != 0) {
        std::strncpy(lastControllerType[displayIndex],
                     controllerFrameConst(player).controller_type_name,
                     sizeof(lastControllerType[displayIndex]) - 1);
        lastControllerType[displayIndex][sizeof(lastControllerType[displayIndex]) - 1] = '\0';
        controllerTypeChanged = true;
      }
    }
  }

  static bool popnMode = false;
  static bool guitarFreaksMode = false;
  bool popnModeChanged = false;
  bool guitarFreaksModeChanged = false;
  #ifdef ENABLE_INPUT_PSX
  if (deviceMode == RZORD_PSX) {
    bool isPopn = (std::strcmp(controllerFrameConst(0).controller_type_name, "Pop'n") == 0 ||
                   std::strcmp(controllerFrameConst(0).controller_type_name, "PopN") == 0);
    bool isGuitarFreaks = (std::strcmp(controllerFrameConst(0).controller_type_name, "GuitarFreaks") == 0);
    if (isPopn != popnMode) {
      popnMode = isPopn;
      popnModeChanged = true;
    }
    if (isGuitarFreaks != guitarFreaksMode) {
      guitarFreaksMode = isGuitarFreaks;
      guitarFreaksModeChanged = true;
    }
  } else {
    if (popnMode) {
      popnMode = false;
      popnModeChanged = true;
    }
    if (guitarFreaksMode) {
      guitarFreaksMode = false;
      guitarFreaksModeChanged = true;
    }
  }
  #endif

  static uint8_t lastJaguarRotaryMask = 0;
  bool jaguarRotaryModeChanged = false;
  #ifdef ENABLE_INPUT_JAGUAR
  uint8_t jaguarRotaryMask = 0;
  for (uint8_t p = 0; p < max_devices && p < 2; ++p) {
    if (jaguarRotaryActivePorts[p]) {
      jaguarRotaryMask |= (1 << p);
    }
  }
  if (jaguarRotaryMask != lastJaguarRotaryMask) {
    lastJaguarRotaryMask = jaguarRotaryMask;
    jaguarRotaryModeChanged = true;
  }
  #endif

  const bool homeLayoutNeedsRedraw =
    lastDeviceMode != deviceMode ||
    forceRedraw ||
    controllerTypeChanged ||
    popnModeChanged ||
    guitarFreaksModeChanged ||
    jaguarRotaryModeChanged ||
    secondPadVisibilityChanged;

  if (homeLayoutNeedsRedraw) {
    lastDeviceMode = deviceMode;
    lastOutputPadMode = effectiveOutputMode;
    lastShowOutputVirtualPad = showOutputVirtualPad;
    lastShowPhysicalSecondPad = showPhysicalSecondPad;
    lastCenterOutputVirtualPad = centerOutputVirtualPad;
    lastShowHomeAnalogGauge = showHomeAnalogGauge;
    lastButtonLabels = menu_home_button_labels;
    lastPrimaryPhysicalPlayer = primaryPhysicalPlayer;
    lastSecondaryPhysicalPlayer = secondaryPhysicalPlayer;

    #ifdef ENABLE_INPUT_PSX
    if (!centerOutputVirtualPad &&
        deviceMode == RZORD_PSX &&
        getSharedControllerTypePadLayout(controllerFrameConst(primaryPhysicalPlayer).controller_type_name,
            &currentLayout[0], &currentLayoutCount[0])) {
      currentLayout[1] = currentLayout[0];
      currentLayoutCount[1] = currentLayoutCount[0];
    } else
    #endif
    {
      if (centerOutputVirtualPad) {
        getLayoutForPlayer(primaryPhysicalPlayer, &currentLayout[0], &currentLayoutCount[0]);
        currentLayout[1] = nullptr;
        currentLayoutCount[1] = 0;
      } else {
        getLayoutForPlayer(primaryPhysicalPlayer, &currentLayout[0], &currentLayoutCount[0]);
        if (showOutputVirtualPad) {
          getVirtualOutputPadLayout(deviceMode, effectiveOutputMode,
                                    &currentLayout[1], &currentLayoutCount[1]);
        } else if (showPhysicalSecondPad) {
          getLayoutForPlayer(secondaryPhysicalPlayer, &currentLayout[1], &currentLayoutCount[1]);
        } else {
          currentLayout[1] = nullptr;
          currentLayoutCount[1] = 0;
        }
      }
    }

    display.setFont(ReflexPad5x7);
    uint8_t maxRow = 6;
    #ifdef ENABLE_INPUT_JAGUAR
    if (deviceMode == RZORD_JAGUAR) {
      bool showJaguarKeypadRow = false;
      for (uint8_t p = 0; p < max_devices && p < 2; ++p) {
        if (!jaguarRotaryActivePorts[p]) {
          showJaguarKeypadRow = true;
          break;
        }
      }
      maxRow = showJaguarKeypadRow ? 7 : 6;
    }
    #endif
#if defined(PRODUCT_CLASSIC2USB)
    maxRow = 7;
#endif
    for (uint8_t row = 3; row <= maxRow; ++row) {
      clearRow(row);
    }

    #ifdef ENABLE_INPUT_DREAMCAST
    const bool primaryLayoutDreamcastWheel =
      !centerOutputVirtualPad &&
      deviceMode == RZORD_DREAMCAST &&
      isDreamcastWheelFrame(max_devices > primaryPhysicalPlayer ? &controllerFrameConst(primaryPhysicalPlayer) : nullptr);
    const bool secondaryLayoutDreamcastWheel =
      showPhysicalSecondPad &&
      !showOutputVirtualPad &&
      deviceMode == RZORD_DREAMCAST &&
      isDreamcastWheelFrame(max_devices > 1 ? &controllerFrameConst(1) : nullptr);
    #else
    const bool primaryLayoutDreamcastWheel = false;
    const bool secondaryLayoutDreamcastWheel = false;
    #endif
    #ifdef ENABLE_INPUT_SATURN
    const bool primaryLayoutSaturnWheel =
      !centerOutputVirtualPad &&
      deviceMode == RZORD_SATURN &&
      isSaturnWheelFrame(max_devices > primaryPhysicalPlayer ? &controllerFrameConst(primaryPhysicalPlayer) : nullptr);
    const bool secondaryLayoutSaturnWheel =
      showPhysicalSecondPad &&
      !showOutputVirtualPad &&
      deviceMode == RZORD_SATURN &&
      isSaturnWheelFrame(max_devices > 1 ? &controllerFrameConst(1) : nullptr);
    #else
    const bool primaryLayoutSaturnWheel = false;
    const bool secondaryLayoutSaturnWheel = false;
    #endif

    if (currentLayout[0] && !primaryLayoutDreamcastWheel) {
      if (!primaryLayoutSaturnWheel) {
        const bool useCenteredLabels =
          centerOutputVirtualPad &&
          shouldUseCenteredOutputButtonLabels();
        drawPadLayoutState(currentLayout[0],
                           currentLayoutCount[0],
                           0,
                           primaryPadColOffset,
                           kHomePadRowOffset,
                           centerOutputVirtualPad ?
                             shouldUseArcadeJoystickForSecondaryPad(true, false, effectiveOutputMode) :
                             shouldUseArcadeJoystickForPrimaryPad(),
                           useCenteredLabels,
                           effectiveOutputMode,
                           max_devices > primaryPhysicalPlayer ? &controllerFrameConst(primaryPhysicalPlayer) : nullptr);
      }
    }

    if (showSecondPad && currentLayout[1]) {
      const bool useSecondaryLabels =
        showOutputVirtualPad &&
        shouldUseSideBySideOutputButtonLabels();
      if (!secondaryLayoutDreamcastWheel) {
        if (!secondaryLayoutSaturnWheel) {
          drawPadLayoutState(currentLayout[1],
                             currentLayoutCount[1],
                             0,
                             kSecondPadColOffset,
                             kHomePadRowOffset,
                             shouldUseArcadeJoystickForSecondaryPad(showOutputVirtualPad,
                                                                   showPhysicalSecondPad,
                                                                   effectiveOutputMode),
                             useSecondaryLabels,
                             effectiveOutputMode,
                             showOutputVirtualPad ?
                               (max_devices > 0 ? &controllerFrameConst(0) : nullptr) :
                               (max_devices > secondaryPhysicalPlayer ? &controllerFrameConst(secondaryPhysicalPlayer) : nullptr));
        }
      }
    }

    display.setFont(System5x7);
    lastButtonState[0] = 0;
    lastButtonState[1] = 0;
    lastAnalogL2[0] = 0;
    lastAnalogR2[0] = 0;
    lastAnalogL2[1] = 0;
    lastAnalogR2[1] = 0;
    lastWheelPaddle[0] = 0xFF;
    lastWheelPaddle[1] = 0xFF;
    lastNeGconTwistValid[0] = false;
    lastNeGconTwistValid[1] = false;
    lastTriggerRawDebugL2 = 0xFF;
    lastTriggerRawDebugR2 = 0xFF;
  }

  if (currentLayout[0] == nullptr) {
    return;
  }

  #ifdef ENABLE_INPUT_DRIVING
  if (deviceMode == RZORD_DRIVING || (deviceMode == RZORD_SMS && isDrivingFallbackActive())) {
    static uint8_t lastAxisDisplay[2] = { 0xFF, 0xFF };
    const bool smsDrivingFallback = deviceMode == RZORD_SMS;
    uint8_t visiblePhysicalPlayers = max_devices;
    if (visiblePhysicalPlayers > 1 && !showPhysicalSecondPad) visiblePhysicalPlayers = 1;
    if (visiblePhysicalPlayers > 2) visiblePhysicalPlayers = 2;

    for (uint8_t slot = 0; slot < visiblePhysicalPlayers; ++slot) {
      const uint8_t p=slot==0?primaryPhysicalPlayer:secondaryPhysicalPlayer;
      const controller_state_t* frame = &controllerFrameConst(p);
      if (smsDrivingFallback && std::strcmp(frame->controller_type_name, "Driving") != 0) {
        continue;
      }
      const bool axisChanged = frame->paddle != lastAxisDisplay[p];
      if (axisChanged || homeLayoutNeedsRedraw) {
        lastAxisDisplay[p] = frame->paddle;
        uint8_t colBase = slot==0?primaryPadColOffset:kSecondPadColOffset;
#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
        drawDrivingDial(frame,colBase,kHomePadRowOffset);
#else
        drawWheelAxisGauge(&controllerFrameConst(p),
                           colBase,
                           kHomePadRowOffset,
                           kSpinnerPaddlePanelWidth);
#endif
      }
    }
  }
  #endif

  #ifdef ENABLE_INPUT_JAGUAR
  uint8_t visiblePhysicalPlayers = max_devices;
  if (visiblePhysicalPlayers > 1 && !showPhysicalSecondPad) visiblePhysicalPlayers = 1;
  if (visiblePhysicalPlayers > 2) visiblePhysicalPlayers = 2;
  bool anyVisibleJaguarRotary = false;
  for (uint8_t slot = 0; slot < visiblePhysicalPlayers; ++slot) {
    const uint8_t p=slot==0?primaryPhysicalPlayer:secondaryPhysicalPlayer;
    if (jaguarRotaryActivePorts[p]) {
      anyVisibleJaguarRotary = true;
      break;
    }
  }
  if (deviceMode == RZORD_JAGUAR && anyVisibleJaguarRotary) {
    static uint8_t lastAxisDisplay[2] = { 0xFF, 0xFF };

    for (uint8_t slot = 0; slot < visiblePhysicalPlayers; ++slot) {
      const uint8_t p=slot==0?primaryPhysicalPlayer:secondaryPhysicalPlayer;
      if (!jaguarRotaryActivePorts[p]) {
        continue;
      }
      const controller_state_t* frame = &controllerFrameConst(p);
#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
      const uint8_t phase = jaguarRotaryDisplayPhase[p];
#else
      const uint8_t phase = frame->paddle;
#endif
      const bool axisChanged = phase != lastAxisDisplay[p];
      if (axisChanged || homeLayoutNeedsRedraw) {
        lastAxisDisplay[p] = phase;
        uint8_t colBase = slot==0?primaryPadColOffset:kSecondPadColOffset;
#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
        drawJaguarRotaryDial(phase,colBase,kHomePadRowOffset);
#else
        drawWheelAxisGauge(&controllerFrameConst(p),
                           colBase+3,
                           kHomePadRowOffset,
                           kSpinnerPaddlePanelWidth);
#endif
      }
    }
  }
  #endif

  #if defined(ENABLE_INPUT_PSX) && (!defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB))
  if (deviceMode == RZORD_PSX) {
    // The phase plus a turning bit; 0xFFFF forces the next draw.
    static uint16_t lastTurntable[2] = { 0xFFFF, 0xFFFF };
    uint8_t visiblePlayers = max_devices;
    if (visiblePlayers > 1 && !showPhysicalSecondPad) visiblePlayers = 1;
    if (visiblePlayers > 2) visiblePlayers = 2;
    for (uint8_t slot = 0; slot < visiblePlayers; ++slot) {
      const uint8_t p = slot == 0 ? primaryPhysicalPlayer : secondaryPhysicalPlayer;
      const controller_state_t* frame = &controllerFrameConst(p);
      if (p >= 2 || std::strcmp(frame->controller_type_name, "beatmania") != 0) {
        if (p < 2) lastTurntable[p] = 0xFFFF;
        continue;
      }
      const bool turning = frame->PAD_U != frame->PAD_D;
      const uint16_t shown = uint16_t(psxTurntableDisplayPhase[p] | (turning ? 0x100 : 0));
      if (shown != lastTurntable[p] || homeLayoutNeedsRedraw) {
        lastTurntable[p] = shown;
        const uint8_t colBase = slot == 0 ? primaryPadColOffset : kSecondPadColOffset;
        drawBeatmaniaTurntable(psxTurntableDisplayPhase[p], turning, colBase, kHomePadRowOffset);
      }
    }
  }
  #endif

  uint32_t state0 = buildButtonMaskFromReport(primaryPhysicalPlayer);
  state0 = applyTurboVisualState(primaryPhysicalPlayer, state0);
  uint32_t state1 = 0;
  if (showOutputVirtualPad && !centerOutputVirtualPad) {
    state1 = buildButtonMaskFromOutputState(0);
    state1 = applyTurboVisualState(0, state1);
  } else if (showPhysicalSecondPad) {
    state1 = buildButtonMaskFromReport(secondaryPhysicalPlayer);
    state1 = applyTurboVisualState(secondaryPhysicalPlayer, state1);
  }

  const controller_state_t* primaryAnalogFrame =
    (max_devices > primaryPhysicalPlayer) ? &controllerFrameConst(primaryPhysicalPlayer) : nullptr;
  const controller_state_t* secondaryAnalogFrame = nullptr;
  if (showOutputVirtualPad && !centerOutputVirtualPad) {
    secondaryAnalogFrame = primaryAnalogFrame;
  } else if (showPhysicalSecondPad && max_devices > secondaryPhysicalPlayer) {
    secondaryAnalogFrame = &controllerFrameConst(secondaryPhysicalPlayer);
  }
  if (showHomeAnalogGauge && primaryAnalogFrame != nullptr) {
    homeAnalogGaugeRender(deviceMode, *primaryAnalogFrame, homeLayoutNeedsRedraw);
  } else {
    homeAnalogGaugeReset();
  }
#if defined(ENABLE_INPUT_PSX) && defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  struct FishingDrawState { int16_t x=0, y=0, speed=0; bool valid=false; };
  static FishingDrawState fishingDraw[2];
  for (uint8_t slot=0; slot<2; ++slot) {
    const controller_state_t* frame = slot == 0 ? primaryAnalogFrame : secondaryAnalogFrame;
    const bool visible = currentLayout[slot] == fishing_pad_art::layout && frame && frame->connected &&
                         (slot == 0 || showPhysicalSecondPad);
    if (!visible) { fishingDraw[slot].valid=false; continue; }
    auto& last = fishingDraw[slot];
    if (homeLayoutNeedsRedraw || !last.valid || last.x!=frame->LX || last.y!=frame->LY || last.speed!=frame->RX) {
      drawFishingIndicators(*frame, currentLayout[slot], currentLayoutCount[slot],
                            slot==0 ? primaryPadColOffset : kSecondPadColOffset);
      last = {frame->LX, frame->LY, frame->RX, true};
    }
  }
#endif
#ifdef ENABLE_INPUT_PSX
  // Raw dial movement must refresh even when the USB joystick axis saturates,
  // or Fake Spinner produces no new button transition.
  static uint16_t lastJogconPosition = 0x100;
  const bool showJogconDial = currentLayout[0] == menu_pad_layouts_internal::padLayoutPSXJogcon &&
                             primaryAnalogFrame != nullptr && primaryAnalogFrame->connected;
  const uint8_t jogconPosition = static_cast<uint8_t>(jogcon_counter);
  if (showJogconDial && (homeLayoutNeedsRedraw || lastJogconPosition != jogconPosition)) {
    drawJogconDial(jogconPosition, primaryPadColOffset, kHomePadRowOffset);
    lastJogconPosition = jogconPosition;
  } else if (!showJogconDial) {
    lastJogconPosition = 0x100;
  }
#endif
  #ifdef ENABLE_INPUT_DREAMCAST
  const bool primaryDreamcastWheel =
    deviceMode == RZORD_DREAMCAST && isDreamcastWheelFrame(primaryAnalogFrame);
  const bool secondaryDreamcastWheel =
    deviceMode == RZORD_DREAMCAST && isDreamcastWheelFrame(secondaryAnalogFrame);
  #else
  const bool primaryDreamcastWheel = false;
  const bool secondaryDreamcastWheel = false;
  #endif
  #ifdef ENABLE_INPUT_SATURN
  const bool primarySaturnWheel =
    deviceMode == RZORD_SATURN && isSaturnWheelFrame(primaryAnalogFrame);
  const bool secondarySaturnWheel =
    deviceMode == RZORD_SATURN && isSaturnWheelFrame(secondaryAnalogFrame);
  #else
  const bool primarySaturnWheel = false;
  const bool secondarySaturnWheel = false;
  #endif
  const bool primaryWheelAxisChanged =
    (primaryDreamcastWheel || primarySaturnWheel) &&
    primaryAnalogFrame != nullptr &&
    (homeLayoutNeedsRedraw || primaryAnalogFrame->paddle != lastWheelPaddle[0]);
  const bool secondaryWheelAxisChanged =
    (secondaryDreamcastWheel || secondarySaturnWheel) &&
    secondaryAnalogFrame != nullptr &&
    (homeLayoutNeedsRedraw || secondaryAnalogFrame->paddle != lastWheelPaddle[1]);
  #ifdef ENABLE_INPUT_PSX
  const bool primaryNeGcon =
    deviceMode == RZORD_PSX && isNeGconFrame(primaryAnalogFrame);
  const bool secondaryNeGcon =
    showPhysicalSecondPad &&
    deviceMode == RZORD_PSX &&
    isNeGconFrame(secondaryAnalogFrame);
  #else
  const bool primaryNeGcon = false;
  const bool secondaryNeGcon = false;
  #endif
  const bool primaryNeGconTwistChanged =
    primaryNeGcon &&
    (!lastNeGconTwistValid[0] || primaryAnalogFrame->LX != lastNeGconTwist[0]);
  const bool secondaryNeGconTwistChanged =
    secondaryNeGcon &&
    (!lastNeGconTwistValid[1] || secondaryAnalogFrame->LX != lastNeGconTwist[1]);
  const uint8_t analogL2GaugeWidth[2] = {
    primaryDreamcastWheel
      ? kDreamcastWheelTriggerGaugeWidth
      : analogTriggerGaugeWidthForMask(currentLayout[0], currentLayoutCount[0], GPAD_L2),
    secondaryDreamcastWheel
      ? kDreamcastWheelTriggerGaugeWidth
      : analogTriggerGaugeWidthForMask(currentLayout[1], currentLayoutCount[1], GPAD_L2),
  };
  const uint8_t analogR2GaugeWidth[2] = {
    primaryDreamcastWheel
      ? kDreamcastWheelTriggerGaugeWidth
      : analogTriggerGaugeWidthForMask(currentLayout[0], currentLayoutCount[0], GPAD_R2),
    secondaryDreamcastWheel
      ? kDreamcastWheelTriggerGaugeWidth
      : analogTriggerGaugeWidthForMask(currentLayout[1], currentLayoutCount[1], GPAD_R2),
  };
  const uint8_t analogL2Columns[2] = {
    padTriggerGaugeFill(primaryAnalogFrame, currentLayout[0], GPAD_L2, analogL2GaugeWidth[0]),
    padTriggerGaugeFill(secondaryAnalogFrame, currentLayout[1], GPAD_L2, analogL2GaugeWidth[1]),
  };
  const uint8_t analogR2Columns[2] = {
    padTriggerGaugeFill(primaryAnalogFrame, currentLayout[0], GPAD_R2, analogR2GaugeWidth[0]),
    padTriggerGaugeFill(secondaryAnalogFrame, currentLayout[1], GPAD_R2, analogR2GaugeWidth[1]),
  };
  const bool analogChanged0 =
    analogL2Columns[0] != lastAnalogL2[0] || analogR2Columns[0] != lastAnalogR2[0];
  const bool analogChanged1 =
    analogL2Columns[1] != lastAnalogL2[1] || analogR2Columns[1] != lastAnalogR2[1];
  const bool triggerRawDebugActive =
    deviceMode == RZORD_PSX &&
    primaryAnalogFrame != nullptr &&
    primaryAnalogFrame->connected &&
    primaryAnalogFrame->HAS_ANALOG_TRIGGERS;
  const bool triggerRawDebugChanged =
    triggerRawDebugActive &&
    (primaryAnalogFrame->ANALOG_L2 != lastTriggerRawDebugL2 ||
     primaryAnalogFrame->ANALOG_R2 != lastTriggerRawDebugR2);

  const bool buttonsChanged =
    state0 != lastButtonState[0] || state1 != lastButtonState[1];
  const bool analogChanged = analogChanged0 || analogChanged1;
  const bool wheelAxisChanged = primaryWheelAxisChanged || secondaryWheelAxisChanged;
  const bool neGconTwistChanged =
    primaryNeGconTwistChanged || secondaryNeGconTwistChanged;
  if (!buttonsChanged && !analogChanged && !triggerRawDebugChanged &&
      !wheelAxisChanged && !neGconTwistChanged) {
    return;
  }
  if (!buttonsChanged && !triggerRawDebugChanged &&
      (analogChanged || wheelAxisChanged || neGconTwistChanged)) {
    const uint32_t now = millis();
    if ((uint32_t)(now - lastAnalogGaugeRedrawMs) <
        kAnalogTriggerGaugeRedrawMs) {
      return;
    }
    lastAnalogGaugeRedrawMs = now;
  } else if (analogChanged || wheelAxisChanged || neGconTwistChanged) {
    lastAnalogGaugeRedrawMs = millis();
  }

  display.setFont(ReflexPad5x7);
  const uint8_t primaryColOffset = primaryPadColOffset;
  const bool usePrimaryJoystick = shouldUseArcadeJoystickForPrimaryPad();
  const bool usePrimaryLabels =
    centerOutputVirtualPad &&
    shouldUseCenteredOutputButtonLabels();
  const bool useSecondaryJoystick =
    shouldUseArcadeJoystickForSecondaryPad(showOutputVirtualPad,
                                           showPhysicalSecondPad,
                                           effectiveOutputMode);
  const bool useSecondaryLabels =
    showOutputVirtualPad &&
    shouldUseSideBySideOutputButtonLabels();
  const bool useWheelButtonLabels = menu_home_button_labels != 0;

#ifdef ENABLE_INPUT_PSX
  if (primaryNeGcon && primaryNeGconTwistChanged) {
    drawNeGconTwistGauge(primaryAnalogFrame, primaryColOffset);
  }
  if (showSecondPad && secondaryNeGcon && secondaryNeGconTwistChanged) {
    drawNeGconTwistGauge(secondaryAnalogFrame, kSecondPadColOffset);
  }
#endif

  if (primaryDreamcastWheel && currentLayout[0]) {
    drawDreamcastWheelHome(primaryAnalogFrame, state0, primaryColOffset, kHomePadRowOffset,
                           analogChanged0 || primaryWheelAxisChanged || primaryDreamcastWheel,
                           state0 != lastButtonState[0] || primaryWheelAxisChanged,
                           primaryWheelAxisChanged,
                           useWheelButtonLabels, !showSecondPad);
  }

  if (primarySaturnWheel && currentLayout[0]) {
    drawSaturnWheelHome(primaryAnalogFrame, state0, primaryColOffset, kHomePadRowOffset,
                        state0 != lastButtonState[0] || primaryWheelAxisChanged,
                        state0 != lastButtonState[0] || primaryWheelAxisChanged,
                        primaryWheelAxisChanged,
                        useWheelButtonLabels, !showSecondPad);
  }

  if (!primaryDreamcastWheel &&
      !primarySaturnWheel &&
      (state0 != lastButtonState[0] || analogChanged0) &&
      currentLayout[0]) {
    for (uint8_t i = 0; i < currentLayoutCount[0]; ++i) {
      uint32_t mask = currentLayout[0][i].mask;
      if (usePrimaryJoystick && isJoystickDirectionMask(mask)) {
        continue;
      }
      const bool triggerGaugeChanged =
        (mask == GPAD_L2 && analogL2Columns[0] != lastAnalogL2[0]) ||
        (mask == GPAD_R2 && analogR2Columns[0] != lastAnalogR2[0]);
      if (((state0 & mask) != (lastButtonState[0] & mask)) ||
          triggerGaugeChanged) {
        drawPadSlotGlyph(currentLayout[0][i],
                         currentLayout[0],
                         currentLayoutCount[0],
                         state0,
                         primaryColOffset,
                         kHomePadRowOffset,
                         usePrimaryJoystick,
                         usePrimaryLabels,
                         effectiveOutputMode,
                         primaryAnalogFrame);
      }
    }
    if (usePrimaryJoystick &&
        ((state0 ^ lastButtonState[0]) & kJoystickDirectionMask)) {
      drawJoystickBallForLayout(currentLayout[0],
                                currentLayoutCount[0],
                                state0,
                                primaryColOffset,
                                kHomePadRowOffset);
    } else if (!usePrimaryJoystick) {
      // A glyph write clears its sixth spacing column. Repair the cross after
      // incremental updates as well as initial/full pad rendering.
      drawDpadCentersForLayoutState(currentLayout[0], currentLayoutCount[0],
                                   state0, primaryColOffset, kHomePadRowOffset);
    }
  }

  if (showSecondPad && secondaryDreamcastWheel && currentLayout[1]) {
    drawDreamcastWheelHome(secondaryAnalogFrame, state1, kSecondPadColOffset, kHomePadRowOffset,
                           analogChanged1 || secondaryWheelAxisChanged || secondaryDreamcastWheel,
                           state1 != lastButtonState[1] || secondaryWheelAxisChanged,
                           secondaryWheelAxisChanged,
                           useWheelButtonLabels);
  }

  if (showSecondPad && secondarySaturnWheel && currentLayout[1]) {
    drawSaturnWheelHome(secondaryAnalogFrame, state1, kSecondPadColOffset, kHomePadRowOffset,
                        state1 != lastButtonState[1] || secondaryWheelAxisChanged,
                        state1 != lastButtonState[1] || secondaryWheelAxisChanged,
                        secondaryWheelAxisChanged,
                        useWheelButtonLabels);
  }

  if (showSecondPad &&
      !secondaryDreamcastWheel &&
      !secondarySaturnWheel &&
      (state1 != lastButtonState[1] || analogChanged1) &&
      currentLayout[1]) {
    for (uint8_t i = 0; i < currentLayoutCount[1]; ++i) {
      uint32_t mask = currentLayout[1][i].mask;
      if (useSecondaryJoystick && isJoystickDirectionMask(mask)) {
        continue;
      }
      const bool triggerGaugeChanged =
        (mask == GPAD_L2 && analogL2Columns[1] != lastAnalogL2[1]) ||
        (mask == GPAD_R2 && analogR2Columns[1] != lastAnalogR2[1]);
      if (((state1 & mask) != (lastButtonState[1] & mask)) ||
          triggerGaugeChanged) {
        drawPadSlotGlyph(currentLayout[1][i],
                         currentLayout[1],
                         currentLayoutCount[1],
                         state1,
                         kSecondPadColOffset,
                         kHomePadRowOffset,
                         useSecondaryJoystick,
                         useSecondaryLabels,
                         effectiveOutputMode,
                         secondaryAnalogFrame);
      }
    }
    if (useSecondaryJoystick &&
        ((state1 ^ lastButtonState[1]) & kJoystickDirectionMask)) {
      drawJoystickBallForLayout(currentLayout[1],
                                currentLayoutCount[1],
                                state1,
                                kSecondPadColOffset,
                                kHomePadRowOffset);
    } else if (!useSecondaryJoystick) {
      drawDpadCentersForLayoutState(currentLayout[1], currentLayoutCount[1],
                                   state1, kSecondPadColOffset, kHomePadRowOffset);
    }
  }

  renderPsxTriggerRawDebugLine(primaryAnalogFrame, forceRedraw || triggerRawDebugChanged);

  lastButtonState[0] = state0;
  lastButtonState[1] = state1;
  lastAnalogL2[0] = analogL2Columns[0];
  lastAnalogR2[0] = analogR2Columns[0];
  lastAnalogL2[1] = analogL2Columns[1];
  lastAnalogR2[1] = analogR2Columns[1];
  lastWheelPaddle[0] =
    ((primaryDreamcastWheel || primarySaturnWheel) && primaryAnalogFrame != nullptr)
      ? primaryAnalogFrame->paddle
      : 0xFF;
  lastWheelPaddle[1] =
    ((secondaryDreamcastWheel || secondarySaturnWheel) && secondaryAnalogFrame != nullptr)
      ? secondaryAnalogFrame->paddle
      : 0xFF;
  lastNeGconTwistValid[0] = primaryNeGcon;
  lastNeGconTwistValid[1] = secondaryNeGcon;
  if (primaryNeGcon) {
    lastNeGconTwist[0] = primaryAnalogFrame->LX;
  }
  if (secondaryNeGcon) {
    lastNeGconTwist[1] = secondaryAnalogFrame->LX;
  }

  if (triggerRawDebugActive) {
    lastTriggerRawDebugL2 = primaryAnalogFrame->ANALOG_L2;
    lastTriggerRawDebugR2 = primaryAnalogFrame->ANALOG_R2;
  } else {
    lastTriggerRawDebugL2 = 0xFF;
    lastTriggerRawDebugR2 = 0xFF;
  }

  display.setFont(System5x7);
}

}  // namespace menu_main_display_internal
