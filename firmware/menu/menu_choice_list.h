#pragma once

#include <stdint.h>

#include "menu_input.h"
#include "menu_item_defs.h"

// A System setting with four or more values opens this list from its row
// instead of cycling in place; two or three values still cycle (owner,
// 2026-10-25). It reads like the Quick Menu's list: OK opens it on the current
// value, Up/Down move, OK chooses, Back keeps the old value and Left/Right do
// nothing. The choice stays staged until the page that owns it saves.
struct MenuChoiceList {
  const char* title;
  uint8_t count;
  uint8_t current;  // 0xFF: the stored value is not one of the choices
  const char* (*label)(uint8_t index);
  void (*choose)(uint8_t index);
  void (*closed)();  // Redraws the page underneath.
};

constexpr uint8_t kMenuChoiceListMinValues = 4;

void openMenuChoiceList(const MenuChoiceList& list);
bool menuChoiceListActive();
void closeMenuChoiceList();
// Returns true while the list owns input and the display.
bool handleMenuChoiceList(const MenuControllerInput& ctrl, bool navigatePressed, bool changePressed);
// A hold finishes the list first, so a save takes the highlighted value.
void finishMenuChoiceListForHold(bool save);

// Rows of the System page itself.
bool menuItemOpensChoiceList(menu_item_enum item);
void openMenuItemChoiceList(menu_item_enum item);
