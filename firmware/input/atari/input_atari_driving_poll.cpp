#include "Input_AtariDriving.h"
#include "../../core/controller_settings_state.h"
#include "../../core/rotary_runtime_policy.h"
#include "../../output/output_runtime_state.h"

bool RZInputDriving::poll() {
  beginPollCycle();
  const outputMode_t output = get_effective_output_mode();
  const bool paddleOutput = output == OUTPUT_MISTER_PADDLE;
  const RotaryTuning tuning = currentRotaryTuning(RotaryInputKind::Driving);
  const bool canonicalProfile = rotaryProfileConfigured(rotary_profile);
  if (paddleOutput != paddleOutputActive) {
    for (auto& divider : paddleDivider) divider.reset();
    paddleOutputActive = paddleOutput;
  }

  for (uint8_t port = 0; port < input_ports && port < MAX_USB_OUT; ++port) {
    const input_driving_config_t& pins = input_driving_config[port];
    uint8_t currState = drivingReadEncoderState(pins.enc_a, pins.enc_b);
    uint8_t tableIndex = (lastState[port] << 2) | currState;
    int8_t direction = driving_quad_table[tableIndex];
    motion[port].sample(port, direction, true, output, tuning.gain, tuning.reverse);

    if (direction != 0) {
      const int32_t scaled = static_cast<int32_t>(direction) * 8 *
                             rotaryGainNumerator(tuning.gain);
      const bool wrap = canonicalProfile ? driving_wrap :
                        (paddleOutput ? driving_wrap : true);
      position[port] = advanceDrivingPaddle(position[port], static_cast<int16_t>(scaled),
          paddleDivider[port], rotaryGainDenominator(tuning.gain), wrap);
    }

    uint8_t btn = drivingReadPin(pins.button);
    bool changed = direction != 0 || btn != lastButton[port];
    lastState[port] = currState;
    resetState(port);
    controller_state_t& frame = inputFrame(port);
    int16_t axisValue = ((int32_t)(position[port] & 0xFF) * 256) - 32768;
    frame.LX = axisValue;
    frame.paddle = position[port] & 0xFF;
    frame.LY = 0;
    frame.RX = 0;
    frame.RY = 0;
    frame.A = (btn == LOW) ? 1 : 0;
    if (changed) {
      setInputFrameConnected(frame, true);
      setUpdated(port);
    }
    lastButton[port] = btn;

    if (port == 0) {
      uint8_t raw[16] = {0};
      raw[0] = 1;
      raw[1] = currState;
      raw[2] = lastState[port];
      raw[3] = position[port] & 0xFF;
      raw[4] = (btn == LOW) ? 1 : 0;
      raw[5] = direction;
      raw[6] = (axisValue >> 8) & 0xFF;
      raw[7] = axisValue & 0xFF;
      raw[8] = port;
      raw[9] = spinner_speed;
      webhid_store_raw_data(raw, 16);
    }
  }

  return endPollCycle();
}
