#include "quick_config.h"
#include "menu_helpers.h"
#include "menu_list_navigation.h"

uint8_t QuickConfigMenu::modePickerCount() const {
  return editing_item == QCI_INPUT_MODE ? menuInputChoiceCount() : menuOutputChoiceCount();
}

void QuickConfigMenu::openModePicker(QuickConfigItem item) {
  editing_item = item;
  mode_picker_index = 0;
  for (uint8_t i = 0; i < modePickerCount(); ++i) {
    if (item == QCI_INPUT_MODE ? menuInputChoiceAt(i) == temp_input_mode :
                                menuOutputChoiceAt(i) == temp_output_mode) {
      mode_picker_index = i;
      break;
    }
  }
  state = QC_MODE_PICKER;
  needs_redraw = true;
}

void QuickConfigMenu::confirmModePicker() {
  if (mode_picker_index < modePickerCount()) {
    if (editing_item == QCI_INPUT_MODE) {
      const DeviceEnum mode = menuInputChoiceAt(mode_picker_index);
      if (mode != temp_input_mode) {
        temp_input_mode = mode;
        loadTempQuickSettingsForMode(mode);
      }
    } else {
      const outputMode_t mode = menuOutputChoiceAt(mode_picker_index);
      if (mode != temp_output_mode) {
        temp_output_mode = mode;
        restageRemapsForOutput();
      }
    }
    rebuildVisibleAndClampSelection();
  }
  state = QC_MAIN_MENU;
  needs_redraw = true;
}

void QuickConfigMenu::renderModePicker(SSD1306AsciiWire& target) {
  printCenteredMenuTitle(target, editing_item == QCI_INPUT_MODE ? "Input mode" : "Output mode");
  const uint8_t count = modePickerCount();
  const uint8_t first = menu_list_navigation::firstVisible(mode_picker_index, count, 6);
  for (uint8_t row = 0; row < 6 && first + row < count; ++row) {
    const uint8_t i = first + row;
    const bool input = editing_item == QCI_INPUT_MODE;
    const bool current = input ? menuInputChoiceAt(i) == temp_input_mode :
                                menuOutputChoiceAt(i) == temp_output_mode;
    target.setCursor(0, row + 1);
    target.print(i == mode_picker_index ? '>' : ' ');
    target.print(current ? '*' : ' ');
    const char* name = input ? getInputModeName(menuInputChoiceAt(i)) : get_mode_name(menuOutputChoiceAt(i));
    // Reserve the cursor/current marks on the 21-column OLED.
    for (uint8_t c = 0; name[c] && c < 19; ++c) target.print(name[c]);
  }
  target.setCursor(0, 7);
  printMenuButton(target, "Back", mode_picker_index == count);
}
