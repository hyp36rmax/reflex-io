#include "Input_AtariDriving.h"
#include "../autodetect/input_autodetect_runtime_state.h"

#define DRIVING_CENTER 128

const input_driving_config_t input_driving_config[2] = {
  { .enc_a = HDMI_1_01, .enc_b = HDMI_1_02, .button = HDMI_1_05 },
  { .enc_a = HDMI_2_01, .enc_b = HDMI_2_02, .button = HDMI_2_05 },
};

RZInputDriving::RZInputDriving() : RZInputModule() {}

const char* RZInputDriving::getUsbId() { return "RZRDriv"; }

void RZInputDriving::configureBcdDeviceVersion() {
  bcd_device_version.platform = BCD_PLAT_DRIVING;
  bcd_device_version.platform_sub = 0;
}

const char* RZInputDriving::getDescription() { return "DRIVING CTRL"; }

void RZInputDriving::setup() {
  pollInterval = 100;
  setInputPortCount(input_ports);

  for (uint8_t p = 0; p < input_ports; ++p) {
    const input_driving_config_t& pins = input_driving_config[p];
    pinMode(pins.enc_a, INPUT_PULLUP);
    pinMode(pins.enc_b, INPUT_PULLUP);
    pinMode(pins.button, INPUT_PULLUP);
    lastState[p] = drivingReadEncoderState(pins.enc_a, pins.enc_b);
    lastButton[p] = drivingReadPin(pins.button);
    position[p] = DRIVING_CENTER;
    paddleDivider[p].reset();

    if (p < MAX_USB_OUT) {
      controller_state_t& frame = inputFrame(p);
      // Nothing senses an idle wheel, so one port starts connected (the one
      // Auto saw turning) and the other connects when it is first used.
      setInputFrameConnected(frame, p == (input_auto_resolve_port < input_ports
                                              ? input_auto_resolve_port : 0));
      frame.sticks_precision_bits = ANALOG_STICK_PRECISION_16;
      setInputFrameTypeName(frame, "Driving");
    }
  }
}

void RZInputDriving::setup2() {
  for (uint8_t i = 0; i < input_ports; ++i)
    setPortLed(i, HIGH);
}

int16_t RZInputDriving::getPosition(uint8_t port) { return (port < input_ports) ? position[port] : 0; }
