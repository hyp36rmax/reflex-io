#pragma once
#include "../../../platform/runtime/rp2040_crash_telemetry.h"

inline bool productDeferIntentionalReset() { return false; }
// True while the product holds intentional resets back to replay them later.
// A query only: unlike productDeferIntentionalReset() it records nothing.
inline bool productIntentionalResetDeferred() { return false; }
// Saves, BOOTSEL and identity restarts are not reported as hangs.
inline void productMarkIntentionalReset() {
  rp2040CrashTelemetrySetPhase(RP2040_CRASH_PHASE_INTENTIONAL_RESET);
}
inline void productPrepareBootloaderReset() {}
