#include "../product_config.h"

#include <cstring>

#include "../core/controller_output_cache_state.h"
#include "../core/rumble_test_runtime.h"
#include "../platform/display_runtime_state.h"
#include "analog_test_renderer.h"
#include "menu_helpers.h"
#include "menu_list_navigation.h"
#include "menu_system_screens.h"
#include "quick_config.h"
#include "../core/n64_assignment_runtime.h"
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
#include "../features/usb2db15/usb2db15_arcade_session.h"
#include "menu_usb2db15_tournament.h"
#endif

namespace {
constexpr uint8_t kQuickConfigTextColumns = 21;
constexpr uint8_t kQuickConfigGlyphWidthPx = 6;
constexpr uint8_t kQuickConfigSettingValueStartColumn = 7;  // "Value: "
constexpr uint16_t kQuickConfigMarqueeTickMs = 250;

uint32_t quickConfigLastMarqueeTickMs = 0;
uint32_t quickConfigMarqueeStartMs = 0;
uint16_t quickConfigDisplayedRumbleRampLevel = 0x100u;

bool analogTraceIsGameCube(DeviceEnum mode) {
  #ifdef ENABLE_INPUT_GAMECUBE
  return mode == RZORD_GAMECUBE;
  #else
  (void)mode;
  return false;
  #endif
}

bool quickConfigMarqueeNeedsTick(QuickConfigState state) {
  return state == QC_MAIN_MENU || state == QC_SETTING_EDIT || state == QC_ANALOG_SUBMENU || state == QC_HOTKEYS_SUBMENU;
}

uint8_t quickConfigVisibleStartIndex(uint8_t selected, uint8_t count, uint8_t visibleRows) {
  return menu_list_navigation::firstVisible(selected, count, visibleRows);
}

uint8_t quickConfigValueStartColumn(const char* name, bool submenu) {
  if (submenu) {
    return kQuickConfigTextColumns;
  }

  const uint8_t nameColumns = (uint8_t)strlen(name ? name : "");
  const uint8_t startColumn = (uint8_t)(1 + nameColumns + 2);  // selector + ": "
  return startColumn < kQuickConfigTextColumns ? startColumn : kQuickConfigTextColumns;
}

uint8_t quickConfigColumnPixel(uint8_t column) {
  const uint16_t pixel = (uint16_t)column * kQuickConfigGlyphWidthPx;
  return pixel < 128 ? (uint8_t)pixel : 127;
}

void renderQuickConfigValueMarquee(SSD1306AsciiWire& display, const char* value,
                                   uint8_t startColumn, bool selected) {
  if (startColumn >= kQuickConfigTextColumns) {
    return;
  }
  printOledMarqueeText(display, value ? value : "",
                       (uint8_t)(kQuickConfigTextColumns - startColumn),
                       selected, quickConfigMarqueeStartMs);
}

void clearQuickConfigValueField(SSD1306AsciiWire& display, uint8_t row,
                                const char* name, bool submenu) {
  const uint8_t startColumn = quickConfigValueStartColumn(name, submenu);
  if (startColumn >= kQuickConfigTextColumns) {
    return;
  }

  const uint8_t startPixel = quickConfigColumnPixel(startColumn);
  display.clear(startPixel, 127, row, row);
  display.setCursor(startPixel, row);
}

void renderQuickConfigOptionRow(SSD1306AsciiWire& display, const char* name,
                                const char* value, bool submenu, bool selected) {
  const char* safeName = name ? name : "";

  display.print(selected ? F(">") : F(" "));
  if (submenu) {
    display.print(safeName);
    display.print(F(" >"));
  } else {
    display.print(safeName);
    display.print(F(": "));
    renderQuickConfigValueMarquee(display, value, quickConfigValueStartColumn(safeName, false), selected);
  }

  display.println();
}
}

