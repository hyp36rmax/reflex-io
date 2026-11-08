#pragma once

#include "../core/device_mode.h"
#include "menu_action.h"

bool should_hide_input_mode(DeviceEnum mode);
bool inputModeAvailableInMenu(DeviceEnum mode);
uint8_t menuInputChoiceCount(bool includeHidden = false);
DeviceEnum menuInputChoiceAt(uint8_t index, bool includeHidden = false);
const char* getInputModeName(DeviceEnum mode);
void printInputModeWithMenuButtons(DeviceEnum mode);
void handle_item_input_mode(menu_item_action action);
