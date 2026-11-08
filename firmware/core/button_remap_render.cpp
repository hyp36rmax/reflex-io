#include "button_remap.h"
#include "n64_assignment_runtime.h"
#include "../menu/menu_helpers.h"

void ButtonRemapMenu::render(DeviceEnum mode) {
  if (!needs_redraw) return;
  needs_redraw = false;

  display.clear();
  display.setFont(System5x7);

  renderButtonList(mode);
  renderPadDisplay(mode);
  renderBottomRow();
}

void ButtonRemapMenu::renderButtonList(DeviceEnum mode) {
  uint8_t visible_count = min((uint8_t)LIST_ROWS, button_count);

  for (uint8_t row = 0; row < visible_count; row++) {
    uint8_t btn_index = scroll_offset + row;
    if (btn_index >= button_count) break;

    display.setCursor(0, row);

    bool is_selected = (btn_index == selected_index) && (state == REMAP_NAVIGATE);
    bool is_editing = (btn_index == edit_source_index) && (state == REMAP_EDIT_SOURCE);

    if (is_editing) {
      display.print(F("*"));
    } else if (is_selected) {
      display.print(F(">"));
    } else {
      display.print(F(" "));
    }

    const uint8_t source_slot = getRemapButtonSlot(mode, btn_index);
    const char* src_name = getRemapButtonName(mode, btn_index);
    display.print(src_name);
    display.print(F("->"));

    const uint8_t target_slot = (source_slot != 0xFF) ? temp_remap[source_slot] : btn_index;
    display.print(remapTargetName(mode, get_effective_output_mode(), temp_remap, btn_index,
        target_slot, n64_cstick_mode, output_uses_switch_n64_profile(),
        effectivePositionButtonMap(mode, get_effective_output_mode(),
            is_nso_special_active(), button_map_mode)));

    if (source_slot != 0xFF && target_slot != source_slot) {
      display.print(F("!"));
    }
  }

  if (state == REMAP_EDIT_SOURCE) {
    display.setCursor(0, 6);
    display.print(F("Press btn.."));
  }
}

void ButtonRemapMenu::renderPadDisplay(DeviceEnum mode) {
  display.setFont(System5x7);

  display.setCursor(PAD_START_COL, 0);
  display.print("In > Out");

  uint8_t remap_count = 0;
  for (uint8_t i = 0; i < button_count; i++) {
    const uint8_t source_slot = getRemapButtonSlot(mode, i);
    if (source_slot != 0xFF && temp_remap[source_slot] != source_slot) remap_count++;
  }

  display.setCursor(PAD_START_COL, 1);
  display.print(remap_count);
  display.print(" changed");

  if (state == REMAP_EDIT_SOURCE) {
    display.setCursor(PAD_START_COL, 3);
    display.print("From:");
    display.setCursor(PAD_START_COL, 4);
    display.print(getRemapButtonName(mode, edit_source_index));
    display.setCursor(PAD_START_COL, 5);
    display.print("Press");
    display.setCursor(PAD_START_COL, 6);
    display.print("new btn");
  } else if (selected_index < button_count) {
    display.setCursor(PAD_START_COL, 3);
    const char* src = getRemapButtonName(mode, selected_index);
    const uint8_t source_slot = getRemapButtonSlot(mode, selected_index);
    const uint8_t target_slot = (source_slot != 0xFF) ? temp_remap[source_slot] : selected_index;
    display.print(src);
    display.print("->");
    display.print(remapTargetName(mode, get_effective_output_mode(), temp_remap, selected_index,
        target_slot, n64_cstick_mode, output_uses_switch_n64_profile(),
        effectivePositionButtonMap(mode, get_effective_output_mode(),
            is_nso_special_active(), button_map_mode)));
  }
}

void ButtonRemapMenu::renderBottomRow() {
  // Default resets every remap in place; Save/Cancel leave the screen.
  static const char* const kButtons[] = { "Cancel", "Default", "Save" };
  const bool onButtons = state == REMAP_NAVIGATE && selected_index >= button_count;
  renderMenuButtonRow(BOTTOM_ROW, kButtons, 3,
      onButtons ? static_cast<int8_t>(selected_index - button_count) : -1);
}