void QuickConfigMenu::render(SSD1306AsciiWire& display) {
  if (state == QC_RANGE_TEST) {
    renderRangeTest(display);
    return;
  }

  if (state == QC_STICK_CAL) {
    renderStickCal(display);
    return;
  }

  if (state == QC_REMAP_SELECT) {
    remapMenu.render(deviceMode);
    return;
  }

  if (state == QC_RUMBLE_SUBMENU) {
    uint8_t rampLevel = 0;
    const uint16_t displayedLevel = rumbleTestGetHeavyRampLevel(&rampLevel)
      ? rampLevel
      : 0x100u;
    if (displayedLevel != quickConfigDisplayedRumbleRampLevel) {
      quickConfigDisplayedRumbleRampLevel = displayedLevel;
      needs_redraw = true;
    }
  }

  if (!needs_redraw && quickConfigMarqueeNeedsTick(state)) {
    const uint32_t now = millis();
    if ((uint32_t)(now - quickConfigLastMarqueeTickMs) >= kQuickConfigMarqueeTickMs) {
      quickConfigLastMarqueeTickMs = now;
      if (renderMarqueeTick(display)) {
        return;
      }
    }
  }

  if (!needs_redraw) return;
  needs_redraw = false;
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  if (state == QC_USB2DB15_TOURNAMENT) {
    display.setFont(System5x7);
    drawUsb2db15TournamentReview(display, menu_kiosk_mode != 0,
                                usb2db15FourWayGame(), false);
    return;
  }
#endif
  const uint32_t now = millis();
  quickConfigLastMarqueeTickMs = now;
  quickConfigMarqueeStartMs = now;

  display.clear();
  display.setFont(System5x7);
  display.set1X();

  switch (state) {
    case QC_MAIN_MENU:
      renderMainMenu(display);
      break;

    case QC_CONFIRM_DEFAULT:
      renderDefaultConfirm(display);
      break;

    case QC_CONFIRM_DINPUT_RESET:
      renderDinputResetConfirm(display);
      break;

    case QC_CONFIRM_NSO_1P:
      renderNsoSinglePlayerConfirm(display);
      break;

    case QC_TURBO_LIST:
      renderTurboList(display);
      break;

    case QC_TURBO_RATE:
      renderTurboRate(display);
      break;

    case QC_SETTING_EDIT:
      renderSettingEdit(display);
      break;
    case QC_MODE_PICKER:
      renderModePicker(display);
      break;
    case QC_CHOICE_PICKER:
      renderChoicePicker(display);
      break;

    case QC_ANALOG_SUBMENU:
      renderAnalogSubmenu(display);
      break;
    case QC_HOTKEYS_SUBMENU:
      renderHotkeysSubmenu(display);
      break;

    case QC_DUAL_MERGE_MAP:
      renderDualMergeMap(display);
      break;

    case QC_RUMBLE_SUBMENU:
      renderRumbleSubmenu(display);
      break;

#ifdef ENABLE_INPUT_PSX
    case QC_GUNCON_ADJUST:
      renderGunconAdjust(display);
      break;
#endif

    case QC_REMAP_LIST:
      renderRemapList(display);
      break;

    default:
      break;
  }
}

bool QuickConfigMenu::renderMarqueeTick(SSD1306AsciiWire& display) {
  display.setFont(System5x7);
  display.set1X();

  switch (state) {
    case QC_MAIN_MENU: {
      if (on_bottom_row || selected_index >= visible_count) {
        return false;
      }

      const uint8_t visible_rows = 5;
      const uint8_t start_index = quickConfigVisibleStartIndex(selected_index, visible_count, visible_rows);
      if (selected_index < start_index || selected_index >= start_index + visible_rows) {
        return false;
      }

      const uint8_t row = (uint8_t)(1 + selected_index - start_index);
      const QuickConfigItem item = visible_items[selected_index];
      clearQuickConfigValueField(display, row, getItemName(item), isSubmenu(item));
      renderQuickConfigValueMarquee(
        display,
        getItemValue(item),
        quickConfigValueStartColumn(getItemName(item), isSubmenu(item)),
        true
      );
      return true;
    }

    case QC_SETTING_EDIT: {
      const uint8_t startPixel = quickConfigColumnPixel(kQuickConfigSettingValueStartColumn);
      display.clear(startPixel, 127, 2, 2);
      display.setCursor(startPixel, 2);
      renderQuickConfigValueMarquee(
        display,
        getItemValue(editing_item),
        kQuickConfigSettingValueStartColumn,
        true
      );
      return true;
    }

    case QC_ANALOG_SUBMENU: {
      buildAnalogItems();
      if (analog_index >= analog_count) {
        return false;
      }

      const uint8_t visible_rows = 6;
      const uint8_t start_index = quickConfigVisibleStartIndex(analog_index, analog_count, visible_rows);
      if (analog_index < start_index || analog_index >= start_index + visible_rows) {
        return false;
      }

      const uint8_t row = (uint8_t)(1 + analog_index - start_index);
      const QuickConfigItem item = analog_items[analog_index];
      clearQuickConfigValueField(display, row, getItemName(item), isSubmenu(item));
      renderQuickConfigValueMarquee(
        display,
        getItemValue(item),
        quickConfigValueStartColumn(getItemName(item), isSubmenu(item)),
        true
      );
      return true;
    }

    case QC_HOTKEYS_SUBMENU: {
      buildHotkeyItems();
      if (hotkey_index >= hotkey_count) {
        return false;
      }

      const uint8_t visible_rows = 6;
      const uint8_t start_index = quickConfigVisibleStartIndex(hotkey_index, hotkey_count, visible_rows);
      if (hotkey_index < start_index || hotkey_index >= start_index + visible_rows) {
        return false;
      }

      const uint8_t row = (uint8_t)(1 + hotkey_index - start_index);
      const QuickConfigItem item = hotkey_items[hotkey_index];
      clearQuickConfigValueField(display, row, getItemName(item), isSubmenu(item));
      renderQuickConfigValueMarquee(
        display,
        getItemValue(item),
        quickConfigValueStartColumn(getItemName(item), isSubmenu(item)),
        true
      );
      return true;
    }

    default:
      return false;
  }
}

