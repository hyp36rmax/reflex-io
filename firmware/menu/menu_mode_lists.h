#pragma once

#include "menu_action.h"
#include "menu_input.h"
#include "menu_item_defs.h"

bool handleMenuModeListsItem(menu_item_enum item, menu_item_action action);
bool handleMenuModeLists(const MenuControllerInput& ctrl, bool navigate, bool select,
                         uint8_t selected, uint8_t scroll, bool bottomRight);
bool menuModeListsActive();
void closeMenuModeLists();
void finishMenuModePickerForHold(bool save);
bool saveMenuModeListsPreferences();
