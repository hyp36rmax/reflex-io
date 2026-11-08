#include "../product_config.h"

#include <cstring>

#include "menu_helpers.h"
#include "menu_list_navigation.h"

#include "../core/device_runtime_state.h"
#include "../platform/display_runtime_state.h"
#include "menu_catalog.h"
#include "menu_mode_state.h"
#include "menu_ui_state.h"

#ifdef USE_I2C_DISPLAY
constexpr uint8_t kOledTextColumns = 21;
constexpr uint16_t kOledMarqueeStepMs = 250;
constexpr uint8_t kOledMarqueeInitialHoldSteps = 6;

void printCenteredMenuTitle(SSD1306AsciiWire& target, const char* title) {
  const char* text = title ? title : "";
  const size_t length = strlen(text);
  const uint8_t visibleLength = static_cast<uint8_t>(
      length < kOledTextColumns ? length : kOledTextColumns);
  const uint8_t x = static_cast<uint8_t>(
      (128u - static_cast<uint16_t>(visibleLength) * 6u) / 2u);
  target.clear(0, 127, 0, 0);
  target.setCursor(x, 0);
  for (uint8_t index = 0; index < visibleLength; ++index) {
    target.print(text[index]);
  }
  target.println();
}

void printOledMarqueeText(SSD1306AsciiWire& target, const char* text,
                          uint8_t max_chars, bool selected,
                          uint32_t marquee_start_ms) {
  if (!text || max_chars == 0) {
    return;
  }
  if (max_chars > kOledTextColumns) {
    max_chars = kOledTextColumns;
  }

  const size_t len = strlen(text);
  if (len <= max_chars) {
    target.print(text);
    return;
  }

  uint8_t offset = 0;
  uint8_t chars_to_print = max_chars;
  if (selected) {
    const uint8_t overflow = (uint8_t)(len - max_chars);
    const uint8_t hold_steps = kOledMarqueeInitialHoldSteps;
    const uint8_t period = (uint8_t)(overflow + 1 + hold_steps * 2);
    const uint32_t now = millis();
    const uint32_t elapsed_ms = marquee_start_ms
      ? (uint32_t)(now - marquee_start_ms)
      : now;
    const uint8_t phase = (uint8_t)((elapsed_ms / kOledMarqueeStepMs) % period);
    if (phase < hold_steps) {
      offset = 0;
    } else if (phase < hold_steps + overflow) {
      offset = phase - hold_steps;
    } else {
      offset = overflow;
    }
  } else if (max_chars > 1) {
    chars_to_print = max_chars - 1;
  }

  for (uint8_t i = 0; i < chars_to_print && text[offset + i]; ++i) {
    target.print(text[offset + i]);
  }
  if (!selected && max_chars > 1) {
    target.print('>');
  }
}
#else
void printCenteredMenuTitle(SSD1306AsciiWire& target, const char* title) {
  (void)target;
  (void)title;
}

void printOledMarqueeText(SSD1306AsciiWire& target, const char* text,
                          uint8_t max_chars, bool selected,
                          uint32_t marquee_start_ms) {
  (void)target;
  (void)text;
  (void)max_chars;
  (void)selected;
  (void)marquee_start_ms;
}
#endif

void clearRow(uint8_t row) {
#ifdef USE_I2C_DISPLAY
  display.clear(0, 127, row, row);
#else
  (void)row;
#endif
}

void printMenuItem(uint8_t actual_index, uint8_t row, bool selected) {
#ifndef USE_I2C_DISPLAY
  (void)actual_index;
  (void)row;
  (void)selected;
  return;
#else
  display.setRow(row);
  display.setCol(0);
  clearRow(row);

  if (selected)
    display.print('>');
  else
    display.print(' ');

  const menu_line_t* item = &menu_items[actual_index];

  switch (item->item) {
    case menu_item_exit:
    case menu_item_save_and_reboot:
      display.print(item->text);
      break;
    case menu_item_controller_settings:
    case menu_item_players:
    case menu_item_controller_leds:
    case menu_item_bluetooth_devices:
    case menu_item_web_config:
    case menu_item_mapping_profiles:
    case menu_item_auth_status:
    case menu_item_device_leds:
    case menu_item_diagnostics:
    case menu_item_output_transport:
    case menu_item_oled:
    case menu_item_mode_lists:
    case menu_item_setup_guide:
    case menu_item_games:
    case menu_item_buzzer:
    case menu_item_whats_new:
#if !ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
    case menu_item_hotkeys:
#endif
    case menu_item_kiosk_mode:
      display.print(item->text);
      display.print(" >");
      break;
    default:
      display.print(item->text);
      display.print(": ");
      if (menu_input != deviceMode && is_mode_specific_setting(item->item)) {
        display.print("---");
      } else {
        handleMenuItemAction(item->item, item_action_display);
      }
      break;
  }
#endif
}

