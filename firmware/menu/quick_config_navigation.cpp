#include "../product_config.h"

#include "quick_config.h"
#include "../core/n64_assignment_runtime.h"

void QuickConfigMenu::navigateDown() {
  needs_redraw = true;
  const uint8_t turboCount = getVisibleTurboCount();
  const uint8_t dualMergeCount = getDualMergeMenuCount();

  switch (state) {
    case QC_MAIN_MENU:
      if (visible_count == 0) {
        on_bottom_row = true;
        bottom_index = 0;
        bottom_right = false;
        break;
      }
      if (on_bottom_row) {
        if (bottom_index < 2) {
          bottom_index++;
          bottom_right = bottom_index == 2;
        } else {
          on_bottom_row = false;
          bottom_index = 0;
          bottom_right = false;
          selected_index = 0;
        }
      } else {
        selected_index++;
        if (selected_index >= visible_count) {
          on_bottom_row = true;
          bottom_index = 0;
          bottom_right = false;
        }
      }
      break;

    case QC_CONFIRM_DEFAULT:
      default_confirm_index = !default_confirm_index;
      break;

    case QC_CONFIRM_DINPUT_RESET:
      dinput_reset_confirm_index = !dinput_reset_confirm_index;
      break;

    case QC_CONFIRM_NSO_1P:
      nso_1p_confirm_index = !nso_1p_confirm_index;
      break;

    case QC_TURBO_LIST:
      turbo_index++;
      if (turbo_index > turboCount) {
        turbo_index = 0;
      }
      break;

    case QC_TURBO_RATE:
      turbo_rate_selection++;
      if (turbo_rate_selection > TURBO_RATE_LAST) {
        turbo_rate_selection = 0;
      }
      break;

    case QC_SETTING_EDIT:
      cycleSettingValue();
      break;

    case QC_MODE_PICKER:
      mode_picker_index = (mode_picker_index + 1) % (modePickerCount() + 1);
      break;

    case QC_CHOICE_PICKER:
      choice_picker_index = (choice_picker_index + 1) % (choicePickerCount() + 1);
      break;

    case QC_ANALOG_SUBMENU:
      buildAnalogItems();
      analog_index++;
      if (analog_index > analog_count) {
        analog_index = 0;
      }
      break;

    case QC_DUAL_MERGE_MAP:
      dual_merge_index++;
      if (dual_merge_index > dualMergeCount) {
        dual_merge_index = 0;
      }
      break;

    case QC_RUMBLE_SUBMENU:
      advanceRumbleIndex(true);
      break;
    case QC_HOTKEYS_SUBMENU:
      buildHotkeyItems();
      hotkey_index = hotkey_index >= hotkey_count ? 0 : hotkey_index + 1;
      break;

#ifdef ENABLE_INPUT_PSX
    case QC_GUNCON_ADJUST:
      handleGunconNavigate();
      break;
#endif

    case QC_REMAP_LIST: {
      uint8_t btn_count = getRemapButtonCount(temp_input_mode);
      remap_index++;
      if (remap_index > btn_count + 1) {
        remap_index = 0;
      }
      break;
    }

    case QC_RANGE_TEST:
    {
      const uint8_t frameCount =
        max_devices < MAX_USB_OUT ? max_devices : MAX_USB_OUT;
      const AnalogDiagnosticTarget next =
        analogDiagnosticNextPortTarget(
          deviceMode, controller_frames, frameCount,
          range_test_target, 1);
      if (analogDiagnosticTargetIsValid(next) &&
          next.port != range_test_target.port) {
        range_test_target = next;
        range_test_right_stick =
          next.stick == AnalogDiagnosticStick::Aux;
        range_trace[0].reset();
        range_trace[1].reset();
        range_trace_type[0] = '\0';
        range_trace_dirty = true;
        range_last_draw_ms = 0;
      }
      break;
    }

    case QC_STICK_CAL:
      break;

    default:
      break;
  }
}

