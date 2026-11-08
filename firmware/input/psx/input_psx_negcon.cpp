#include "Input_Psx.h"
#include "psx_negcon_pressure.h"

#include "../../core/controller_settings_state.h"
#include "../../core/negcon_input_modes.h"
#include "../../core/rotary_runtime_policy.h"
#include "../../menu/menu_ui_state.h"

void RZInputPSX::negconSetup() {
  for (uint8_t port = 0; port < input_ports; ++port) {
    negconSpinner[port].reset(port);
    inputFrame(port).HAS_BTN_SELECT = 0;
    negcon_origin_valid[port] = false;
    negcon_origin_twist[port] = 0x80;
  }
}

void RZInputPSX::negconSetup2() {
  output_try_enable_psx_negcon_mode();
}

void RZInputPSX::loopNeGcon(uint8_t i) {
  if (!negcon_inited) {
    for (uint8_t port = 0; port < input_ports; ++port) {
      negcon_last_x[port]      = ANALOG_IDLE_VALUE;
      negcon_last_z[port]      = ANALOG_MAX_VALUE;
      negcon_last_cross[port]  = ANALOG_MAX_VALUE;
      negcon_last_square[port] = ANALOG_MAX_VALUE;
    }
    negcon_inited = true;
  }

  byte lx, ly;
  psx[i]->getLeftAnalog (lx, ly);

  const RotaryTuning tuning = currentRotaryTuning(RotaryInputKind::NeGcon);
  // MiSTer Spinner and Mouse take the twist as relative motion.
  negconSpinner[i].sample(i, lx,
      rotaryOutputQueuesMotion(RotaryInputKind::NeGcon, get_effective_output_mode()) &&
      psx[i]->getProtocol() == PSPROTO_NEGCON && !menuOwnsControllerInput(),
      tuning.gain, tuning.reverse);

  const bool analogTriggers = canUseAnalogTrigger();
  const bool isXinput = analogTriggers && !output_effective_mode_is_psx_negcon();

  const uint8_t raw_z = psx[i]->getAnalogButton(PSAB_L1);
  const uint8_t raw_cross = psx[i]->getAnalogButton(PSAB_CROSS);
  const uint8_t raw_square = psx[i]->getAnalogButton(PSAB_SQUARE);
  const uint8_t z = negconAxisValue(raw_z, true);
  const uint8_t cross = negconAxisValue(raw_cross, !isXinput);
  const uint8_t square = negconAxisValue(raw_square, !isXinput);
  controller_state_t& frame = inputFrame(i);

  int16_t centeredTwist = (int16_t)lx - (int16_t)negcon_origin_twist[i];
  uint8_t twistPaddle = lx;
  if ((stick_invert & 0x01) != 0) {
    centeredTwist = -centeredTwist;
    const int16_t reflectedPaddle = (int16_t)negcon_origin_twist[i] + centeredTwist;
    twistPaddle = (uint8_t)constrain(reflectedPaddle, 0, 255);
  }

  frame.HAS_ANALOG_STICK_MAIN = true;
  frame.HAS_ANALOG_MAIN_BUTTONS = true;
  frame.LX = centeredTwist;
  frame.paddle = twistPaddle;
  frame.ANALOG_A = raw_cross;
  frame.ANALOG_X = raw_square;
  frame.ANALOG_L1 = raw_z;
  frame.PAD_U = psx[i]->buttonPressed(PSB_PAD_UP);
  frame.PAD_D = psx[i]->buttonPressed(PSB_PAD_DOWN);
  frame.PAD_L = psx[i]->buttonPressed(PSB_PAD_LEFT);
  frame.PAD_R = psx[i]->buttonPressed(PSB_PAD_RIGHT);
  // The neGcon has no I/II/L switches; each press is made up at half travel.
  // PsxNewLib uses 240 for L, so only the calibration chord below keeps that.
  frame.L1    = raw_z >= NEGCON_INPUT_PRESS_THRESHOLD;
  frame.R1    = psx[i]->buttonPressed(PSB_R1);
  frame.START = psx[i]->buttonPressed(PSB_START);
  frame.A     = raw_cross >= NEGCON_INPUT_PRESS_THRESHOLD;
  frame.X     = raw_square >= NEGCON_INPUT_PRESS_THRESHOLD;

  if (false) {
  } else {
    if (analogTriggers) {
      frame.HAS_ANALOG_TRIGGERS = true;
      frame.ANALOG_L2 = square;
      frame.ANALOG_R2 = cross;

      if (output_effective_mode_uses_psx_negcon_extended_axes()) {
        frame.RX = z;
      }

    } else {
      frame.HAS_ANALOG_TRIGGERS = false;
    }
    frame.B  = psx[i]->buttonPressed(PSB_CIRCLE);
    frame.Y  = psx[i]->buttonPressed(PSB_TRIANGLE);
  }

  if (psx[i]->buttonPressed(PSB_L1) && psx[i]->buttonPressed(PSB_R1) && psx[i]->buttonPressed(PSB_CIRCLE) && psx[i]->buttonPressed(PSB_TRIANGLE)) {
    frame.L1 = frame.R1 = frame.B = frame.Y = 0;
    frame.ANALOG_L1 = 0;
    if (psx[i]->buttonPressed(PSB_PAD_UP)) {
      negcon_origin_twist[i] = lx;
      frame.PAD_U = 0;
    } else if (psx[i]->buttonPressed(PSB_PAD_DOWN)) {
      negcon_maxpos[i] = frame.LX < 0 ? (frame.LX * -1) : frame.LX;
      frame.PAD_D = 0;
    }
  }

  syncNegconDigitalButtonPressure(frame);

  if (negcon_maxpos[i] > 0 && frame.LX != 0) {
    // Full-width math: the twist and calibrated range reach 255, so int8
    // casts wrapped and a 128 range collapsed map() to a divide by zero.
    const long maxpos = negcon_maxpos[i];
    const long twist = constrain((long)frame.LX, -maxpos, maxpos);
    frame.LX = map(twist, -maxpos, maxpos, INT8_MIN, INT8_MAX);
  }

  negcon_last_x[i]      = lx;
  negcon_last_z[i]      = z;
  negcon_last_cross[i]  = cross;
  negcon_last_square[i] = square;
}