void QuickConfigMenu::renderMainMenu(SSD1306AsciiWire& display) {
#if ADAPT_POLICY_QUICK_SELECTED_CONTROLLER
  const uint8_t player = selectedControllerPlayer();
  const controller_state_t& frame = selectedControllerFrame();
  char title[32];
  snprintf(title, sizeof(title), "P%u %.16s", player + 1,
           frame.controller_type_name[0] ? frame.controller_type_name : "Controller");
#else
  const char* modeName = activeInputAdapterDescriptionOr("Mode");
  char title[32];
  snprintf(title, sizeof(title), "%s Settings", modeName);
#endif

  printCenteredMenuTitle(display, title);

  uint8_t visible_rows = 5;
  const uint8_t start_index = quickConfigVisibleStartIndex(
      on_bottom_row ? visible_count : selected_index, visible_count, visible_rows);

  for (uint8_t row = 0; row < visible_rows && (start_index + row) < visible_count; row++) {
    uint8_t idx = start_index + row;
    QuickConfigItem item = visible_items[idx];
    bool is_selected = !on_bottom_row && (idx == selected_index);

    renderQuickConfigOptionRow(display, getItemName(item), getItemValue(item),
                               isSubmenu(item), is_selected);
  }

  display.setCursor(0, 6);
  display.clearToEOL();
#if ADAPT_POLICY_QUICK_SELECTED_CONTROLLER
  char scopeHint[22];
  snprintf(scopeHint, sizeof(scopeHint), "Shared; P%u tools",
           selectedControllerPlayer() + 1);
  display.print(scopeHint);
#endif

  static const char* const kFooter[] = { "Cancel", "Default", "Save" };
  renderMenuButtonRow(7, kFooter, 3, on_bottom_row ? static_cast<int8_t>(bottom_index) : -1);
}

void QuickConfigMenu::renderDefaultConfirm(SSD1306AsciiWire& display) {
  (void)display;
  drawModeDefaultsPrompt(default_confirm_index);
}

void QuickConfigMenu::renderDinputResetConfirm(SSD1306AsciiWire& display) {
  display.println(F("Remap DInput pad?"));
  display.println();
  display.println(F("Selected USB pad"));
  display.println(F("will be relearned."));
  display.println();
  display.println(F("Other maps stay saved."));

  static const char* const kChoices[] = { "Cancel", "Remap" };
  renderMenuButtonRow(7, kChoices, 2, static_cast<int8_t>(dinput_reset_confirm_index));
}

void QuickConfigMenu::renderNsoSinglePlayerConfirm(SSD1306AsciiWire& display) {
  display.print(F("Use "));
  display.print(switch_device_selection_label(temp_input_mode, temp_nso_special));
  display.println(F("?"));
  display.println();
  display.println(F("Other pads removed"));
  display.println(F("after restart."));
  display.println();
#if ADAPT_POLICY_HID_OUTPUT_SLOT_CAP > 2
  display.println(F("Pro mode keeps 4P."));
#else
  display.println(F("Pro mode keeps 2P."));
#endif

  static const char* const kChoices[] = { "Cancel", "Enable" };
  renderMenuButtonRow(7, kChoices, 2, static_cast<int8_t>(nso_1p_confirm_index));
}

