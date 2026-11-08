#pragma once

#include <stdint.h>

#if defined(ARDUINO)
#include <Print.h>
#else
class Print;
#endif

// Bounded diagnostic overlay for selected production MODERN2USB neutral frames.
// The live frame is always restored immediately after output staging.
bool handleSerialNeutralFrameInjectionCommand(char* text, Print& out);
void serialNeutralFrameInjectionService();
void serialNeutralFrameInjectionPrepareOutputFrame();
void serialNeutralFrameInjectionNoteOutputStaged();
#ifdef __cplusplus
extern "C" {
#endif
void serialNeutralFrameInjectionNoteUsbSubmit(uint8_t sourcePlayer,
                                               uint8_t outputSlot);
void serialNeutralFrameInjectionNoteUsbComplete(uint8_t outputSlot,
                                                 bool success = true);
#ifdef __cplusplus
}
#endif
void serialNeutralFrameInjectionRestoreOutputFrame();
void serialNeutralFrameInjectionCancel();
bool serialNeutralFrameInjectionActive();
// NFRAME TRAIN is a transport diagnostic. While it is active the runtime may
// defer controller polling/UI/background work so endpoint scheduling, rather
// than unrelated loop stalls, is what the train measures. Normal gameplay
// never enters this path and retains latest-state coalescing.
bool serialNeutralFrameInjectionOwnsPriorityOutput();
