#include "Input_ColecoVision.h"
#include "input_colecovision_decode.h"
#include "input_colecovision_runtime_state.h"

bool RZInputColecoVision::poll() {
  beginPollCycle();

  for (uint8_t port = 0; port < input_ports; ++port) {
    const uint16_t raw = readController(port);
    // Accept a state once two consecutive polls agree (one poll of latency).
    const bool stable = raw == colecovision_pending_state[port];
    colecovision_pending_state[port] = raw;
    if (!stable || raw == colecovision_last_state[port]) continue;

    const colecovision_decode::Scan scan{
      static_cast<uint8_t>(raw & 0x0F),
      static_cast<uint8_t>((raw >> 4) & 0x0F),
      (raw & 0x100) != 0,
      (raw & 0x200) != 0,
    };
    resetState(port);
    controller_state_t& frame = inputFrame(port);
    const colecovision_decode::Result decoded = colecovision_decode::decode(scan);
    frame.digital_buttons |= decoded.buttons;
    frame.LX = decoded.lx;
    frame.LY = decoded.ly;
    colecovision_last_state[port] = raw;
    setUpdated(port);

    if (port == 0) {
      uint8_t debug[16] = {0};
      debug[0] = 1;
      debug[1] = scan.joystick;
      debug[2] = scan.keypad;
      debug[3] = scan.leftFire ? 1 : 0;
      debug[4] = scan.rightFire ? 1 : 0;
      debug[5] = port;
      webhid_store_raw_data(debug, 16);
    }
  }

  return endPollCycle();
}