void QuickConfigMenu::renderTurboList(SSD1306AsciiWire& display) {
  const uint8_t turboCount = getVisibleTurboCount();

  printCenteredMenuTitle(display, "Turbo Config");

  uint8_t visible_rows = 6;
  const uint8_t start_index = quickConfigVisibleStartIndex(turbo_index, turboCount, visible_rows);

  for (uint8_t row = 0; row < visible_rows && (start_index + row) < turboCount; row++) {
    uint8_t item = start_index + row;
    bool is_selected = (item == turbo_index);

    display.print(is_selected ? F(">") : F(" "));

    uint8_t btn_index = getTurboButtonIndexForVisibleIndex(item);
    display.print(getTurboButtonNameForVisibleIndex(item));
    display.print(F(": "));
    display.println((btn_index == 0xFF) ? "?" : getTurboRateName(temp_rates[btn_index]));
  }

  display.setCursor(0, 7);
  printMenuButton(display, "Back", (turbo_index == turboCount));
}

void QuickConfigMenu::renderTurboRate(SSD1306AsciiWire& display) {
  char title[32];
  snprintf(title, sizeof(title), "%s Turbo",
           getTurboButtonNameForVisibleIndex(turbo_index));
  printCenteredMenuTitle(display, title);

  const uint8_t visible_rows = 6;
  const uint8_t start_index = quickConfigVisibleStartIndex(turbo_rate_selection, TURBO_RATE_LAST, visible_rows);

  for (uint8_t row = 0; row < visible_rows && (start_index + row) < TURBO_RATE_LAST; row++) {
    uint8_t r = start_index + row;
    display.print(r == turbo_rate_selection ? F(">") : F(" "));
    display.print(r);
    display.print(F(": "));
    display.println(getTurboRateFullName(getTurboRateForMenuIndex(r)));
  }

  display.setCursor(0, 7);
  printMenuButton(display, "Back", (turbo_rate_selection >= TURBO_RATE_LAST));
}

void QuickConfigMenu::renderSettingEdit(SSD1306AsciiWire& display) {
  printCenteredMenuTitle(display, getItemName(editing_item));
  display.println();
  display.print(F("Value: "));
  renderQuickConfigValueMarquee(
    display,
    getItemValue(editing_item),
    kQuickConfigSettingValueStartColumn,
    true
  );
  display.println();
  display.println();
  display.println(F("L/R = Cycle value"));
  display.println(F("A = Confirm"));

  display.setCursor(0, 7);
  printMenuButton(display, "Back", false);
}

void QuickConfigMenu::renderAnalogSubmenu(SSD1306AsciiWire& display) {
  buildAnalogItems();
  if (analog_index > analog_count) {
    analog_index = analog_count;
  }

  if (mousePageActive()) {
    printCenteredMenuTitle(display, "Buttons");
  } else {
    printCenteredMenuTitle(display, "Analog");
  }

  uint8_t visible_rows = 6;
  uint8_t start_index = quickConfigVisibleStartIndex(analog_index, analog_count, visible_rows);

  for (uint8_t row = 0; row < visible_rows && (start_index + row) < analog_count; row++) {
    uint8_t idx = start_index + row;
    QuickConfigItem item = analog_items[idx];
    renderQuickConfigOptionRow(display, getItemName(item), getItemValue(item),
                               isSubmenu(item), idx == analog_index);
  }

  display.setCursor(0, 7);
  printMenuButton(display, "Back", (analog_index >= analog_count));
}

void QuickConfigMenu::renderHotkeysSubmenu(SSD1306AsciiWire& display) {
  buildHotkeyItems();
  if (hotkey_index > hotkey_count) {
    hotkey_index = hotkey_count;
  }

  printCenteredMenuTitle(display, "Hotkeys");

  uint8_t visible_rows = 6;
  uint8_t start_index = quickConfigVisibleStartIndex(hotkey_index, hotkey_count, visible_rows);

  for (uint8_t row = 0; row < visible_rows && (start_index + row) < hotkey_count; row++) {
    uint8_t idx = start_index + row;
    QuickConfigItem item = hotkey_items[idx];
    renderQuickConfigOptionRow(display, getItemName(item), getItemValue(item),
                               isSubmenu(item), idx == hotkey_index);
  }

  display.setCursor(0, 7);
  printMenuButton(display, "Back", (hotkey_index >= hotkey_count));
}

