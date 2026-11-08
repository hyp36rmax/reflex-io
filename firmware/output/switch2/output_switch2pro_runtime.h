#pragma once

// Switch 2 Pro report delivery, header-only like the other persona runtimes
// so it can use the USB objects and mapping helpers owned by out_usb.h.

#include "output_switch2pro_source_port.h"

static SwitchReport _switch2pro_report = {};

inline void queuePeriodicSwitch2ProReports(uint32_t now) {
  const bool active = get_effective_output_mode() == OUTPUT_SWITCH2PRO &&
      _switch2pro && _switch2pro->responder().reportsEnabled() &&
      tud_mounted() && !tud_suspended();
  if (!active) {
    switch2pro_report_stream.reset();
  } else if (switch2pro_report_stream.due(now)) {
    // Queue before preparation, like the Switch Pro stream, so the report
    // carries remaps and held shortcut outputs.
    requestControllerFrameDelivery(switch2pro_output_source_port());
  }
}

// Sends one report for the source frame; true when it was submitted.
inline bool send_switch2pro_report(uint8_t sourcePort) {
  if (!_switch2pro || !_switch2pro->inputReady()) return false;
  map_switchpro_report(_switch2pro_report, sourcePort, sourcePort);
  uint32_t buttons =
      switch2_pro::buttonsFromSwitchProBytes(_switch2pro_report.buttons);
#if defined(ADAPT_BRIDGE_USB_TO_USB)
  // Only Modern2USB inputs have GameChat (C) and grip (GL/GR) buttons.
  buttons |= switch2_pro::buttonsFromNeutralExtras(
      controllerFrameConst(sourcePort).EXTRA);
#endif
  if (!_switch2pro->sendInputReport(sourcePort, buttons, _switch2pro_report.lstick,
                                    _switch2pro_report.rstick)) {
    return false;
  }
  switch2pro_report_stream.submitted(millis());
  return true;
}