void QuickConfigMenu::navigateUp() {
  needs_redraw = true;
  const uint8_t turboCount = getVisibleTurboCount();
  const uint8_t dualMergeCount = getDualMergeMenuCount();

  switch (state) {
    case QC_MAIN_MENU:
      if (visible_count == 0) {
        on_bottom_row = true;
        bottom_index = 2;
        bottom_right = true;
        break;
      }
      if (on_bottom_row) {
        if (bottom_index > 0) {
          bottom_index--;
          bottom_right = bottom_index == 2;
        } else {
          on_bottom_row = false;
          selected_index = visible_count - 1;
        }
      } else {
        if (selected_index == 0) {
          on_bottom_row = true;
          bottom_index = 2;
          bottom_right = true;
        } else {
          selected_index--;
        }
      }
      break;

    case QC_CONFIRM_DEFAULT:
      default_confirm_index = !default_confirm_index;
      break;

    case QC_CONFIRM_DINPUT_RESET:
      dinput_reset_confirm_index = !dinput_reset_confirm_index;
      break;

    case QC_CONFIRM_NSO_1P:
      nso_1p_confirm_index = !nso_1p_confirm_index;
      break;

    case QC_TURBO_LIST:
      if (turbo_index == 0) {
        turbo_index = turboCount;
      } else {
        turbo_index--;
      }
      break;

    case QC_TURBO_RATE:
      if (turbo_rate_selection == 0) {
        turbo_rate_selection = TURBO_RATE_LAST;
      } else {
        turbo_rate_selection--;
      }
      break;

    case QC_SETTING_EDIT:
      cycleSettingValueBack();
      break;

    case QC_MODE_PICKER:
      mode_picker_index = mode_picker_index == 0 ? modePickerCount() : mode_picker_index - 1;
      break;

    case QC_CHOICE_PICKER:
      choice_picker_index = choice_picker_index == 0 ? choicePickerCount() : choice_picker_index - 1;
      break;

    case QC_ANALOG_SUBMENU:
      buildAnalogItems();
      if (analog_index == 0) {
        analog_index = analog_count;
      } else {
        analog_index--;
      }
      break;

    case QC_DUAL_MERGE_MAP:
      if (dual_merge_index == 0) {
        dual_merge_index = dualMergeCount;
      } else {
        dual_merge_index--;
      }
      break;

    case QC_RUMBLE_SUBMENU:
      advanceRumbleIndex(false);
      break;
    case QC_HOTKEYS_SUBMENU:
      buildHotkeyItems();
      hotkey_index = hotkey_index == 0 ? hotkey_count : hotkey_index - 1;
      break;

#ifdef ENABLE_INPUT_PSX
    case QC_GUNCON_ADJUST:
      handleGunconNavigateBack();
      break;
#endif

    case QC_REMAP_LIST: {
      uint8_t btn_count = getRemapButtonCount(temp_input_mode);
      if (remap_index == 0) {
        remap_index = btn_count + 1;
      } else if (remap_index >= btn_count) {
        remap_index = btn_count - 1;
      } else {
        remap_index--;
      }
      break;
    }

    case QC_RANGE_TEST:
    {
      const uint8_t frameCount =
        max_devices < MAX_USB_OUT ? max_devices : MAX_USB_OUT;
      const AnalogDiagnosticTarget next =
        analogDiagnosticNextPortTarget(
          deviceMode, controller_frames, frameCount,
          range_test_target, -1);
      if (analogDiagnosticTargetIsValid(next) &&
          next.port != range_test_target.port) {
        range_test_target = next;
        range_test_right_stick =
          next.stick == AnalogDiagnosticStick::Aux;
        range_trace[0].reset();
        range_trace[1].reset();
        range_trace_type[0] = '\0';
        range_trace_dirty = true;
        range_last_draw_ms = 0;
      }
      break;
    }

    case QC_STICK_CAL:
      break;

    default:
      break;
  }
}

