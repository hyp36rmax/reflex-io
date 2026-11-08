#include "input_cdi_debug.h"

#include "../../core/device_runtime_state.h"
#include "Input_Cdi.h"
#include "input_cdi_receiver.h"
#include "input_cdi_runtime_state.h"

namespace {

const __FlashStringHelper* phaseName(cdi_protocol::Phase phase) {
  switch (phase) {
    case cdi_protocol::Phase::RtsNegated: return F("RTS_LOW");
    case cdi_protocol::Phase::AwaitId: return F("WAIT_ID");
    case cdi_protocol::Phase::Active: return F("ACTIVE");
  }
  return F("?");
}

char deviceLetter(cdi_protocol::Device device) {
  switch (device) {
    case cdi_protocol::Device::Relative: return 'M';
    case cdi_protocol::Device::Maneuvering: return 'J';
    case cdi_protocol::Device::Absolute: return 'T';
    case cdi_protocol::Device::AbsoluteScreen: return 'S';
    default: return '-';
  }
}

}  // namespace

void printCdiStatus(Print& out) {
  out.print(F("CDISTAT ACTIVE="));
  out.println(deviceMode == RZORD_CDI ? 1 : 0);
  for (uint8_t port = 0; port < 2; ++port) {
    const cdi_protocol::Port& state = cdi_ports[port];
    out.print(F("CDISTAT P="));
    out.print((int)port);
    out.print(F(" PIO="));
    out.print(cdi_receiver::running(port) ? 1 : 0);
    out.print(F(" PHASE="));
    out.print(phaseName(state.phase));
    out.print(F(" DEV="));
    out.print(deviceLetter(state.device));
    out.print(F(" BAUD="));
    out.print((unsigned long)cdi_protocol::baudRate(state));
    out.print(F(" RTS="));
    out.print(cdi_protocol::rtsAsserted(state) ? 1 : 0);
    out.print(F(" LINE="));
    out.print(cdi_receiver::lineAtMark(input_cdi_config[port].rxd) ? F("MARK") : F("SPACE"));
    out.print(F(" IDS="));
    out.print((unsigned long)state.ids);
    out.print(F(" PKTS="));
    out.print((unsigned long)state.packets);
    out.print(F(" FRAMING="));
    out.print((unsigned long)state.framingErrors);
    out.print(F(" REJECT="));
    out.print((unsigned long)state.rejectedCharacters);
    out.print(F(" BTN="));
    out.print((int)state.buttons);
    out.print(F(" X="));
    out.print((int)state.heldX);
    out.print(F(" Y="));
    out.print((int)state.heldY);
    out.print(F(" CHARS="));
    out.print((unsigned long)state.characters);
    // The latest characters in hex, oldest first; !! is a framing error.
    out.print(F(" RAW="));
    for (uint8_t i = 0; i < state.recentCount; ++i) {
      const uint8_t entry = cdi_protocol::recentEntry(state, i);
      if (i != 0) out.print(' ');
      if (entry == cdi_protocol::kRecentFramingError) {
        out.print(F("!!"));
        continue;
      }
      if (entry < 0x10) out.print('0');
      out.print(entry, HEX);
    }
    out.println();
  }
  out.println(F("OK:CDISTAT"));
}
