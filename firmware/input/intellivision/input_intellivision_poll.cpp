#include "Input_Intellivision.h"
#include "input_intellivision_decode.h"
#include "input_intellivision_runtime_state.h"

bool RZInputIntv::poll() {
  beginPollCycle();

  for (uint8_t port = 0; port < input_ports; ++port) {
    const uint8_t raw = readController(port);
    // Accept a state once two consecutive polls agree: membrane contacts
    // bounce, and this costs one poll of latency.
    const bool stable = raw == intellivision_pending_state[port];
    intellivision_pending_state[port] = raw;
    if (!stable || raw == intellivision_last_state[port]) continue;

    resetState(port);
    controller_state_t& frame = inputFrame(port);
    const intv_decode::Result decoded = intv_decode::decode(raw);
    frame.digital_buttons |= decoded.buttons;
    frame.LX = decoded.lx;
    frame.LY = decoded.ly;
    intellivision_last_state[port] = raw;
    setUpdated(port);

    if (port == 0) {
      uint8_t debug[16] = {0};
      debug[0] = 1;
      debug[1] = raw;
      debug[2] = raw & 0x1F;  // disc lines
      debug[3] = raw & 0xE0;  // side-button / key row lines
      debug[5] = port;
      webhid_store_raw_data(debug, 16);
    }
  }

  return endPollCycle();
}