bool QuickConfigMenu::canCycleValueWithDpad() {
  // Directional repeats may edit values or move a selection, never activate
  // submenu/Back rows. Confirm and physical Select keep their existing path.
  switch (state) {
    case QC_MAIN_MENU:
      return on_bottom_row ||
          (selected_index < visible_count &&
           isDirectlyEditable(visible_items[selected_index]) &&
           !needsSubmenu(visible_items[selected_index]));
    case QC_MODE_PICKER:
    case QC_CHOICE_PICKER:
      return false;
    case QC_TURBO_LIST:
      return false;  // Each rate opens a list; Back takes OK.
    case QC_REMAP_LIST:
      return remap_index >= getRemapButtonCount(temp_input_mode);
    case QC_ANALOG_SUBMENU:
      buildAnalogItems();
      return analog_index < analog_count &&
          isDirectlyEditable(analog_items[analog_index]) &&
          !needsSubmenu(analog_items[analog_index]);
    case QC_DUAL_MERGE_MAP:
      return dual_merge_index < getDualMergeMenuCount();
    case QC_HOTKEYS_SUBMENU:
      buildHotkeyItems();
      return hotkey_index < hotkey_count &&
          !needsSubmenu(hotkey_items[hotkey_index]);
    case QC_RUMBLE_SUBMENU:
      return rumble_index == QCI_RUMBLE_LEVEL;
    case QC_TURBO_RATE:
      return false; // This legacy page commits/exits on Left.
    default:
      return true;
  }
}

void QuickConfigMenu::cycleValueNext(bool isSelectAction) {
  if (!isSelectAction && !canCycleValueWithDpad()) return;
  needs_redraw = true;
  const uint8_t turboCount = getVisibleTurboCount();

  switch (state) {
    case QC_MODE_PICKER:
      if (isSelectAction) confirmModePicker();
      break;
    case QC_CHOICE_PICKER:
      if (isSelectAction) confirmChoicePicker();
      break;
    case QC_MAIN_MENU:
      if (on_bottom_row) {
        if (isSelectAction) {
          select();
          return;
        }
        bottom_index = (bottom_index + 1) % 3;
        bottom_right = bottom_index == 2;
        return;
      }
      cycleMainMenuItemNext();
      break;

    case QC_CONFIRM_DEFAULT:
      if (isSelectAction) {
        select();
      } else {
        default_confirm_index = !default_confirm_index;
      }
      break;

    case QC_CONFIRM_DINPUT_RESET:
      if (isSelectAction) {
        select();
      } else {
        dinput_reset_confirm_index = !dinput_reset_confirm_index;
      }
      break;

    case QC_CONFIRM_NSO_1P:
      if (isSelectAction) {
        select();
      } else {
        nso_1p_confirm_index = !nso_1p_confirm_index;
      }
      break;

    case QC_TURBO_LIST:
      if (turbo_index >= turboCount) {
        state = QC_MAIN_MENU;
      } else {
        openChoicePicker(QCI_TURBO);
      }
      break;

    case QC_ANALOG_SUBMENU:
      handleAnalogSelect();
      break;
    case QC_HOTKEYS_SUBMENU:
      handleHotkeySelect(true);
      break;

    case QC_DUAL_MERGE_MAP:
      handleDualMergeSelect();
      break;

    case QC_RUMBLE_SUBMENU:
      handleRumbleSelect(true);
      break;

#ifdef ENABLE_INPUT_PSX
    case QC_GUNCON_ADJUST:
      handleGunconNavigate();
      break;
#endif

    case QC_REMAP_LIST: {
      uint8_t btn_count = getRemapButtonCount(temp_input_mode);
      if (remap_index >= btn_count) {
        if (isSelectAction) {
          select();  // A activates Back or Default; the d-pad moves between them.
          return;
        }
        remap_index = remap_index == btn_count ? btn_count + 1 : btn_count;
      } else {
        openChoicePicker(QCI_REMAP);
      }
      break;
    }

    case QC_RANGE_TEST:
      if (isSelectAction) {
        state = return_to_analog_submenu ? QC_ANALOG_SUBMENU : QC_MAIN_MENU;
      } else {
        const uint8_t frameCount =
          max_devices < MAX_USB_OUT ? max_devices : MAX_USB_OUT;
        const AnalogDiagnosticTarget next =
          analogDiagnosticTargetForPort(
            deviceMode, controller_frames, frameCount,
            range_test_target.port, AnalogDiagnosticStick::Aux);
        if (analogDiagnosticTargetIsValid(next) &&
            next.kind == AnalogDiagnosticKind::Stick &&
            next.stick == AnalogDiagnosticStick::Aux) {
          range_test_target = next;
          range_test_right_stick = true;
          range_trace_dirty = true;
        }
      }
      break;

    case QC_STICK_CAL:
      if (isSelectAction) {
        select();
      } else {
        cal_selection = (cal_selection + 1) % 3;
      }
      break;

    default:
      break;
  }
}