uint8_t menuButtonWidth(const char* label) {
  return static_cast<uint8_t>(strlen(label) * 6 + kMenuButtonPadLeft + kMenuButtonPadRight);
}

void printMenuButton(SSD1306AsciiWire& target, const char* label, bool selected) {
#ifdef USE_I2C_DISPLAY
  // Inverted blank columns become the filled box around the label.
  target.setInvertMode(selected);
  for (uint8_t i = 0; i < kMenuButtonPadLeft; ++i) target.ssd1306WriteRamBuf(0);
  target.print(label);
  for (uint8_t i = 0; i < kMenuButtonPadRight; ++i) target.ssd1306WriteRamBuf(0);
  target.setInvertMode(false);
#else
  (void)target;
  (void)label;
  (void)selected;
#endif
}

void renderMenuButtonRow(uint8_t row, const char* const* labels, uint8_t count,
                         int8_t selected) {
#ifndef USE_I2C_DISPLAY
  (void)row;
  (void)labels;
  (void)count;
  (void)selected;
#else
  clearRow(row);
  if (count == 0 || count > 3) return;
  uint8_t x[3] = {0, 0, 0};
  if (count > 1) x[count - 1] = static_cast<uint8_t>(128 - menuButtonWidth(labels[count - 1]));
  if (count == 3) {
    const uint8_t left = menuButtonWidth(labels[0]);
    x[1] = static_cast<uint8_t>(left + (x[2] - left - menuButtonWidth(labels[1])) / 2);
  }
  for (uint8_t i = 0; i < count; ++i) {
    display.setCursor(x[i], row);
    printMenuButton(display, labels[i], selected == static_cast<int8_t>(i));
  }
#endif
}

void printMenuChoiceRow(SSD1306AsciiWire& target, uint8_t row, bool selected, bool current,
                        const char* name) {
#ifdef USE_I2C_DISPLAY
  target.setCursor(0, row);
  target.print(selected ? '>' : ' ');
  target.print(current ? '*' : ' ');
  // Reserve the cursor/current marks on the 21-column OLED.
  for (uint8_t c = 0; name[c] && c < 19; ++c) target.print(name[c]);
#else
  (void)target;
  (void)row;
  (void)selected;
  (void)current;
  (void)name;
#endif
}

void renderBottomRow(bool on_bottom_row, bool right_selected) {
  static const char* const kLabels[] = { "Cancel", "Save" };
  renderMenuButtonRow(7, kLabels, 2, on_bottom_row ? (right_selected ? 1 : 0) : -1);
}

void renderScrollIndicator(uint8_t scroll_offset) {
#ifndef USE_I2C_DISPLAY
  (void)scroll_offset;
  return;
#else
  display.setRow(0);
  display.setCol(120);
  if (scroll_offset > 0)
    display.print('^');
  else
    display.print(' ');
#endif
}

void renderMenu(uint8_t selected_visible, uint8_t scroll_offset, bool bottom_right_selected) {
#ifndef USE_I2C_DISPLAY
  (void)selected_visible;
  (void)scroll_offset;
  (void)bottom_right_selected;
  return;
#else
  display.setFont(System5x7);
  display.set1X();

  uint8_t visible_count = getVisibleItemCount();
  bool on_bottom_row = (selected_visible >= visible_count);
  scroll_offset = menu_list_navigation::firstVisible(selected_visible, visible_count, MENU_ITEM_ROWS);

  printCenteredMenuTitle(
      display, system_menu_only ? "System Settings" : "Settings");

  for (uint8_t row = 0; row < MENU_ITEM_ROWS; ++row) {
    uint8_t visible_idx = scroll_offset + row;
    if (visible_idx < visible_count) {
      uint8_t actual_idx = getActualIndex(visible_idx);
      printMenuItem(actual_idx, row + 1, visible_idx == selected_visible);
    } else {
      clearRow(row + 1);
    }
  }

  renderScrollIndicator(scroll_offset);
  renderBottomRow(on_bottom_row, bottom_right_selected);
#endif
}
