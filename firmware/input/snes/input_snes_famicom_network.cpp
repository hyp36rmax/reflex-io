#include "Input_Snes.h"
#include "famicom_network_reader.h"
#include "../../core/device_runtime_state.h"

namespace {

struct FamicomNetworkBus {
  const input_snes_config_t& pins;
  void clock(bool high) { digitalWrite(pins.clk, high ? HIGH : LOW); }
  void latch(bool high) { digitalWrite(pins.lat, high ? HIGH : LOW); }
  bool data() { return digitalRead(pins.dat0); }
  void waitUs(unsigned us) { delayMicroseconds(us); }
};

constexpr unsigned kRuntimeHalfPeriodUs = 6;

}  // namespace

bool RZInputSnes::famicomNetworkEligible(uint8_t port) {
  // The HVC-051 is a Famicom expansion-port device; NES and SNES modes both
  // read that adapter's pins. Accessory and multitap paths keep the port.
  const uint8_t mode = getInternalMode();
  return (mode == 1 || mode == 2) && port < input_ports && snes[port] != nullptr &&
         !snes[port]->getMultitapPorts() && !snes[port]->isPowerPadMode() &&
         !snes[port]->isZapperMode();
}

// True while the port holds an HVC-051; its report is then in snes_fcnet[port].
// An idle one reads as an empty port to SnesLib, so only ports with nothing
// identified are probed.
bool RZInputSnes::tryPollFamicomNetwork(uint8_t port) {
  if (!fcnet_active[port] && is_port_connected(port)) return false;
  SnesController& pad = snes_fcnet[port];
  const uint32_t now = micros();
  if (int32_t(now - fcnet_next_read_us[port]) < 0) {
    if (!fcnet_active[port]) return false;
    pad.copyCurrentToLast();
    return true;
  }
  FamicomNetworkBus bus{input_snes_config[port]};
  const uint32_t levels = famicom_network::readLevels(bus, kRuntimeHalfPeriodUs);
  pad.copyCurrentToLast();
  if (famicom_network::signature(levels)) {
    const famicom_network::NttShapedReport report =
        famicom_network::toNttShape(famicom_network::decodePressed(levels));
    pad.currentState.id = 0x2;  // NTT Data; see famicom_network_reader.h
    pad.currentState.digital = report.digital;
    pad.currentState.extended = report.extended;
    fcnet_active[port] = true;
    fcnet_misses[port] = 0;
  } else if (fcnet_active[port] && ++fcnet_misses[port] >= FCNET_LEAVE_MISSES) {
    // A few bad reads keep the last report; an unplug drops it.
    fcnet_active[port] = false;
    fcnet_misses[port] = 0;
  }
  fcnet_next_read_us[port] =
      now + (fcnet_active[port] ? FCNET_ACTIVE_POLL_US : FCNET_PROBE_INTERVAL_US);
  return fcnet_active[port];
}

void RZInputSnes::resetFamicomNetworkPorts() {
  for (uint8_t port = 0; port < input_ports; ++port) {
    snes_fcnet[port].reset(true, true);
    fcnet_active[port] = false;
    fcnet_misses[port] = 0;
    fcnet_next_read_us[port] = 0;
  }
}

void printFamicomNetworkRawDump(Print& out) {
  if (deviceMode != RZORD_NES && deviceMode != RZORD_SNES) {
    out.println(F("ERR:FCNET_NEEDS_NES_OR_SNES_INPUT"));
    return;
  }
  static constexpr unsigned kHalfPeriodsUs[] = {1, 3, 6, 12};
  const uint8_t ports = sizeof(input_snes_config) / sizeof(input_snes_config[0]);
  for (uint8_t port = 0; port < ports; ++port) {
    FamicomNetworkBus bus{input_snes_config[port]};
    for (unsigned halfUs : kHalfPeriodsUs) {
      const uint32_t levels = famicom_network::readLevels(bus, halfUs);
      out.print(F("FCNET P"));
      out.print(port);
      out.print(F(" HALF_US="));
      out.print(halfUs);
      out.print(F(" LEVELS=0x"));
      out.print(levels, HEX);
      out.print(F(" SIG="));
      out.print(famicom_network::signature(levels) ? 1 : 0);
      out.print(F(" PRESSED=0x"));
      out.println(famicom_network::decodePressed(levels), HEX);
    }
  }
  out.println(F("OK:FCNET"));
}
