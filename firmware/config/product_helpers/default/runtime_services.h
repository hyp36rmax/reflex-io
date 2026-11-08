#pragma once

inline void productRuntimeBootInit() {}
inline void productRuntimePhaseBoot() {}
inline void productRuntimePhaseHardware() {}
inline void productRuntimePhaseStorage() {}
inline void productRuntimeMarkSettingsReady() {}
inline void productRuntimePhaseInput() {}
inline void productRuntimePhaseOutput() {}
inline void productRuntimeSetupInputBridge(bool) {}
inline void productRuntimeBeforeTimingQuietUsb() {}
inline void productRuntimePhaseUsbConfig() {}
inline void productRuntimeAfterOutputConnect() {}
inline void productRuntimeStartDeferredInputHost() {}
inline void productRuntimePhaseMainLoop() {}