void QuickConfigMenu::renderDualMergeMap(SSD1306AsciiWire& display) {
  const uint8_t buttonCount = getClassicDualMergeButtonCount(temp_input_mode);
  printCenteredMenuTitle(display, "2P Merge");

  const uint8_t visible_rows = 6;
  const uint8_t start_index = quickConfigVisibleStartIndex(dual_merge_index, getDualMergeMenuCount(), visible_rows);

  for (uint8_t row = 0; row < visible_rows && (start_index + row) < getDualMergeMenuCount(); ++row) {
    const uint8_t item = start_index + row;
    display.print(item == dual_merge_index ? F(">") : F(" "));
    if (item == 0) {
      display.print(F("Enabled: "));
      display.println(temp_classic_dual_merge ? F("On") : F("Off"));
      continue;
    }

    const uint8_t buttonIndex = item - 1;
    const uint32_t mask = getClassicDualMergeButtonMask(temp_input_mode, buttonIndex);
    display.print(getClassicDualMergeButtonName(temp_input_mode, buttonIndex));
    display.print(F(": P"));
    display.println((temp_classic_dual_merge_p2_mask & mask) ? 2 : 1);
  }

  display.setCursor(0, 7);
  printMenuButton(display, "Back", (dual_merge_index > buttonCount));
}

void QuickConfigMenu::renderRumbleSubmenu(SSD1306AsciiWire& display) {
  const char* levelNames[] = {"Off", "Variable", "Max"};
  const uint8_t motors = rumbleMotorCount();
  printCenteredMenuTitle(display, "Rumble");

  for (uint8_t i = 0; i < QCI_RUMBLE_BACK; ++i) {
    RumbleMenuItem item = (RumbleMenuItem)i;
    if (!isRumbleMenuItemVisible(item)) {
      continue;
    }
    display.print(i == rumble_index ? F(">") : F(" "));
    switch (item) {
      case QCI_RUMBLE_LEVEL:
        if (hasVariableRumbleStrength()) {
          display.print(F("Level: "));
          display.println(levelNames[temp_rumble <= RUMBLE_MODE_MAX ? temp_rumble : RUMBLE_MODE_VARIABLE]);
        } else {
          display.print(F("Enabled: "));
          display.println(temp_rumble == 0 ? F("Off") : F("On"));
        }
        break;
      case QCI_RUMBLE_TEST_HEAVY:
        display.println(F("Test Heavy"));
        break;
      case QCI_RUMBLE_TEST_LIGHT:
        display.println(F("Test Light"));
        break;
      case QCI_RUMBLE_TEST_BOTH:
        display.println(motors <= 1 ? F("Test Motor") : F("Test Both"));
        break;
      case QCI_RUMBLE_RAMP_HEAVY:
        display.print(motors <= 1 ? F("Motor Ramp: ") : F("Heavy Ramp: "));
        {
          uint8_t rampLevel = 0;
          if (rumbleTestGetHeavyRampLevel(&rampLevel)) {
            display.println(rampLevel);
          } else {
            display.println(F("0-255"));
          }
        }
        break;
      default:
        break;
    }
  }

  display.setCursor(0, 7);
  printMenuButton(display, "Back", (rumble_index == QCI_RUMBLE_BACK));
}

void QuickConfigMenu::renderGunconAdjust(SSD1306AsciiWire& display) {
  printCenteredMenuTitle(display, "GunCon Alignment");
  display.println();

  display.print(guncon_edit_axis == 0 ? F("> ") : F("  "));
  display.print(F("X Alignment: "));
  display.println(temp_guncon_x);

  display.print(guncon_edit_axis == 1 ? F("> ") : F("  "));
  display.print(F("Y Alignment: "));
  display.println(temp_guncon_y);

  display.println();
  display.println(F("L/R = Adjust"));

  display.setCursor(0, 7);
  printMenuButton(display, "Done", guncon_edit_axis == 2);
}

