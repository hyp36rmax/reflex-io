#include "Input_ColecoVision.h"

const input_colecovision_config_t input_colecovision_config[2] = {
  { .up = HDMI_1_01, .down = HDMI_1_02, .left = HDMI_1_03, .right = HDMI_1_04,
    .fire = HDMI_1_05, .keypadSelect = HDMI_1_06, .joystickSelect = HDMI_1_07,
    .rollerA = HDMI_1_10, .rollerB = HDMI_1_11 },
  { .up = HDMI_2_01, .down = HDMI_2_02, .left = HDMI_2_03, .right = HDMI_2_04,
    .fire = HDMI_2_05, .keypadSelect = HDMI_2_06, .joystickSelect = HDMI_2_07,
    .rollerA = HDMI_2_10, .rollerB = HDMI_2_11 },
};

namespace {

// Settling time after switching selects: the level shifters' 10k pull-ups,
// the cable and the keypad diodes. Unmeasured; check on a scope.
constexpr uint32_t kSelectSettleUs = 20;

// A select line is either pulled low or released to the board's pull-up. Its
// output latch stays 0, so switching it to an output can only drive low.
void releaseSelect(uint8_t pin) {
  gpio_set_dir(pin, GPIO_IN);
}

void assertSelect(uint8_t pin) {
  gpio_put(pin, 0);
  gpio_set_dir(pin, GPIO_OUT);
}

void initSelect(uint8_t pin) {
  gpio_init(pin);  // input, output latch 0
  gpio_put(pin, 0);
  gpio_pull_up(pin);
}

uint8_t readNibble(const input_colecovision_config_t& pins) {
  uint8_t nibble = 0;
  if (!gpio_get(pins.up)) nibble |= 0x01;
  if (!gpio_get(pins.right)) nibble |= 0x02;
  if (!gpio_get(pins.down)) nibble |= 0x04;
  if (!gpio_get(pins.left)) nibble |= 0x08;
  return nibble;
}

}  // namespace

RZInputColecoVision::RZInputColecoVision() : RZInputModule() {}

RZInputColecoVision::~RZInputColecoVision() {
  for (const input_colecovision_config_t& pins : input_colecovision_config) {
    releaseSelect(pins.keypadSelect);
    releaseSelect(pins.joystickSelect);
  }
}

const char* RZInputColecoVision::getUsbId() {
  return "RZRCOLECO";
}

void RZInputColecoVision::configureBcdDeviceVersion() {
  bcd_device_version.platform = BCD_PLAT_COLECOVISION;
  bcd_device_version.platform_sub = 0;
}

const char* RZInputColecoVision::getDescription() {
  return "COLECOVISION";
}

void RZInputColecoVision::setup() {
  pollInterval = 1000;
  setInputPortCount(input_ports);

  for (uint8_t p = 0; p < input_ports; ++p) {
    const input_colecovision_config_t& pins = input_colecovision_config[p];

    const uint8_t inputs[] = {pins.up, pins.down, pins.left, pins.right, pins.fire,
                              pins.rollerA, pins.rollerB};
    for (uint8_t pin : inputs) {
      gpio_init(pin);
      gpio_pull_up(pin);
    }
    initSelect(pins.keypadSelect);
    initSelect(pins.joystickSelect);
    // Idle like the console: one select always low, the joystick's here.
    assertSelect(pins.joystickSelect);

    if (p < MAX_USB_OUT) {
      controller_state_t& frame = inputFrame(p);
      frame.HAS_BTN_SELECT = 1;
      frame.HAS_BTN_START = 1;
      frame.HAS_ANALOG_STICK_MAIN = 1;  // the joystick
      setInputFrameConnected(frame, true);
    }
  }
}

void RZInputColecoVision::setup2() {
  for (uint8_t i = 0; i < input_ports; ++i)
    setPortLed(i, HIGH);
}

// Bits 0-3: joystick nibble, 4-7: keypad nibble, 8: left fire, 9: right fire.
uint16_t RZInputColecoVision::readController(uint8_t port) {
  const input_colecovision_config_t& pins = input_colecovision_config[port];

  // The joystick select has been low and settled since the previous scan.
  const uint8_t joystick = readNibble(pins);
  const bool leftFire = !gpio_get(pins.fire);

  releaseSelect(pins.joystickSelect);
  assertSelect(pins.keypadSelect);
  delayMicroseconds(kSelectSettleUs);
  const uint8_t keypad = readNibble(pins);
  const bool rightFire = !gpio_get(pins.fire);

  releaseSelect(pins.keypadSelect);
  assertSelect(pins.joystickSelect);

  return (uint16_t)joystick | ((uint16_t)keypad << 4) |
         ((uint16_t)leftFire << 8) | ((uint16_t)rightFire << 9);
}
