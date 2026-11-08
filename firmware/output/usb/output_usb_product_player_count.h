#pragma once

#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
// This header can be included from out_usb.h's extern "C" block.
extern "C++" uint8_t rotaryMouseOutputPlayerCount();
#endif

#if defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT) && defined(ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT)
// Declared (C linkage) in output/xboxone/out_xboxone.h, which not every
// includer of this header pulls in.
extern "C" bool xboxone_windows_multi_enabled();
#endif

inline uint8_t adjust_product_usb_player_count(uint8_t count) {
  if (get_effective_output_mode() == OUTPUT_SWITCH2PRO) return 1;
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  // Keep both physical frames polled/prepared, but enumerate one remote.
  if (get_effective_output_mode() == OUTPUT_REMOTE) return 1;
  if (get_effective_output_mode() == OUTPUT_ROTARY_MOUSE) {
    return rotaryMouseOutputPlayerCount();
  }
#endif
#if defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT) && defined(ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT)
  if (get_effective_output_mode() == OUTPUT_XBOXONE &&
      xboxone_windows_multi_enabled()) {
    return 4;
  }
#endif

#if defined(ADAPT_CAP_DUAL_INPUT_MERGE)
  // 2P Merge still reads both input controllers, but the host should see one
  // output player so the merged frame lands on a single gamepad endpoint.
  if (classic_dual_merge_enabled) {
    return 1;
  }
#endif
  return count;
}