void QuickConfigMenu::renderRangeTest(SSD1306AsciiWire& display) {
  (void)display;
  const uint8_t frameCount =
    max_devices < MAX_USB_OUT ? max_devices : MAX_USB_OUT;
  AnalogDiagnosticTarget target = analogDiagnosticTargetForPort(
    deviceMode, controller_frames, frameCount,
    range_test_target.port, range_test_target.stick);
  if (!analogDiagnosticTargetIsValid(target)) {
    target = analogDiagnosticDefaultTarget(
      deviceMode, controller_frames, frameCount);
  }
  if (!analogDiagnosticTargetIsValid(target)) {
    state = return_to_analog_submenu ? QC_ANALOG_SUBMENU : QC_MAIN_MENU;
    needs_redraw = true;
    return;
  }

  const controller_state_t& frame = controllerFrameConst(target.port);
  const bool octagonal = target.kind == AnalogDiagnosticKind::Stick &&
    analogTraceUsesOctagonalGate(deviceMode, frame.controller_type_name);
  const bool contextChanged =
    range_trace_mode != deviceMode ||
    range_test_target.port != target.port ||
    range_test_target.kind != target.kind ||
    range_trace_octagonal != octagonal ||
    strncmp(range_trace_type, frame.controller_type_name,
            sizeof(range_trace_type)) != 0;
  if (contextChanged) {
    range_trace[0].reset();
    range_trace[1].reset();
    range_trace_mode = deviceMode;
    range_trace_octagonal = octagonal;
    strncpy(range_trace_type, frame.controller_type_name,
            sizeof(range_trace_type) - 1);
    range_trace_type[sizeof(range_trace_type) - 1] = '\0';
    range_trace_dirty = true;
    range_last_draw_ms = 0;
    range_last_lx = range_last_ly = range_last_rx = range_last_ry = 0;
  }
  range_test_target = target;
  range_test_right_stick =
    target.stick == AnalogDiagnosticStick::Aux;

  uint8_t targetCount = 0;
  for (uint8_t port = 0; port < frameCount; ++port) {
    if (analogDiagnosticTargetIsValid(
          analogDiagnosticTargetForPort(
            deviceMode, controller_frames, frameCount, port))) {
      ++targetCount;
    }
  }
  const bool multiplePorts = targetCount > 1;

  if (target.kind != AnalogDiagnosticKind::Stick) {
    AnalogTestValue values[5] = {};
    uint8_t valueCount = 0;
    const char* title = "Analog Test";

    switch (target.kind) {
      case AnalogDiagnosticKind::Wheel:
        title = "Wheel Test";
        values[valueCount++] = { "Raw", frame.paddle };
        values[valueCount++] = {
          "Centered", (int16_t)((int16_t)frame.paddle - 0x80)
        };
        values[valueCount++] = { "Axis", frame.LX };
        break;
      case AnalogDiagnosticKind::NeGcon:
        title = "neGcon Test";
        values[valueCount++] = { "Twist", frame.LX };
        values[valueCount++] = { "I", frame.ANALOG_A };
        values[valueCount++] = { "II", frame.ANALOG_X };
        values[valueCount++] = { "L", frame.ANALOG_L1 };
        break;
      case AnalogDiagnosticKind::Paddle:
        title = "Paddle Test";
        values[valueCount++] = { "Position", frame.LX };
        values[valueCount++] = {
          "Button", (int16_t)(frame.A ? 1 : 0)
        };
        break;
      default:
        return;
    }

    range_trace_dirty = true;
    const uint32_t now = millis();
    if (range_trace_dirty &&
        (range_last_draw_ms == 0 ||
         (uint32_t)(now - range_last_draw_ms) >=
           ANALOG_TEST_FRAME_INTERVAL_MS)) {
      renderAnalogValueTestScreen(
        title, target.port, values, valueCount, multiplePorts);
      range_trace_dirty = false;
      range_last_draw_ms = now;
      needs_redraw = false;
    }
    return;
  }

  RawAnalogInputSnapshot rawSnapshot{};
  const bool hasRawSnapshot =
    getRawAnalogInputSnapshot(deviceMode, target.port, rawSnapshot);
  ClassicAnalogRangeSnapshot rangeSnapshot{};
  const bool hasRangeSnapshot =
    getClassicAnalogRangeSnapshot(
      deviceMode, target.port, rangeSnapshot);
  const bool hasMainStick =
    (hasRawSnapshot ? rawSnapshot.has_main_stick
                    : frame.HAS_ANALOG_STICK_MAIN) ||
    deviceMode == RZORD_GAMEPORT;
  const bool hasAuxStick =
    hasRawSnapshot ? rawSnapshot.has_aux_stick
                   : frame.HAS_ANALOG_STICK_AUX;
  const int16_t raw_lx = (hasRangeSnapshot && rangeSnapshot.valid[CLASSIC_ANALOG_AXIS_LX])
                           ? rangeSnapshot.raw[CLASSIC_ANALOG_AXIS_LX]
                           : (hasRawSnapshot ? rawSnapshot.lx : 0);
  const int16_t raw_ly = (hasRangeSnapshot && rangeSnapshot.valid[CLASSIC_ANALOG_AXIS_LY])
                           ? rangeSnapshot.raw[CLASSIC_ANALOG_AXIS_LY]
                           : (hasRawSnapshot ? rawSnapshot.ly : 0);
  const int16_t raw_rx = (hasRangeSnapshot && rangeSnapshot.valid[CLASSIC_ANALOG_AXIS_RX])
                           ? rangeSnapshot.raw[CLASSIC_ANALOG_AXIS_RX]
                           : (hasRawSnapshot ? rawSnapshot.rx : 0);
  const int16_t raw_ry = (hasRangeSnapshot && rangeSnapshot.valid[CLASSIC_ANALOG_AXIS_RY])
                           ? rangeSnapshot.raw[CLASSIC_ANALOG_AXIS_RY]
                           : (hasRawSnapshot ? rawSnapshot.ry : 0);
  if (range_test_target.stick == AnalogDiagnosticStick::Aux &&
      !hasAuxStick) {
    range_test_target = analogDiagnosticTargetForPort(
      deviceMode, controller_frames, frameCount, target.port,
      AnalogDiagnosticStick::Main);
    range_test_right_stick = false;
  }

  const analog_stick_precision precision =
    hasRawSnapshot ? rawSnapshot.precision
                   : frame.sticks_precision_bits;
  const int16_t fullScale = analogTraceFullScale(precision);
  const int16_t centerThreshold = analogTraceCenterThreshold(fullScale);
  if (hasMainStick) {
    range_trace_dirty =
      range_trace[0].sample(
        raw_lx, raw_ly, octagonal, centerThreshold, fullScale) ||
      range_trace_dirty;
  }
  if (hasAuxStick) {
    range_trace_dirty =
      range_trace[1].sample(
        raw_rx, raw_ry, octagonal, centerThreshold, fullScale) ||
      range_trace_dirty;
  }
  range_trace_dirty =
    range_trace_dirty || needs_redraw ||
    raw_lx != range_last_lx || raw_ly != range_last_ly ||
    raw_rx != range_last_rx || raw_ry != range_last_ry;

  const uint32_t now = millis();
  if (range_trace_dirty &&
      (range_last_draw_ms == 0 ||
       (uint32_t)(now - range_last_draw_ms) >=
         ANALOG_TEST_FRAME_INTERVAL_MS)) {
    const uint8_t stickIndex =
      range_test_target.stick == AnalogDiagnosticStick::Aux ? 1 : 0;
    const AnalogStickTraceScreen screen{
      &range_trace[stickIndex],
      octagonal,
      stickIndex == 0 ? raw_lx : raw_rx,
      stickIndex == 0 ? raw_ly : raw_ry,
      precision,
      frame.connected,
      stickIndex == 1,
      analogTraceIsGameCube(deviceMode),
      target.port,
      multiplePorts,
      hasMainStick && hasAuxStick,
    };
    renderAnalogStickTraceScreen(screen);
    needs_redraw = false;
    range_trace_dirty = false;
    range_last_draw_ms = now;
  }

  range_last_lx = raw_lx;
  range_last_ly = raw_ly;
  range_last_rx = raw_rx;
  range_last_ry = raw_ry;
}

