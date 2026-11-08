#pragma once

#include "menu_input.h"

void openJvsAnalogWizard();
void openJvsAnalogDiagnostics();
void closeJvsAnalogWizard();
bool finishJvsAnalogMenuForHold(bool save);

void renderJvsAnalogMenu(bool selectJustPressed, bool navigateJustPressed,
                         const MenuControllerInput& controllerInput);
