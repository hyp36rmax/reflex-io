#pragma once
#include "../../../platform/runtime/rp2040_crash_telemetry.h"

inline void productRuntimeBootInit() { rp2040CrashTelemetryBootInit(); }
inline void productRuntimePhaseBoot() { rp2040CrashTelemetryBootPhase(RP2040_CRASH_PHASE_BOOT); }
inline void productRuntimePhaseHardware() { rp2040CrashTelemetryBootPhase(RP2040_CRASH_PHASE_HARDWARE); }
inline void productRuntimePhaseStorage() { rp2040CrashTelemetryBootPhase(RP2040_CRASH_PHASE_STORAGE); }
inline void productRuntimeMarkSettingsReady() {}
inline void productRuntimePhaseInput() { rp2040CrashTelemetryBootPhase(RP2040_CRASH_PHASE_INPUT); }
inline void productRuntimePhaseOutput() { rp2040CrashTelemetryBootPhase(RP2040_CRASH_PHASE_OUTPUT); }
inline void productRuntimeSetupInputBridge(bool) {}
inline void productRuntimeBeforeTimingQuietUsb() {}
inline void productRuntimePhaseUsbConfig() { rp2040CrashTelemetryBootPhase(RP2040_CRASH_PHASE_USB_CONFIG); }
inline void productRuntimeAfterOutputConnect() {}
inline void productRuntimeStartDeferredInputHost() {}
inline void productRuntimePhaseMainLoop() { rp2040CrashTelemetrySetPhase(RP2040_CRASH_PHASE_RUNTIME); }