void QuickConfigMenu::renderStickCal(SSD1306AsciiWire& display) {
  const controller_state_t& frame = selectedControllerFrame();
  int8_t lx = (int8_t)frame.LX;
  int8_t ly = (int8_t)frame.LY;
  int8_t rx = (int8_t)frame.RX;
  int8_t ry = (int8_t)frame.RY;

  uint8_t stick_count = getStickCount(deviceMode);

  if (lx < cal_lx_min) { cal_lx_min = lx; cal_has_data = true; }
  if (lx > cal_lx_max) { cal_lx_max = lx; cal_has_data = true; }
  if (ly < cal_ly_min) { cal_ly_min = ly; cal_has_data = true; }
  if (ly > cal_ly_max) { cal_ly_max = ly; cal_has_data = true; }
  if (stick_count >= 2) {
    if (rx < cal_rx_min) { cal_rx_min = rx; cal_has_data = true; }
    if (rx > cal_rx_max) { cal_rx_max = rx; cal_has_data = true; }
    if (ry < cal_ry_min) { cal_ry_min = ry; cal_has_data = true; }
    if (ry > cal_ry_max) { cal_ry_max = ry; cal_has_data = true; }
  }

  if (needs_redraw) {
    display.clear();
    display.setFont(System5x7);
    display.set1X();
    printCenteredMenuTitle(display, "Calibrate Stick");
    display.println(F("Move sticks to edges"));
    needs_redraw = false;
  }

  char buf[22];
  display.setCursor(0, 2);
  snprintf(buf, sizeof(buf), "L:%4d/%4d %4d/%4d", cal_lx_min, cal_lx_max, cal_ly_min, cal_ly_max);
  display.print(buf);

  display.setCursor(0, 3);
  if (stick_count >= 2) {
    snprintf(buf, sizeof(buf), "R:%4d/%4d %4d/%4d", cal_rx_min, cal_rx_max, cal_ry_min, cal_ry_max);
    display.print(buf);
  } else {
    display.print(F("                    "));
  }

  display.setCursor(0, 7);
  display.print(cal_selection == 0 ? F(">") : F(" "));
  display.print(F("Cancel"));

  display.setCursor(48, 7);
  display.print(cal_selection == 1 ? F(">") : F(" "));
  display.print(F("Save"));

  display.setCursor(84, 7);
  display.print(cal_selection == 2 ? F(">") : F(" "));
  display.print(F("Clear"));
}

