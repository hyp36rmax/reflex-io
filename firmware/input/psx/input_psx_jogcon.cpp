#include "Input_Psx.h"
#include "../../core/controller_settings_state.h"
#include "../../core/rotary_motion_state.h"
#include "../../core/rotary_runtime_policy.h"
#include "../../output/output_runtime_state.h"
#include "../../features/jogcon_capture/jogcon_capture.h"

namespace {

RotaryDivider jogconOutputDivider;
bool jogconOutputReverse = false;
uint8_t jogconOutputGain = 1;

int16_t clampJogconAxis(int16_t value) {
  if (value > 127) return 127;
  if (value < -128) return -128;
  return value;
}

int16_t centeredJogconPaddleAxis(uint8_t paddle) {
  // Absolute sensitivity is applied to the temporary output frame once.
  return (int16_t)paddle - 0x80;
}

uint8_t configuredJogconMode(bool mouseMode) {
  // MiSTer Spinner and Mouse take the Spinner mode's relative motion.
  if (rotaryOutputQueuesMotion(RotaryInputKind::JogCon, get_effective_output_mode())) return 0;
  if (get_effective_output_mode() == OUTPUT_MISTER_PADDLE) return 1;
  return mouseMode ? 0 : (menu_jogcon_mode <= 3 ? menu_jogcon_mode : 0);
}

int16_t configuredJogconSpinnerStep() {
  // Counter differences below are already converted into gained output units.
  return 1;
}

uint8_t configuredJogconForce() {
  return menu_jogcon_force == 1 || menu_jogcon_force == 3 ||
         menu_jogcon_force == 7 || menu_jogcon_force == 15
           ? menu_jogcon_force
           : 15;
}

bool consumeJogconModeTransition(bool validJogcon, bool digitalReport, bool& latched) {
  if (validJogcon) latched = false;
  if (validJogcon || !digitalReport || latched) return false;
  latched = true;
  return true;
}

}  // namespace

void RZInputPSX::update_jogcon_display_name() {
  const char* names[] = { "JogCon-S", "JogCon-P", "JogCon-W", "JogCon-Fake" };
  if (mode <= 3) {
    controller_state_t& frame = inputFrame(0);
    setInputFrameTypeName(frame, names[mode]);
  }
}

void RZInputPSX::queueJogconMotorStop() {
  ff = 0;
  psx[0]->setJogconMotorMode(JOGCON_DIR_NONE, JOGCON_CMD_NONE, 0);
}

void RZInputPSX::init_jogcon(bool forceAttempt) {
  const uint32_t now = millis();
  if (!forceAttempt && jogconInitAttempted &&
      (uint32_t)(now - jogconLastInitMs) < 1000u) return;
  jogconInitAttempted = true;
  jogconLastInitMs = now;
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
  jogconCaptureInit(0, jogconMotorReady, uint8_t(psx[0]->getProtocol()));
#endif
  // Never carry a direction/HOLD command through begin(), a mode switch,
  // failed configuration, or a replacement controller session.
  if (haveController[0]) stopRumble(0);
  queueJogconMotorStop();
  jogconMotorReady = false;
  if (psx[0]->begin(*psxControllerDriver[0])) {
    const bool newlyConnected = !haveController[0];
    haveController[0] = true;
    if (newlyConnected) armControllerStartupGrace(0);

    if (psx[0]->enterConfigMode()) {
      const bool analogReady = psx[0]->enableAnalogSticks();
      const bool motorReady = psx[0]->enableRumble();
      const bool exited = psx[0]->exitConfigMode();
      jogconMotorReady = analogReady && motorReady && exited;
    }
    const bool inputReady = psx[0]->read();
    jogconMotorReady = jogconMotorReady && inputReady &&
        psx[0]->getProtocol() == PSPROTO_JOGCON;
    jogconRuntimePrimed = false;
  }
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
  jogconCaptureInit(1, jogconMotorReady, uint8_t(psx[0]->getProtocol()));
#endif
}

