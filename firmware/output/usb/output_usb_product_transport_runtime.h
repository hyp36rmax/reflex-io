#pragma once

inline bool beginProductXinput2pTransportRuntime() {
#ifdef ENABLE_XINPUT2P_WIRELESS_TRANSPORT
  _xinputw->begin();
  return true;
#else
  return false;
#endif
}

inline void configureProductXinputTransportRuntime() {
#if defined(ENABLE_XINPUT2P_WIRELESS_TRANSPORT)
  if (get_effective_output_mode() != OUTPUT_XINPUT2P || !_xinputw) return;
  xinputw_slot_routing_reset();
  service_xinput2p_wireless_slot_routing();
  for (uint8_t target = 0; target < XINPUT_WIRELESS_CONTROLLERS; ++target) {
    _xinputw->set_connected(target, xinput2p_target_connected(target));
  }
#endif
}
