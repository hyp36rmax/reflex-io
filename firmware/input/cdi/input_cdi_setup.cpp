#include "Input_Cdi.h"
#include "input_cdi_receiver.h"
#include "input_cdi_runtime_state.h"

const input_cdi_config_t input_cdi_config[2] = {
  { .rxd = HDMI_1_02, .rts = HDMI_1_06 },
  { .rxd = HDMI_2_02, .rts = HDMI_2_06 },
};

RZInputCdi::RZInputCdi() : RZInputModule() {}

RZInputCdi::~RZInputCdi() {
  for (uint8_t port = 0; port < input_ports; ++port) {
    cdi_receiver::stop(port);
    setRts(port, true);
  }
}

// RTS is pulled low or released to the board's 5 V pull-up; its output latch
// stays 0, so it is never driven high.
void RZInputCdi::setRts(uint8_t port, bool asserted) {
  const uint8_t pin = input_cdi_config[port].rts;
  if (asserted) {
    gpio_set_dir(pin, GPIO_IN);
  } else {
    gpio_put(pin, 0);
    gpio_set_dir(pin, GPIO_OUT);
  }
}

const char* RZInputCdi::getUsbId() {
  return "RZRCDI";
}

void RZInputCdi::configureBcdDeviceVersion() {
  bcd_device_version.platform = BCD_CDI_PLATFORM;
  bcd_device_version.platform_sub = BCD_CDI_PLATFORM_SUB;
}

const char* RZInputCdi::getDescription() {
  return "CD-I";
}

void RZInputCdi::setup() {
  pollInterval = 1000;
  setInputPortCount(input_ports);
  const uint32_t now = micros();

  for (uint8_t port = 0; port < input_ports; ++port) {
    const input_cdi_config_t& pins = input_cdi_config[port];
    gpio_init(pins.rts);  // input, output latch 0
    gpio_put(pins.rts, 0);
    gpio_pull_up(pins.rts);

    cdi_protocol::reset(cdi_ports[port], now);
    cdi_last_output[port] = cdi_protocol::Output{};
    setRts(port, cdi_protocol::rtsAsserted(cdi_ports[port]));
    cdi_receiver_failed[port] =
        !cdi_receiver::start(port, pins.rxd, cdi_protocol::baudRate(cdi_ports[port]));

    if (port < MAX_USB_OUT) {
      controller_state_t& frame = inputFrame(port);
      frame.HAS_ANALOG_STICK_MAIN = 1;  // pointer motion as stick deflection
      setInputFrameTypeName(frame, cdi_receiver_failed[port] ? "CD-i no PIO" : "CD-i");
      setInputFrameConnected(frame, false);
    }
  }
}

void RZInputCdi::setup2() {
  for (uint8_t i = 0; i < input_ports; ++i)
    setPortLed(i, LOW);
}
