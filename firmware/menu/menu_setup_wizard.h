#pragma once

#include <stdint.h>

#include "menu_input.h"

// First-boot setup on the OLED. It runs as a System menu page, so the menu
// owns the controller and input auto-detection waits while it is open.

// True once after boot when factory defaults are still awaiting setup, the
// home screen has been shown and the USB host has been identified.
bool setupWizardShouldOpen();
// Loads the current settings as the starting choices. The caller opens the
// System menu around it.
void beginSetupWizard();
bool setupWizardActive();
// Owns physical button gestures during the safe, fake-settings tutorial.
// Call before the normal menu hold actions. True consumes this UI pass.
bool handleSetupWizardTutorialButtons(bool leftPressed, bool rightPressed,
                                     bool leftTap, bool rightTap, uint32_t now);
// Leaves without saving: previewed brightness, sound and OK/Back are restored
// and setup is offered again on the next boot.
void cancelSetupWizard();
// Saves the current choices and marks setup complete. False if the write failed.
bool finishSetupWizard();
// Returns true while the wizard owns input and the display.
bool handleSetupWizard(const MenuControllerInput& ctrl,
                       bool btnNavigateJustPressed,
                       bool btnChangeJustPressed);
