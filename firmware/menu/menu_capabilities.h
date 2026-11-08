#pragma once

#include "../core/device_mode.h"
#include "../output/output_mode.h"
#include "rumble_capabilities.h"

bool menuModeHasAnalog(DeviceEnum mode);
bool menuModeHasRightStick(DeviceEnum mode);
bool menuModeCurrentControllerHasAnalog(DeviceEnum mode);
bool menuModeCurrentControllerHasAnalogStick(DeviceEnum mode);
bool menuModeCurrentControllerHasRightStick(DeviceEnum mode);
bool menuModeCurrentControllerHasAnalogTriggers(DeviceEnum mode);
bool menuModeHasLiveAnalogDiagnostic(DeviceEnum mode);
bool menuModeHasRumble(DeviceEnum mode);
RumbleCapabilities menuModeCurrentRumbleCapabilities(DeviceEnum mode);
bool menuModeHasAnalogTriggers(DeviceEnum mode);
bool menuModeSupportsTriggerAxisMode(DeviceEnum mode);
bool menuModeSupportsTriggerBothMode(DeviceEnum mode);
bool menuModeHasIndependentDigitalTriggerButtons(DeviceEnum mode);
bool outputModeSupportsTriggerBothMode(outputMode_t mode);
bool menuModeIsNintendoController(DeviceEnum mode);
bool menuModeIsN64Controller(DeviceEnum mode);
// Keypad controllers use directional fields for keypad/disc data rather than
// generic stick policy. Their Quick Menu intentionally stays on basic controls.
bool menuModeUsesBasicKeypadQuickConfig(DeviceEnum mode);
// The N64 Adapt V1 XInput layout pins physical Z to Back, so its per-mode
// Z assignment would be a no-op. GameCube keeps its independent Z setting.
bool menuShouldShowN64ZMode(DeviceEnum inputMode, outputMode_t selectedOutput,
                            bool nsoSpecialActive);
bool shouldHideMenuOutputMode(outputMode_t mode);