const char* QuickConfigMenu::remapRowTarget(uint8_t index) {
  const uint8_t source_slot = getRemapButtonSlot(temp_input_mode, index);
  const uint8_t target_slot = (source_slot != 0xFF) ? transaction.remaps()[source_slot] : index;
  const auto assignOutput = temp_output_mode == OUTPUT_AUTO ? get_effective_output_mode() : temp_output_mode;
  return remapTargetName(temp_input_mode, assignOutput, transaction.remaps(), index,
      target_slot, temp_n64_cstick,
      assignOutput == OUTPUT_SWITCHPRO && temp_nso_special != SWITCH_DEVICE_PRO,
      effectivePositionButtonMap(temp_input_mode, assignOutput,
          isNsoSpecialActiveForSelection(temp_input_mode), temp_btn_map));
}

void QuickConfigMenu::renderRemapList(SSD1306AsciiWire& display) {
  printCenteredMenuTitle(display, "Input > Output");

  uint8_t btn_count = getRemapButtonCount(temp_input_mode);
  uint8_t visible_rows = 6;
  const uint8_t start_index = quickConfigVisibleStartIndex(remap_index, btn_count, visible_rows);

  for (uint8_t row = 0; row < visible_rows && (start_index + row) < btn_count; row++) {
    uint8_t item = start_index + row;
    bool is_selected = (item == remap_index);

    display.print(is_selected ? F(">") : F(" "));

    const uint8_t source_slot = getRemapButtonSlot(temp_input_mode, item);
    const char* name = getRemapButtonName(temp_input_mode, item);
    display.print(name);
#ifdef ENABLE_INPUT_JVS
    // JVS rows read cabinet button:host button, for example "P1:X".
    display.print(temp_input_mode == RZORD_JVS ? F(":") : F("->"));
#else
    display.print(F("->"));
#endif
    display.print(remapRowTarget(item));
    if (source_slot != 0xFF && transaction.remaps()[source_slot] != source_slot) {
      display.print(F("!"));
    }
    display.println();
  }

  static const char* const kRemapButtons[] = { "Back", "Default" };
  renderMenuButtonRow(7, kRemapButtons, 2,
      remap_index >= btn_count ? static_cast<int8_t>(remap_index - btn_count) : -1);
}
