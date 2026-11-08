#include "Input_Custom.h"
#include "../../product_config.h"
#if ADAPT_POLICY_DIRECT_BOARD_CONTROLS
#include "input_custom_board_controls.h"
#endif

const input_custom_config_t input_custom_config[ADAPT_DIRECT_INPUT_PORTS] = {
  {
#if defined(ADAPT_INPUT_DIRECT_GPIO)
    .buttons = {
      .up = PIN_DIRECT_UP, .down = PIN_DIRECT_DOWN,
      .left = PIN_DIRECT_LEFT, .right = PIN_DIRECT_RIGHT,
      .a = PIN_DIRECT_A, .b = PIN_DIRECT_B,
      .x = PIN_DIRECT_X, .y = PIN_DIRECT_Y,
      .l1 = PIN_DIRECT_L1, .r1 = PIN_DIRECT_R1,
      .l2 = PIN_DIRECT_L2, .r2 = PIN_DIRECT_R2,
      .l3 = PIN_DIRECT_L3, .r3 = PIN_DIRECT_R3,
      .select = PIN_DIRECT_SELECT, .start = PIN_DIRECT_START,
      .home = PIN_DIRECT_HOME, .capture = PIN_DIRECT_CAPTURE,
      .extra0 = PIN_DIRECT_EXTRA0, .extra1 = PIN_DIRECT_EXTRA1,
      .extra2 = PIN_DIRECT_EXTRA2, .extra3 = PIN_DIRECT_EXTRA3,
      .extra4 = PIN_DIRECT_EXTRA4
    },
    .sticks = { .stick_lx = PIN_DIRECT_LX, .stick_ly = PIN_DIRECT_LY,
                .stick_rx = PIN_DIRECT_RX, .stick_ry = PIN_DIRECT_RY },
#else
    .buttons = {
      .up = 2, .down = 3, .left = 5, .right = 4,
      .a = 6, .b = 7, .x = 10, .y = 11,
      .l1 = 18, .r1 = 19, .l2 = -1, .r2 = -1,
      .l3 = -1, .r3 = -1, .select = 16, .start = 17,
      .home = -1, .capture = -1,
      .extra0 = -1, .extra1 = -1, .extra2 = -1,
      .extra3 = -1, .extra4 = -1
    },
    .sticks = { .stick_lx = -1, .stick_ly = -1, .stick_rx = -1, .stick_ry = -1 },
#endif
#if defined(ADAPT_INPUT_DIRECT_GPIO)
    .debounceMs = ADAPT_DIRECT_DEBOUNCE_MS
#else
    .debounceMs = 8
#endif
  },
#if defined(PRODUCT_MISTERCADE_2P_SIMPLE)
  {
    .buttons = {
      .up = PIN_DIRECT_P2_UP, .down = PIN_DIRECT_P2_DOWN,
      .left = PIN_DIRECT_P2_LEFT, .right = PIN_DIRECT_P2_RIGHT,
      .a = PIN_DIRECT_P2_A, .b = PIN_DIRECT_P2_B,
      .x = PIN_DIRECT_P2_X, .y = PIN_DIRECT_P2_Y,
      .l1 = PIN_DIRECT_P2_L1, .r1 = PIN_DIRECT_P2_R1,
      .l2 = PIN_DIRECT_P2_L2, .r2 = PIN_DIRECT_P2_R2,
      .l3 = -1, .r3 = -1,
      .select = PIN_DIRECT_P2_SELECT, .start = PIN_DIRECT_P2_START,
      .home = -1, .capture = -1,
      .extra0 = -1, .extra1 = -1, .extra2 = -1,
      .extra3 = -1, .extra4 = -1
    },
    .sticks = { .stick_lx = -1, .stick_ly = -1,
                .stick_rx = -1, .stick_ry = -1 },
    .debounceMs = ADAPT_DIRECT_DEBOUNCE_MS
  },
#endif
};

RZInputCustom::RZInputCustom() : RZInputModule() {}

const char* RZInputCustom::getUsbId() {
  return "RZRCUSTOM";
}

void RZInputCustom::configureBcdDeviceVersion() {
  bcd_device_version.platform = 0;
  bcd_device_version.platform_sub = 0;
}

const char* RZInputCustom::getDescription() {
  return "CUSTOM";
}

void RZInputCustom::setup() {
  pollInterval = 250;
  setInputPortCount(input_ports);

  for (uint8_t i = 0; i < input_ports; ++i) {
    input_custom_config_t c = input_custom_config[i];
    uint32_t buttonsMask = 0;
    for (uint8_t j = 0; j < sizeof(c.btnArray); ++j) {
      if (c.btnArray[j] < 32)
        buttonsMask |= (1UL << c.btnArray[j]);
    }
#if ADAPT_POLICY_DIRECT_BOARD_CONTROLS
    buttonsMask |= directBoardInputPinMask();
#endif
    debouncer[i] = new ButtonDebouncer(buttonsMask, c.debounceMs);

    for (uint8_t k = 0; k < sizeof(c.sticksArray); ++k) {
      if (c.sticksArray[k] < 32)
        pinMode(c.sticksArray[k], INPUT);
    }

    if (i < MAX_USB_OUT) {
      controller_state_t& frame = inputFrame(i);
      frame.HAS_BTN_SELECT = input_custom_config[i].buttons.select != -1;
      frame.HAS_BTN_START = input_custom_config[i].buttons.start != -1;
      frame.HAS_BTN_HOME = input_custom_config[i].buttons.home != -1;
      frame.HAS_ANALOG_STICK_MAIN = input_custom_config[i].sticks.stick_lx != -1;
      frame.HAS_ANALOG_STICK_AUX = input_custom_config[i].sticks.stick_rx != -1;
      frame.sticks_precision_bits = ANALOG_STICK_PRECISION_12;
#ifdef ADAPT_DIRECT_CONTROLLER_NAME
      setInputFrameTypeName(frame, ADAPT_DIRECT_CONTROLLER_NAME);
#endif
      setInputFrameConnected(frame, true);
    }

    analogReadResolution(12);
  }
}

void RZInputCustom::setup2() {
  for (uint8_t i = 0; i < input_ports; ++i)
    setPortLed(i, HIGH);
#if ADAPT_POLICY_DIRECT_BOARD_CONTROLS
  directBoardControlsBegin();
#endif
}

uint16_t RZInputCustom::tryReadAnalog(uint8_t pin) {
  return pin == static_cast<uint8_t>(-1) ? 0 : analogRead(pin);
}
