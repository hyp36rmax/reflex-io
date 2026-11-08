#pragma once

#include "../button_handler.h"

// Platform-owned menu/button runtime helpers that coordinate quick config,
// system menu entry, home-screen button actions, and menu hotkeys.

bool wakeFromVisibleScreensaver(ButtonEvent modeEvent, ButtonEvent resetEvent);
void handleMenuAndButtonUi(ButtonEvent modeEvent, ButtonEvent resetEvent);
// Called after a complete JVS reply, before the next request is sent.
void serviceJvsSafeWindowMenuUi();
bool isJvsMenuSafeWindowActive();
void runPlatformMenuHotkeys();
bool queuePlatformMenuControlAction(const char* action);
void openCombinedControllerSettingsMenu();
void openControllerMenuFromSerial();
void openSystemMenuFromSerial();
void closeMenusFromSerial();
