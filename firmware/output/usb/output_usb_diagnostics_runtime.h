#pragma once

#include <stdint.h>

#include "../../product_config.h"

#if ADAPT_POLICY_SERIAL_NEUTRAL_FRAME_INJECTION
#include "../../core/serial_neutral_frame_injection.h"
#endif

inline void outputUsbDiagnosticsNoteSubmit(uint8_t sourcePlayer,
                                           uint8_t outputSlot) {
#if ADAPT_POLICY_SERIAL_NEUTRAL_FRAME_INJECTION
  serialNeutralFrameInjectionNoteUsbSubmit(sourcePlayer, outputSlot);
#else
  (void)sourcePlayer;
  (void)outputSlot;
#endif
}

inline void outputUsbDiagnosticsNoteComplete(uint8_t outputSlot,
                                             bool success) {
#if ADAPT_POLICY_SERIAL_NEUTRAL_FRAME_INJECTION
  serialNeutralFrameInjectionNoteUsbComplete(outputSlot, success);
#else
  (void)outputSlot;
  (void)success;
#endif
}
