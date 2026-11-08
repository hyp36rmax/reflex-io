#pragma once
#if defined(ADAPT_FEATURE_N64_CAPTURE)
#include "../features/n64_capture/n64_capture.h"
#endif
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
#include "../features/jogcon_capture/jogcon_capture.h"
#endif
#if defined(ADAPT_FEATURE_HOST_CAPTURE)
#include "../features/host_capture/host_capture.h"
#endif

inline bool productForcedRuntimeOutputMode(outputMode_t& forcedMode) {
#if defined(ADAPT_FEATURE_N64_CAPTURE)
  if (n64CaptureRecoveryActive()) { forcedMode = OUTPUT_MISTER; return true; }
#endif
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
  if (jogconCaptureRecoveryActive()) { forcedMode = OUTPUT_MISTER; return true; }
#endif
#if defined(ADAPT_FEATURE_HOST_CAPTURE)
  if (hostCaptureRecoveryActive()) { forcedMode = OUTPUT_MISTER; return true; }
#endif
#if defined(FORCE_JVSIO_MANAGEMENT_OUTPUT)
  forcedMode = OUTPUT_MISTER;
  return true;
#endif
#if defined(FORCE_CLASSIC_XINPUT2P_OUTPUT) && defined(ENABLE_EXPERIMENTAL_XINPUT2P_OUTPUT)
  forcedMode = OUTPUT_XINPUT2P;
  return true;
#endif
#if defined(FORCE_CLASSIC_XINPUT_OUTPUT) && defined(ENABLE_EXPERIMENTAL_XINPUT2P_OUTPUT)
  forcedMode = OUTPUT_XINPUT;
  return true;
#endif
#if defined(FORCE_CLASSIC_MANAGEMENT_OUTPUT)
  forcedMode = OUTPUT_MISTER;
  return true;
#endif
#if defined(FORCE_JVSIO_XINPUT2P_OUTPUT) && defined(ENABLE_EXPERIMENTAL_XINPUT2P_OUTPUT)
  forcedMode = OUTPUT_XINPUT2P;
  return true;
#endif
#if defined(FORCE_JVSIO_XID_OUTPUT)
  forcedMode = OUTPUT_XID;
  return true;
#endif
#if defined(FORCE_JVSIO_XBOXONE_OUTPUT) && defined(ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT)
  forcedMode = OUTPUT_XBOXONE;
  return true;
#endif
  (void)forcedMode;
  return false;
}

inline bool productAllowsXboxOneRuntimeOutput() {
#if !ADAPT_POLICY_EXPERIMENTAL_XBOXONE_OUTPUT
  return false;
#else
#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
  return true;
#else
  return false;
#endif
#endif
}

inline outputMode_t productWindowsXboxOnePreferenceMode(outputMode_t dinputMode) {
#if defined(ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT) && defined(ENABLE_UNSAFE_WINDOWS_MULTI_GIP)
  return OUTPUT_XBOXONE;
#elif defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT)
  // Four wired GIP functions are not a supported Windows receiver shape.
  // Keep this preference on the proven multi-player XInput receiver until a
  // real receiver-style GIP transport exists.
  return OUTPUT_XINPUT2P;
#else
  return dinputMode;
#endif
}

inline bool productForcesMisterDinputPreference() {
#ifdef FORCE_MODERN2USB_MISTER_DINPUT_OUTPUT
  return true;
#else
  return false;
#endif
}