bool RZInputPSX::handleJogconData() {
  bool stateChanged = false;
  const uint8_t selectedMode = configuredJogconMode(enableMouseMove);
  const bool modeChanged = mode != selectedMode;
  if (modeChanged) {
    mode = selectedMode;
    update_jogcon_display_name();
    // Stop the previous mode before reconfiguring. Like Adapt V1, Wheel uses
    // the controller's own HOLD servo, not a host-side constant-torque loop.
    init_jogcon(true);
  }
  force = configuredJogconForce();

  uint8_t jogPosition = 0;
  uint8_t jogRevolutions = 0;
  JogconDirection jogDirection = JOGCON_DIR_NONE;
  JogconCommand cmdResult = JOGCON_CMD_NONE;

  bool gotJogconData = psx[0]->getJogconData(jogPosition, jogRevolutions, jogDirection, cmdResult);
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
  jogcon_capture::Event captured{};
  captured.us = micros(); captured.position = jogPosition; captured.revolutions = jogRevolutions;
  captured.direction = uint8_t(jogDirection); captured.command = uint8_t(cmdResult);
  captured.previousCounter = jogcon_counter; captured.previousMotion = jogcon_prevcnt;
  captured.readOk = gotJogconData;
  if (gotJogconData) captured.flags |= jogcon_capture::ReadOk;
  if (jogDirection == JOGCON_DIR_OTHER) captured.flags |= jogcon_capture::BadDirection;
  if (jogDirection == JOGCON_DIR_MAX) captured.flags |= jogcon_capture::Overflow;
#endif
  const bool physicalModeTransition = consumeJogconModeTransition(
      gotJogconData, psx[0]->getProtocol() == PSPROTO_DIGITAL, jogconDigitalModeLatched);
  JogconCommand nextCmd = JOGCON_CMD_NONE;
  if(jogDirection == JOGCON_DIR_MAX) {
    nextCmd = JOGCON_CMD_DROP_REVOLUTIONS;
    jogDirection = jogcon_lastDirection;
    if (jogDirection == JOGCON_DIR_CW) {
      jogPosition = 254;
      jogRevolutions = 255;
    } else {
      jogPosition = 254;
      jogRevolutions = 0;
    }
    jogcon_prevcnt = 0;
    jogcon_cleancnt = 0;
    jogcon_counter = (jogRevolutions << 8) | jogPosition;
  }

  if(jogDirection != JOGCON_DIR_NONE)
    jogcon_lastDirection = jogDirection;

  jogcon_newcnt = (jogRevolutions << 8) | jogPosition;
  jogcon_newbtn = psx[0]->getButtonWord();
  jogcon_newbtn = (jogcon_newbtn & ~3) | ((jogcon_newbtn&1)<<2);

  // AUTO detection can destroy and recreate the PSX input module while the
  // JogCon remains attached. Prime the freshly reset runtime state from the
  // first valid report so an old revolution count cannot become a huge delta.
  if (gotJogconData && !jogconRuntimePrimed) {
    jogcon_counter = jogcon_newcnt;
    jogcon_prevcnt = 0;
    jogcon_cleancnt = 0;
    jogconRuntimePrimed = true;
    jogconOutputDivider.reset();
    resetRotaryMotion(0);
  }

  if (modeChanged) {
    jogconOutputDivider.reset();
    resetRotaryMotion(0);
    jogcon_prevcnt = 0;
    jogcon_cleancnt = 0;
    jogcon_counter = jogcon_newcnt;
    jogcon_pdlpos = sp_half;
    jogcon_spinnerAnalog = 0;
    jogcon_wheelCenterSet = false;
    jogcon_oldbtn = 0xFFFF;
    jogcon_oldpaddle = 0xFF;
    jogcon_oldspinner = INT8_MIN;
  }

  if(!gotJogconData) {
    jogconOutputDivider.reset();
    resetRotaryMotion(0);
    queueJogconMotorStop();
    jogconMotorReady = false;
    // Only a real digital-mode report can be the legacy physical MODE chord.
    // A malformed analog report must not change settings from stale buttons.
    if (!physicalModeTransition) {
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
      captured.mode = mode; captured.ready = jogconMotorReady;
      jogconCaptureInput(captured);
#endif
      init_jogcon();
      return false;
    }
    if (!enableMouseMove) {
      if(psx[0]->buttonPressed(PSB_L2) && psx[0]->buttonPressed(PSB_R2))
        mode = 0;
      else if(psx[0]->buttonPressed(PSB_L1) && psx[0]->buttonPressed(PSB_R1))
        mode = 3;
      else if(psx[0]->buttonPressed(PSB_L2))
        mode = 1;
      else if(psx[0]->buttonPressed(PSB_R2))
        mode = 2;
      menu_jogcon_mode = mode;

      if(psx[0]->buttonPressed(PSB_TRIANGLE))
        force = 1;
      else if(psx[0]->buttonPressed(PSB_CIRCLE))
        force = 3;
      else if(psx[0]->buttonPressed(PSB_CROSS))
        force = 7;
      else if(psx[0]->buttonPressed(PSB_SQUARE))
        force = 15;
      menu_jogcon_force = force;

      if(psx[0]->buttonPressed(PSB_L1) ^ psx[0]->buttonPressed(PSB_R1)) {
        sp_div = psx[0]->buttonPressed(PSB_L1) ? 2 : 1;
        sp_max = SP_MAX/sp_div;
        sp_half = sp_max/2;
      }
    } else {
      mode = 0;

      if(psx[0]->buttonPressed(PSB_CIRCLE))
        mouse_axis = 0;
      else if(psx[0]->buttonPressed(PSB_TRIANGLE))
        mouse_axis = 1;
      else if(psx[0]->buttonPressed(PSB_SQUARE))
        mouse_axis = 2;
      else if(psx[0]->buttonPressed(PSB_CROSS))
        mouse_axis = 3;
    }

    if(psx[0]->buttonPressed(PSB_PAD_UP))
      spinner_speed = 4;
    else if(psx[0]->buttonPressed(PSB_PAD_RIGHT))
      spinner_speed = 3;
    else if(psx[0]->buttonPressed(PSB_PAD_DOWN))
      spinner_speed = 2;
    else if(psx[0]->buttonPressed(PSB_PAD_LEFT))
      spinner_speed = 1;

    if (rotaryProfileConfigured(jogcon_rotary_profile) &&
        (psx[0]->buttonPressed(PSB_PAD_UP) || psx[0]->buttonPressed(PSB_PAD_RIGHT) ||
         psx[0]->buttonPressed(PSB_PAD_DOWN) || psx[0]->buttonPressed(PSB_PAD_LEFT))) {
      jogcon_rotary_profile = encodeRotaryProfile(rotaryGainFromSpinnerSpeed(spinner_speed),
          rotaryProfileReverse(jogcon_rotary_profile));
    }

    // Capture this physical MODE transition immediately, even during the
    // retry cooldown, so releasing the chord cannot lose its assignment.
    // A stream of digital reports is latched and uses the bounded retry path.
    init_jogcon(true);

    gotJogconData = psx[0]->getJogconData(jogPosition, jogRevolutions, jogDirection, cmdResult);

    jogcon_prevcnt = 0;
    jogcon_cleancnt = 0;
    jogcon_counter = (jogRevolutions << 8) | jogPosition;
    jogcon_pdlpos = sp_half;
    jogcon_spinnerAnalog = 0;
    jogcon_wheelCenterSet = false;
    jogcon_oldbtn = 0xFFFF;
    jogcon_oldpaddle = 0xFF;
    jogcon_oldspinner = INT8_MIN;
    update_jogcon_display_name();
  } else {
    sp_step = configuredJogconSpinnerStep();
    const RotaryTuning tuning = currentRotaryTuning(RotaryInputKind::JogCon, mode);
    if (jogconOutputReverse != tuning.reverse || jogconOutputGain != tuning.gain) {
      jogconOutputDivider.reset();
      resetRotaryMotion(0);
      jogcon_cleancnt = jogcon_prevcnt = 0;
      jogconOutputReverse = tuning.reverse;
      jogconOutputGain = tuning.gain;
    }
    if(jogDirection != JOGCON_DIR_NONE) {
      const int16_t delta = (int16_t)(jogcon_newcnt - jogcon_counter);
      const int32_t directed = tuning.reverse ? -(int32_t)delta : delta;
      const int32_t motion = jogconOutputDivider.apply(
          directed * rotaryGainNumerator(tuning.gain),
          4 * rotaryGainDenominator(tuning.gain));
      if (rotaryOutputQueuesMotion(RotaryInputKind::JogCon, get_effective_output_mode())) {
        if (motion != 0) {
          addRotaryMotion(0, motion);
          stateChanged = true;
        }
      }
      jogcon_cleancnt += motion;
      if(mode == 0) {
        const int32_t analogMin = -128L * sp_step;
        const int32_t analogMax = 127L * sp_step;
        jogcon_spinnerAnalog += motion;
        if(jogcon_spinnerAnalog < analogMin) jogcon_spinnerAnalog = analogMin;
        if(jogcon_spinnerAnalog > analogMax) jogcon_spinnerAnalog = analogMax;
      }
    }

    const int16_t signedPosition = (int16_t)jogcon_newcnt;
    JogconDirection motorDirection = JOGCON_DIR_NONE;
    if(mode == 1 || mode == 2) {
      if(signedPosition < -sp_half) {
        jogcon_pdlpos = 0;
      }
      else if(signedPosition > sp_half) {
        jogcon_pdlpos = sp_max;
      }
      else {
        jogcon_pdlpos = (uint16_t)(signedPosition + sp_half);
      }
    }

    if(mode == 1) {
      // Match the proven MiSTer JogCon paddle behavior: START acts as a
      // motor brake beyond either endpoint, rather than driving the wheel.
      if(signedPosition < -sp_half || signedPosition > sp_half)
        motorDirection = JOGCON_DIR_START;
    } else if(mode == 2) {
      // Adapt V1: the JogCon owns the centering servo. CW/CCW are sustained
      // torque commands, not target positions, and can run away after rocking.
      motorDirection = JOGCON_DIR_START;
    }

    if (!jogconMotorReady) motorDirection = JOGCON_DIR_NONE;
    ff = motorDirection != JOGCON_DIR_NONE;
    psx[0]->setJogconMotorMode(
      motorDirection, nextCmd, ff ? force : 0);

    int16_t val = ((int16_t)(jogcon_cleancnt - jogcon_prevcnt))/sp_step;
    if(val>127) val = 127; else if(val<-127) val = -127;
    jogcon_prevcnt += val*sp_step;
    const int8_t spinner = val;

    const int16_t spinnerAnalogAxis =
      clampJogconAxis((int16_t)(jogcon_spinnerAnalog / sp_step));
    const uint8_t paddle = mode == 0
                             ? (uint8_t)(spinnerAnalogAxis + 0x80)
                             : (uint8_t)((jogcon_pdlpos * 255) / sp_max);

    if (mode == 3) {
      if(spinner < 0)
        jogcon_btntimeout = -64;
      else if(spinner > 0)
        jogcon_btntimeout = 63;
      bitWrite(jogcon_newbtn, 0, jogcon_btntimeout < 0);
      bitWrite(jogcon_newbtn, 1, jogcon_btntimeout > 0);
    }

    if(jogcon_oldbtn != jogcon_newbtn || jogcon_oldpaddle != paddle || jogcon_oldspinner != spinner) {
      stateChanged = true;
      jogcon_oldbtn = jogcon_newbtn;
      jogcon_oldpaddle = paddle;
      jogcon_oldspinner = spinner;

      handleDpad();
      const uint8_t i = 0;
      controller_state_t& frame = inputFrame(i);
      frame.sticks_precision_bits = ANALOG_STICK_PRECISION_8;
      frame.LX = 0;
      frame.LY = 0;
      frame.RX = 0;
      frame.RY = 0;
      frame.paddle = 0x80;
      frame.spinner = 0;
      frame.HAS_ANALOG_STICK_MAIN = (!enableMouseMove && mode != 3);
      frame.HAS_ANALOG_STICK_AUX = 0;
      frame.PAD_U = psx[0]->buttonPressed(PSB_PAD_UP);
      frame.PAD_D = psx[0]->buttonPressed(PSB_PAD_DOWN);
      frame.PAD_L = psx[0]->buttonPressed(PSB_PAD_LEFT);
      frame.PAD_R = psx[0]->buttonPressed(PSB_PAD_RIGHT);
      frame.A  = psx[0]->buttonPressed(PSB_CROSS);
      frame.B  = psx[0]->buttonPressed(PSB_CIRCLE);
      frame.X  = psx[0]->buttonPressed(PSB_SQUARE);
      frame.Y  = psx[0]->buttonPressed(PSB_TRIANGLE);
      frame.L1 = psx[0]->buttonPressed(PSB_L1);
      frame.R1 = psx[0]->buttonPressed(PSB_R1);
      frame.L2 = psx[0]->buttonPressed(PSB_L2);
      frame.R2 = psx[0]->buttonPressed(PSB_R2);
      frame.HAS_ANALOG_TRIGGERS = 0;
      frame.ANALOG_L2 = 0;
      frame.ANALOG_R2 = 0;
      frame.L3 = psx[0]->buttonPressed(PSB_L3);
      frame.R3 = psx[0]->buttonPressed(PSB_R3);
      frame.SELECT = psx[0]->buttonPressed(PSB_SELECT);
      frame.START = psx[0]->buttonPressed(PSB_START);

      if (mode == 3) {
        bool spinCCW = jogcon_btntimeout < 0;
        bool spinCW = jogcon_btntimeout > 0;
        switch (menu_jogcon_digital_map) {
          case 0: frame.PAD_L |= spinCCW; frame.PAD_R |= spinCW; break;
          case 1: frame.L3    |= spinCCW; frame.R3    |= spinCW; break;
          case 2: frame.L1    |= spinCCW; frame.R1    |= spinCW; break;
          case 3: frame.PAD_U |= spinCCW; frame.PAD_D |= spinCW; break;
        }
      }

      if(enableMouseMove) {
        frame.mouse_x       = mouse_axis == 0 ? spinner : 0;
        frame.mouse_y       = mouse_axis == 1 ? spinner : 0;
        frame.mouse_wheel_y = mouse_axis == 2 ? spinner : 0;
        frame.mouse_wheel_x = mouse_axis == 3 ? spinner : 0;
      } else if (mode != 3) {
        const int16_t axis = mode == 0
                               ? spinnerAnalogAxis
                               : centeredJogconPaddleAxis(paddle);
        if (menu_jogcon_wheel_axis == 0)
          frame.LX = axis;
        else
          frame.LY = axis;
        if (mode == 0)
          frame.spinner = spinner;
        else
          frame.paddle = paddle;
      }
    }
  }
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
  captured.counter = jogcon_newcnt;
  captured.delta = int16_t(jogcon_newcnt - captured.previousCounter);
  captured.cleanCounter = jogcon_cleancnt; captured.spinnerAnalog = jogcon_spinnerAnalog;
  captured.lastDirection = uint8_t(jogcon_lastDirection); captured.rawButtons = jogcon_newbtn;
  captured.mode = mode; captured.force = force; captured.ready = jogconMotorReady;
  captured.primed = jogconRuntimePrimed; captured.axis = menu_jogcon_wheel_axis;
  const RotaryTuning captureTuning = currentRotaryTuning(RotaryInputKind::JogCon, mode);
  captured.gain = captureTuning.gain; captured.reverse = captureTuning.reverse;
  captured.lx = inputFrame(0).LX; captured.ly = inputFrame(0).LY;
  captured.paddle = inputFrame(0).paddle; captured.spinner = inputFrame(0).spinner;
  captured.buttons = inputFrame(0).digital_buttons;
  jogconCaptureInput(captured);
#endif
  jogcon_counter = jogcon_newcnt;
  // Keep publishing input while retrying a failed motor handshake at most
  // once per second. Re-prime counters on the following fresh report.
  if (!jogconMotorReady) init_jogcon();

  if (mode == 3) {
    if(jogcon_btntimeout < 0)
      jogcon_btntimeout ++;
    else if(jogcon_btntimeout > 0)
      jogcon_btntimeout --;
  }
  return stateChanged;
}

