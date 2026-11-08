#include "Input_Cdi.h"
#include "input_cdi_receiver.h"
#include "input_cdi_runtime_state.h"
#include "../../core/classic_mouse_motion.h"
#include "../../core/classic_mouse_source.h"

namespace {

// Bounds one poll's FIFO drain (the joined RX FIFO holds 8 characters).
constexpr uint8_t kMaxCharactersPerPoll = 16;

bool sameOutput(const cdi_protocol::Output& a, const cdi_protocol::Output& b) {
  return a.connected == b.connected && a.device == b.device &&
         a.buttons == b.buttons && a.up == b.up && a.down == b.down &&
         a.left == b.left && a.right == b.right && a.lx == b.lx &&
         a.ly == b.ly && a.mouseX == b.mouseX && a.mouseY == b.mouseY;
}

void storeDebug(uint8_t port, const cdi_protocol::Port& state) {
  uint8_t debug[16] = {0};
  debug[0] = 1;
  debug[1] = port;
  debug[2] = (uint8_t)state.phase;
  debug[3] = (uint8_t)state.device;
  debug[4] = state.baudIndex;
  debug[5] = (uint8_t)state.ids;
  debug[6] = (uint8_t)state.packets;
  debug[7] = (uint8_t)state.framingErrors;
  debug[8] = (uint8_t)state.rejectedCharacters;
  debug[9] = state.buttons;
  debug[10] = (uint8_t)state.heldX;
  debug[11] = (uint8_t)state.heldY;
  debug[12] = cdi_receiver_failed[port] ? 1 : 0;
  webhid_store_raw_data(debug, 16);
}

}  // namespace

bool RZInputCdi::poll() {
  beginPollCycle();

  for (uint8_t port = 0; port < input_ports; ++port) {
    const input_cdi_config_t& pins = input_cdi_config[port];
    cdi_protocol::Port& state = cdi_ports[port];
    const uint32_t now = micros();

    int character = -1;
    uint8_t drained = 0;
    while (drained++ < kMaxCharactersPerPoll &&
           (character = cdi_receiver::read(port)) >= 0) {
      cdi_protocol::onCharacter(state, (uint8_t)character, now);
    }
    if (cdi_receiver::takeFramingError(port)) {
      cdi_protocol::onFramingError(state, now);
    }

    const bool wasAsserted = cdi_protocol::rtsAsserted(state);
    const bool restart =
        cdi_protocol::tick(state, cdi_receiver::lineAtMark(pins.rxd), now);
    if (cdi_protocol::rtsAsserted(state) != wasAsserted) {
      setRts(port, cdi_protocol::rtsAsserted(state));
    }
    if (restart) {
      cdi_receiver::restart(port, cdi_protocol::baudRate(state));
    }

    const cdi_protocol::Output out = cdi_protocol::output(state, now);
    if (port == 0) storeDebug(port, state);
    if (port >= MAX_USB_OUT) continue;

    const bool mouse = out.connected && out.device == cdi_protocol::Device::Relative;
    if (out.connected != cdi_last_output[port].connected ||
        out.device != cdi_last_output[port].device) resetClassicMouseMotion(port);
    setClassicMouseInput(port, mouse);
    // output() consumes the protocol accumulator. Equal successive deltas are
    // separate events; only the persistent button/stick frame is deduplicated.
    const bool motion = mouse && (out.mouseX != 0 || out.mouseY != 0);
    if (motion) queueClassicMouseMotion(port, out.mouseX, out.mouseY);
    if (!motion && sameOutput(out, cdi_last_output[port])) continue;

    if (out.connected != cdi_last_output[port].connected ||
        out.device != cdi_last_output[port].device) {
      setInputFrameTypeName(inputFrame(port), cdi_protocol::deviceName(out.device));
      setPortLed(port, out.connected ? HIGH : LOW);
    }
    cdi_last_output[port] = out;

    resetState(port);
    controller_state_t& frame = inputFrame(port);
    setInputFrameConnected(frame, out.connected);
    frame.HAS_ANALOG_STICK_MAIN = !mouse;
    frame.A = (out.buttons & cdi_protocol::kButton1) ? 1 : 0;
    frame.B = (out.buttons & cdi_protocol::kButton2) ? 1 : 0;
    frame.PAD_U = out.up;
    frame.PAD_D = out.down;
    frame.PAD_L = out.left;
    frame.PAD_R = out.right;
    frame.LX = out.lx;
    frame.LY = out.ly;
    frame.mouse_x = out.mouseX;
    frame.mouse_y = out.mouseY;
    setUpdated(port);
  }

  return endPollCycle();
}
