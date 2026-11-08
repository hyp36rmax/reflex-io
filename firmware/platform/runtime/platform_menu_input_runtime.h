#pragma once

#include <Arduino.h>

#include "../button_handler.h"

enum class SerialMenuAction : uint8_t {
  None,
  Menu,
  Up,
  Down,
  Left,
  Right,
  Ok,
  Back
};

void applyKioskButtonGate(ButtonEvent& modeEvent, ButtonEvent& resetEvent);
uint32_t serialMenuActionToButtons(SerialMenuAction action);
SerialMenuAction takeSerialMenuAction();
bool queuePlatformMenuControlAction(const char* action);