void RZInputPSX::jogconSetup() {
  jogconOutputDivider.reset();
  resetRotaryMotion(0);
  // JogCon state is shared by the protocol helpers rather than owned by the
  // input-module instance. Reset every session value when AUTO detection
  // replaces the module so counters, cached reports, and motor state cannot
  // leak into the next setup.
  ff = 0;
  jogconMotorReady = false;
  jogconInitAttempted = false;
  jogconDigitalModeLatched = false;
  jogconLastInitMs = 0;
  force = configuredJogconForce();
  mouse_axis = 0;
  sp_step = configuredJogconSpinnerStep();
  sp_div = 1;
  sp_max = SP_MAX/sp_div;
  sp_half = sp_max/2;
  jogcon_counter = 0;
  jogcon_newcnt = 0;
  jogcon_cleancnt = 0;
  jogcon_newbtn = 0;
  jogcon_oldbtn = 0xFFFF;
  jogcon_oldspinner = INT8_MIN;
  jogcon_oldpaddle = 0xFF;
  jogcon_pdlpos = sp_half;
  jogcon_spinnerAnalog = 0;
  jogcon_wheelCenter = 0;
  jogcon_wheelCenterSet = false;
  jogcon_prevcnt = 0;
  jogcon_btntimeout = 0;
  jogcon_lastDirection = JOGCON_DIR_NONE;
  jogconRuntimePrimed = false;

  setInputPortCount(1);
  enableMouseMove = haveController[0] && psx[0]->buttonPressed(PSB_SELECT);
  mode = configuredJogconMode(enableMouseMove);
  update_jogcon_display_name();
}

void RZInputPSX::jogconSetup2() {
  output_try_enable_psx_jogcon_mode();

  if (enableMouseMove)
    delay(200);
}
