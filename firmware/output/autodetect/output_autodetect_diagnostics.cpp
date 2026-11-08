#include "output_autodetect_diagnostics.h"

#include "output_autodetect_rp2040_usb_debug.h"
#include "../output_runtime_state.h"

void appendOutputAutodetectDiagnostics(Print& out) {
  out.print(F("AUTO_STATE="));
  out.print((int)autoDetectState);
  out.print(F(" PROBE="));
  out.print((int)autoDetectProbeStage);
  out.print(F(" TRANS="));
  out.print((int)output_autodetect_transition_reason);
  out.print(F(" MS="));
  out.print((int)output_autodetect_last_ms);
  out.print(F(" FLAGS=0x"));
  out.print((int)output_autodetect_last_flags, HEX);
  out.print(F(" AUX=0x"));
  out.print((int)output_autodetect_aux_flags, HEX);
  out.print(F(" COUNTERS=0x"));
  out.print((int)output_autodetect_probe_counters, HEX);
  out.print(F(" LIVE_USBHW=0x"));
  out.print((int)output_usb_device_live_hw_flags(), HEX);
  out.print(F(" LIVE_USBIRQ="));
  out.print((int)output_autodetect_usb_irq_observed);
  out.print(F(" LIVE_USBIRQS=0x"));
  out.print((int)output_usb_device_live_irq_flags(), HEX);
}

void printOutputAutodetectDiagnostics(Print& out, const char* phase) {
  out.print(F("AUTODETECT_DIAG"));
  if (phase != nullptr && *phase != '\0') {
    out.print(F(" PHASE="));
    out.print(phase);
  }
  out.print(' ');
  appendOutputAutodetectDiagnostics(out);
  out.println();
}