void QuickConfigMenu::cycleValuePrev() {
  if (!canCycleValueWithDpad()) return;
  needs_redraw = true;

  switch (state) {
    case QC_MAIN_MENU:
      cycleMainMenuItemPrev();
      break;

    case QC_CONFIRM_DEFAULT:
      default_confirm_index = !default_confirm_index;
      break;

    case QC_CONFIRM_DINPUT_RESET:
      dinput_reset_confirm_index = !dinput_reset_confirm_index;
      break;

    case QC_CONFIRM_NSO_1P:
      nso_1p_confirm_index = !nso_1p_confirm_index;
      break;

    case QC_TURBO_RATE:
      if (turbo_rate_selection >= TURBO_RATE_LAST) {
        state = QC_TURBO_LIST;
      } else {
        uint8_t btn_index = getTurboButtonIndexForVisibleIndex(turbo_index);
        if (btn_index != 0xFF) {
          temp_rates[btn_index] = getTurboRateForMenuIndex(turbo_rate_selection);
        }
        state = QC_TURBO_LIST;
      }
      break;

    case QC_ANALOG_SUBMENU:
      handleAnalogSelectBack();
      break;
    case QC_HOTKEYS_SUBMENU:
      handleHotkeySelect(false);
      break;

    case QC_DUAL_MERGE_MAP:
      handleDualMergeSelectBack();
      break;

    case QC_RUMBLE_SUBMENU:
      handleRumbleSelect(false);
      break;

#ifdef ENABLE_INPUT_PSX
    case QC_GUNCON_ADJUST:
      handleGunconNavigateBack();
      break;
#endif

    case QC_REMAP_LIST: {
      uint8_t btn_count = getRemapButtonCount(temp_input_mode);
      if (remap_index >= btn_count) {
        remap_index = remap_index == btn_count ? btn_count + 1 : btn_count;
      }
      break;
    }

    case QC_RANGE_TEST:
    {
      const uint8_t frameCount =
        max_devices < MAX_USB_OUT ? max_devices : MAX_USB_OUT;
      const AnalogDiagnosticTarget next =
        analogDiagnosticTargetForPort(
          deviceMode, controller_frames, frameCount,
          range_test_target.port, AnalogDiagnosticStick::Main);
      if (analogDiagnosticTargetIsValid(next) &&
          next.kind == AnalogDiagnosticKind::Stick &&
          next.stick == AnalogDiagnosticStick::Main) {
        range_test_target = next;
        range_test_right_stick = false;
        range_trace_dirty = true;
      }
      break;
    }

    case QC_STICK_CAL:
      cal_selection = (cal_selection == 0) ? 2 : cal_selection - 1;
      break;

    default:
      break;
  }
}

void QuickConfigMenu::back() {
  needs_redraw = true;
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  if (state == QC_USB2DB15_TOURNAMENT) {
    state = QC_MAIN_MENU;
    return;
  }
#endif

  switch (state) {
    case QC_MODE_PICKER:
      state = QC_MAIN_MENU;
      break;
    case QC_CHOICE_PICKER:
      state = choice_return_state;
      break;
    case QC_MAIN_MENU:
      discard();
      break;

    case QC_CONFIRM_DEFAULT:
      state = QC_MAIN_MENU;
      break;

    case QC_CONFIRM_DINPUT_RESET:
      state = QC_MAIN_MENU;
      break;

    case QC_CONFIRM_NSO_1P:
      state = QC_MAIN_MENU;
      break;

    case QC_TURBO_LIST:
    case QC_REMAP_LIST:
    case QC_ANALOG_SUBMENU:
    case QC_HOTKEYS_SUBMENU:
    case QC_DUAL_MERGE_MAP:
    case QC_RUMBLE_SUBMENU:
    case QC_GUNCON_ADJUST:
      state = QC_MAIN_MENU;
      break;

    case QC_RANGE_TEST:
    case QC_STICK_CAL:
      state = return_to_analog_submenu ? QC_ANALOG_SUBMENU : QC_MAIN_MENU;
      break;

    case QC_TURBO_RATE:
      state = QC_TURBO_LIST;
      break;

    case QC_SETTING_EDIT:
      state = QC_MAIN_MENU;
      break;

    default:
      break;
  }
}
