#pragma once

#include "../auth/auth_status.h"
#include "../../features/usb_drive/usb_drive.h"
#include "output_usb_product_transport_runtime.h"

// Internal USB output configuration/runtime helpers. This stays header-only so
// it can reuse the shared TinyUSB objects and output configuration state still
// declared in out_usb.h.

inline bool output_mode_requires_home(outputMode_t mode) {
  switch (mode) {
    case OUTPUT_XINPUT:
    case OUTPUT_XINPUTW:
    case OUTPUT_XINPUT2P:
    case OUTPUT_XBOXONE:
    case OUTPUT_PS3:
    case OUTPUT_PS4:
#ifdef ENABLE_OUTPUT_PS5
    case OUTPUT_PS5:
#endif
    case OUTPUT_SWITCH:
    case OUTPUT_SWITCHPRO:
    case OUTPUT_SWITCH2PRO:
      return true;
    default:
      return false;
  }
}

inline void configure_usb_output() {
  #if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
  const bool lateTinyUsbInit = !tud_inited();
  #endif

  TinyUSBDevice.clearConfiguration();

  #if defined(ENABLE_INPUT_AUTODETECT) && defined(AUTODETECT_DEBUG_CDC)
  Serial.begin(115200);
  #endif

#if defined(ENABLE_INPUT_DREAMCAST)
  dreamcastDebugCdcEnabled = false;
#endif

  auto_detect_clear_scratch_state();

  use_device_hid_class = false;
  grouped_devices = 0;
  outputMode_t effectiveOutputMode = get_effective_output_mode();
  if (!authOutputModeCanRun(effectiveOutputMode)) {
    outputMode = OUTPUT_MISTER;
    effectiveOutputMode = OUTPUT_MISTER;
  }
  const bool managementCdcEnabled =
    output_allows_management_cdc(effectiveOutputMode);

  #ifdef ENABLE_RUNTIME_SERIAL_DEBUG_CDC
  runtimeDebugCdcEnabled = managementCdcEnabled;
  if (runtimeDebugCdcEnabled) {
    configure_management_composite_buffer_runtime();
    // The interface name comes from runtimeDebugCdc's NamedUsbCdc class.
    runtimeDebugCdc.begin(115200);
  }
  #endif

  if (output_is_generic_mister_hid_mode(effectiveOutputMode)) {
    configure_generic_mister_hid_output_runtime();
  // Other products sanitize these PSX peripheral modes to OUTPUT_MISTER
  // (sanitizeConfiguredOutputMode), so only classic ingress builds them.
  #if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  } else if (effectiveOutputMode == OUTPUT_REMOTE) {
    configure_remote_output_runtime();
  } else if (output_effective_mode_is(OUTPUT_MISTER_JOGCON)) {
    configure_mister_jogcon_output_runtime();
  } else if (effectiveOutputMode == OUTPUT_MISTER_NEGCON) {
    configure_mister_negcon_output_runtime();
  } else if (effectiveOutputMode == OUTPUT_MISTER_PADDLE) {
    configure_mister_paddle_output_runtime();
  } else if (effectiveOutputMode == OUTPUT_MISTER_SPINNER) {
    configure_mister_spinner_output_runtime();
  } else if (effectiveOutputMode == OUTPUT_ROTARY_MOUSE) {
    configure_rotary_mouse_output_runtime();
  } else if (effectiveOutputMode == OUTPUT_MISTER_GUNCON) {
    configure_mister_guncon_output_runtime();
  #endif
  } else if (effectiveOutputMode == OUTPUT_KEYBOARD) {
    configure_keyboard_output_runtime();
  } else if (effectiveOutputMode == OUTPUT_XINPUT) {
    configure_xinput_output_runtime();
  } else if (effectiveOutputMode == OUTPUT_XINPUTW) {
    configure_xinput_wireless_output_runtime();
  } else if (effectiveOutputMode == OUTPUT_XINPUT2P) {
    configure_xinput_2p_output_runtime();
  } else if (effectiveOutputMode == OUTPUT_XID) {
    configure_xid_output_runtime();
  #ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
  } else if (effectiveOutputMode == OUTPUT_XBOXONE) {
    configure_xboxone_output_runtime();
  #endif
  } else if (effectiveOutputMode == OUTPUT_PS3) {
    configure_ps3_output_runtime();
  } else if (effectiveOutputMode == OUTPUT_PS4) {
    configure_ps4_output_runtime();
  #ifdef ENABLE_OUTPUT_PS5
  } else if (effectiveOutputMode == OUTPUT_PS5) {
    configure_ps5_output_runtime();
  #endif
  } else if (effectiveOutputMode == OUTPUT_SWITCH) {
    configure_switch_output_runtime();
  } else if (effectiveOutputMode == OUTPUT_SWITCHPRO) {
    configure_switchpro_output_runtime();
  } else if (effectiveOutputMode == OUTPUT_SWITCH2PRO) {
    configure_switch2pro_output_runtime();
  } else if (effectiveOutputMode == OUTPUT_PANTHERLORD) {
    configure_pantherlord_output_runtime();
  } else if (effectiveOutputMode == OUTPUT_GCWIIU) {
    configure_gcwiiu_output_runtime();
  } else if (effectiveOutputMode == OUTPUT_MDMINI) {
    configure_mdmini_output_runtime();
#ifdef ENABLE_OUTPUT_JVS
  } else if (effectiveOutputMode == OUTPUT_JVS) {
    use_device_hid_class = false;
    jvsOutput.begin();
    return;
#endif
  }

  prepare_manual_tinyusb_output_init_runtime();
  begin_output_dreamcast_debug_cdc_runtime();
  begin_output_usb_interfaces_runtime();
  // Last, so the gamepad interface numbers are the same with the drive on or off.
  usb_drive_configure(effectiveOutputMode);

  configureProductXinputTransportRuntime();

  #if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
  if (lateTinyUsbInit) {
    TinyUSB_Port_InitDevice(0);
    tud_disconnect();
  }
  #endif
}
