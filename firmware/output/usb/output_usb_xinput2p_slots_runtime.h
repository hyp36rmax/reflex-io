#pragma once

#include <Arduino.h>

#include "../../core/controller_frame_state.h"
#include "../../core/firmware_support.h"
#include "../output_runtime_state.h"
#include "../xinputw/output_xinputw_slot_routing.h"
#include "output_usb_player_count.h"

#ifdef ADAPT_OUTPUT_USB_DEVICE
#include <Adafruit_TinyUSB.h>
#endif

// Multi-player XInput is the Xbox 360 wireless receiver (out_xinputw). Live
// sources are compacted into its lowest children without re-enumerating.

inline uint8_t xinput2p_physical_source_count() {
#ifdef ENABLE_XINPUT2P_WIRELESS_TRANSPORT
  // Every receiver child the product offers carries a source; a slot cap
  // (Classic2USB and JVSIO2USB: 2) limits them.
  constexpr uint8_t routed =
      (ADAPT_POLICY_XINPUT2P_OUTPUT_SLOT_CAP > 0 &&
       ADAPT_POLICY_XINPUT2P_OUTPUT_SLOT_CAP < XINPUT_WIRELESS_CONTROLLERS)
          ? ADAPT_POLICY_XINPUT2P_OUTPUT_SLOT_CAP : XINPUT_WIRELESS_CONTROLLERS;
  return min(routed, (uint8_t)MAX_USB_OUT);
#else
  return 0;
#endif
}

inline uint32_t xinput2p_connected_source_mask() {
  const uint8_t sourceCount = xinput2p_physical_source_count();
  uint32_t mask = 0;
  for (uint8_t source = 0; source < sourceCount; ++source) {
    if (controllerFrameConst(source).connected) {
      mask |= 1UL << source;
    }
  }
#if defined(ADAPT_CAP_DUAL_INPUT_MERGE)
  // 2P Merge is one player sending from slot 0, whichever port holds the pad.
  // For a lone P2 that slot is empty between sends.
  if (classic_dual_merge_enabled && mask != 0) return 1UL;
#endif
  return mask;
}

inline bool xinput2p_target_connected(uint8_t target) {
  const uint8_t source = xinputw_source_port_for_target(target);
  return source != XINPUTW_INVALID_PORT && source < xinput2p_physical_source_count() &&
         (xinput2p_connected_source_mask() & (1UL << source)) != 0;
}

inline void service_xinput2p_wireless_slot_routing() {
#ifdef ENABLE_XINPUT2P_WIRELESS_TRANSPORT
  if (get_effective_output_mode() != OUTPUT_XINPUT2P) {
    return;
  }

  const uint8_t sourceCount = xinput2p_physical_source_count();
  const uint32_t connectedMask = xinput2p_connected_source_mask();

  if (!xinputw_slot_routing_update(sourceCount, connectedMask, true)) {
    return;
  }

  // Compact all live physical/transport sources into the lowest XInput
  // children without re-enumerating the receiver. A route change clears stale
  // feedback and pushes every surviving controller into its new child.
  for (uint8_t source = 0; source < sourceCount; ++source) {
    rumble_callback(source, 0, 0);
    if ((connectedMask & (1UL << source)) != 0) {
      requestControllerFrameDelivery(source);
    }
  }
#endif
}
