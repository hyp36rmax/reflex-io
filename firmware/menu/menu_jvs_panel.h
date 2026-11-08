#pragma once

#include "menu_input.h"

// Open Learn directly as a child of Test Controls.
void openJvsPanelLearning();
bool finishJvsPanelMenuForHold(bool save);

// Legacy Panel page: Auto (switch-count guess) or Learn (guided P1-K4).
void renderJvsPanelMenu(bool selectJustPressed, bool navigateJustPressed,
                        const MenuControllerInput& controllerInput);
// Value shown beside the System menu item.
const char* jvsPanelMenuValue();
