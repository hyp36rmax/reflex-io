#pragma once

#include <cstdint>

namespace controller_runtime_internal {

void updateStickCenteringFromPoll(bool updated);
void snapshotRawInputButtonsBeforeTransforms();
void applyDeadzoneAndTriggerTransforms(bool updated);
void snapshotPhysicalButtonsBeforeTransforms();
void applyButtonRemapTransforms();
void applyButtonChordRemapTransforms();
void applyTurboTransforms();
void captureDualInputMergeSourceSlots();
void applyClassicDualControllerMergeTransform();
void updateHeldControllerHotkeysBeforeTransforms();
bool virtualControllerHotkeysRequireService();
void applyVirtualControllerHotkeyOutputs();
uint32_t suppressedControllerHotkeySourceButtons(uint8_t index);
void updateJaguarRotaryStateFromInputs();
void applySocdCleaningToInputs();

void cacheProcessedControllerOutputStateInternal();
void handleHeldBootloaderHotkey();
void handleHeldDpadModeHotkey();
bool dualMergePrimaryIsFirstSlot();
void captureOutputFinalizeRestoreFrames();
void retainOutputFinalizeRestoreFrame(uint8_t index);
void rebuildDigitalButtonsForOutputFrame(bool applyRemap,
                                         bool applyChordRemap,
                                         bool applyTurbo,
                                         bool applyVirtualHotkeys);
void applyAnalogCalibrationToConnectedInputs();
void applyFinalStickDeadzoneToConnectedInputs();
void applyRotaryTuningToOutputFrames();
void applyNegconInputPressesToProcessedFrames();  // PSX products only
void applyOutputButtonModeMappingsToConnectedInputs();
void clearJaguarRotaryDpadBeforeUsbSend();
void consumeMenuCapturedControllerOutput();
void restorePhysicalButtonsAfterUsbSend();
void noteProcessedButtonsAfterTransforms();
void restorePhysicalButtonsBeforeInputPoll();

}  // namespace controller_runtime_internal
