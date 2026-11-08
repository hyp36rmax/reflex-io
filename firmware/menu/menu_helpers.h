#pragma once

#include <stdint.h>

class SSD1306AsciiWire;

#include "menu_action.h"
#include "menu_item_defs.h"

void printCenteredMenuTitle(SSD1306AsciiWire& target, const char* title);
void printOledMarqueeText(SSD1306AsciiWire& target, const char* text,
                          uint8_t max_chars, bool selected,
                          uint32_t marquee_start_ms = 0);

bool should_hide_menu_item(menu_item_enum item);
uint8_t getVisibleItemCount();
uint8_t getActualIndex(uint8_t visible_index);
uint8_t getVisibleIndex(uint8_t actual_index);
void clearRow(uint8_t row);
bool is_mode_specific_setting(menu_item_enum item);
bool is_per_input_quick_setting(menu_item_enum item);
bool is_system_setting(menu_item_enum item);
void printMenuItem(uint8_t actual_index, uint8_t row, bool selected);
void renderBottomRow(bool on_bottom_row, bool right_selected);
// Selectable text buttons. The selected one is inverted inside a filled box;
// every button keeps the same padding so selection never moves the row.
constexpr uint8_t kMenuButtonPadLeft = 3;
constexpr uint8_t kMenuButtonPadRight = 2;
uint8_t menuButtonWidth(const char* label);
void printMenuButton(SSD1306AsciiWire& target, const char* label, bool selected);
// One row of a value list: '>' on the cursor, '*' on the stored value.
void printMenuChoiceRow(SSD1306AsciiWire& target, uint8_t row, bool selected, bool current,
                        const char* name);
// One to three buttons on a text row: the first at the left edge, the last
// at the right edge, a middle one centred between them. selected < 0: none.
void renderMenuButtonRow(uint8_t row, const char* const* labels, uint8_t count,
                         int8_t selected);
void renderScrollIndicator(uint8_t scroll_offset);
void renderMenu(uint8_t selected_visible, uint8_t scroll_offset, bool bottom_right_selected);
void menuResetAllItems();
void menuSaveAllItems();
void handleMenuItemAction(menu_item_enum item, menu_item_action action);
