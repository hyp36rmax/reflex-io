#include "Input_Sms.h"

#include <cstring>

#include "../shared/input_button_bits.h"
#include "../../core/classic_mouse_motion.h"
#include "../../core/classic_mouse_source.h"
#include "../../core/controller_settings_state.h"
#include "../../core/rotary_runtime_policy.h"
#include "../../output/output_runtime_state.h"

#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
#include "hardware/gpio.h"
#endif

namespace {

__force_inline uint8_t __not_in_flash_func(readSmsPin)(uint8_t pin) {
#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
  return gpio_get(pin) ? HIGH : LOW;
#else
  return digitalRead(pin);
#endif
}

// Up..Right as a Sports Pad nibble, D0..D3, line high = 1.
inline uint8_t readSmsLines(const input_sms_config_t& pins) {
  return static_cast<uint8_t>(readSmsPin(pins.up) | (readSmsPin(pins.down) << 1) |
                              (readSmsPin(pins.left) << 2) | (readSmsPin(pins.right) << 3));
}

// TH idles as an input with its pull-up. Set the latch before enabling the
// driver: pinMode(OUTPUT) clears it, which would pulse TH low.
void driveSmsTh(uint8_t pin, bool high) {
#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
  gpio_put(pin, high);
  gpio_set_dir(pin, GPIO_OUT);
#else
  pinMode(pin, OUTPUT);
  digitalWrite(pin, high ? HIGH : LOW);
#endif
}

void releaseSmsTh(uint8_t pin) {
#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
  gpio_set_dir(pin, GPIO_IN);
#else
  pinMode(pin, INPUT_PULLUP);
#endif
}

// One packet: TH low, X high; TH high, X low; TH low, Y high; TH high, Y low.
// TH is left driven high.
uint16_t readSmsSportsPadPacket(const input_sms_config_t& pins) {
  uint16_t packet = 0;
  for (uint8_t phase = 0; phase < 4; ++phase) {
    driveSmsTh(pins.th, (phase & 1) != 0);
    delayMicroseconds(phase == 0 ? sms_sports_pad::kFirstNibbleWaitUs
                                 : sms_sports_pad::kNibbleWaitUs);
    packet = static_cast<uint16_t>((packet << 4) | readSmsLines(pins));
  }
  return packet;
}

}  // namespace

void __not_in_flash_func(RZInputSms::stageWebhidRawData)(uint8_t state, uint8_t select, uint8_t runCombo,
                                                         uint8_t port, uint8_t currentDrivingState,
                                                         int8_t drivingDelta) {
  pendingWebhidRaw[0] = isJPC ? 2 : (drivingActive[port] ? 3 : 1);
  pendingWebhidRaw[1] = state;
  pendingWebhidRaw[2] = select;
  pendingWebhidRaw[3] = runCombo;
  pendingWebhidRaw[4] = port;
  pendingWebhidRaw[5] = currentDrivingState;
  pendingWebhidRaw[6] = (uint8_t)drivingDelta;
  pendingWebhidRaw[7] = (uint8_t)drivingPosition[port];
  pendingWebhidRawDirty = true;
}

void RZInputSms::flushPendingWebhidRawData() {
  if (!pendingWebhidRawDirty) {
    return;
  }
  uint8_t raw[16] = {0};
  memcpy(raw, pendingWebhidRaw, sizeof(pendingWebhidRaw));
  webhid_store_raw_data(raw, 16);
  pendingWebhidRawDirty = false;
}

bool __not_in_flash_func(RZInputSms::poll)() {
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
    const input_sms_config_t& pins = input_sms_config[port];

    if (!isJPC) {
      const uint32_t nowUs = micros();
      if (static_cast<int32_t>(nowUs - sportsPad[port].dueUs) >= 0) {
        serviceSportsPad(port, nowUs);
      }
      if (sportsPad[port].active) {
        drivingStep[port] = 0;
        continue;
      }
    }

    uint8_t up    = readSmsPin(pins.up);
    uint8_t down  = readSmsPin(pins.down);
    uint8_t left  = readSmsPin(pins.left);
    uint8_t right = readSmsPin(pins.right);
    uint8_t btn1  = readSmsPin(pins.tl);
    uint8_t btn2  = HIGH;
    if (isJPC) {
      btn2 = readSmsPin(pins.th);
    } else {
      btn2 = readSmsPin(pins.tr);
    }

    // A Sports Pad holds a packet's last nibble until the next TH edge: all
    // four lines low at rest, which no pad or Driving controller can show, or
    // whatever the last probe left. Neither is a press or a Driving step.
    bool sportsPadResidue = false;
    if (!isJPC) {
      const uint8_t lines = static_cast<uint8_t>(up | (down << 1) | (left << 2) | (right << 3));
      if (lines != sportsPad[port].tail) sportsPad[port].tail = sms_sports_pad::kNoTail;
      sportsPadResidue = sportsPad[port].tail != sms_sports_pad::kNoTail;
      if (lines == 0) up = down = left = right = HIGH;
    }

    uint8_t currentDrivingState = ((up == HIGH) ? 1 : 0) << 1 | ((down == HIGH) ? 1 : 0);
    int8_t drivingDelta = 0;
    bool drivingActivatedThisPoll = false;
    controller_state_t& frame = inputFrame(port);
    if (!isJPC && !drivingActive[port] && !sportsPadResidue && up == LOW && down == LOW) {
      drivingActive[port] = true;
      drivingActivatedThisPoll = true;
      frame.HAS_ANALOG_STICK_MAIN = 1;
      frame.sticks_precision_bits = ANALOG_STICK_PRECISION_16;
      setInputFrameTypeName(frame, "Driving");
    }
    if (!isJPC && currentDrivingState != lastDrivingState[port]) {
      uint8_t drivingTableIndex = (lastDrivingState[port] << 2) | currentDrivingState;
      drivingDelta = drivingQuadTable[drivingTableIndex];

      if (!drivingActive[port] && !sportsPadResidue &&
          (currentDrivingState == 0 || lastDrivingState[port] == 0) &&
          drivingDelta != 0) {
        drivingActive[port] = true;
        drivingActivatedThisPoll = true;
        frame.HAS_ANALOG_STICK_MAIN = 1;
        frame.sticks_precision_bits = ANALOG_STICK_PRECISION_16;
        setInputFrameTypeName(frame, "Driving");
      }

      if (drivingActive[port] && drivingDelta != 0) {
        const int32_t scaled = static_cast<int32_t>(drivingDelta) * 8 *
                               rotaryGainNumerator(tuning.gain);
        const bool wrap = canonicalProfile ? driving_wrap :
                          (paddleOutput ? driving_wrap : false);
        drivingPosition[port] = advanceDrivingPaddle(drivingPosition[port],
            static_cast<int16_t>(scaled), paddleDivider[port],
            rotaryGainDenominator(tuning.gain), wrap);
      }
    }
    lastDrivingState[port] = currentDrivingState;
    drivingStep[port] = drivingDelta;

    uint8_t state = (HIGH << 6) | (btn2 << 5) | (btn1 << 4) | (right << 3) | (left << 2) | (down << 1) | up;

    uint8_t select = 0, runCombo = 0;
    if (isJPC) {
      if ((state & 0x03) == 0x00) select = 1;
      if ((state & 0x0C) == 0x00) runCombo = 1;
    }

    if (state != lastState[port] || drivingActivatedThisPoll) {
      lastState[port] = state;

      if (!drivingActive[port]) {
        uint32_t buttons = 0;
        buttons |= (up == LOW) ? INPUT_PAD_U : 0;
        buttons |= (down == LOW) ? INPUT_PAD_D : 0;
        buttons |= (left == LOW) ? INPUT_PAD_L : 0;
        buttons |= (right == LOW) ? INPUT_PAD_R : 0;

        if (isJPC) {
          buttons |= (btn2 == LOW) ? INPUT_A : 0;
          buttons |= (btn1 == LOW) ? INPUT_B : 0;
          if (select) {
            buttons = (buttons & ~(INPUT_PAD_U | INPUT_PAD_D)) | INPUT_SELECT;
          }
          if (runCombo) {
            buttons = (buttons & ~(INPUT_PAD_L | INPUT_PAD_R)) | INPUT_START;
          }
        } else {
          buttons |= (btn1 == LOW) ? INPUT_A : 0;
          buttons |= (btn2 == LOW) ? INPUT_B : 0;
        }

        frame.digital_buttons = buttons;
      } else {
        resetState(port);

        frame.PAD_U = (up == LOW) ? 1 : 0;
        frame.PAD_D = (down == LOW) ? 1 : 0;
        frame.PAD_L = (left == LOW) ? 1 : 0;
        frame.PAD_R = (right == LOW) ? 1 : 0;
      }

      if (drivingActive[port]) {
        frame.A = (btn1 == LOW) ? 1 : 0;
        frame.B = (btn2 == LOW) ? 1 : 0;

        int16_t axisValue = ((int32_t)drivingPosition[port] * 256) - 32768;
        frame.HAS_ANALOG_STICK_MAIN = 1;
        frame.sticks_precision_bits = ANALOG_STICK_PRECISION_16;
        frame.LX = axisValue;
        frame.LY = 0;
        frame.RX = 0;
        frame.RY = 0;
        frame.paddle = (uint8_t)drivingPosition[port];
        frame.PAD_U = 0;
        frame.PAD_D = 0;
        setInputFrameTypeName(frame, "Driving");
      }

      latchPassiveInputFrameConnection(frame);
      // Turning sets no button bits, and only a real encoder step activates this.
      if (drivingActive[port]) setInputFrameConnected(frame, true);
      setUpdated(port);

      if (port == 0) {
        stageWebhidRawData(state, select, runCombo, port, currentDrivingState, drivingDelta);
      }
    }
  }

  queueDrivingMotion(output, tuning.gain, tuning.reverse);
  return endPollCycle();
}

// Runs from flash: the RAM-resident poll above only records each port's step.
void __attribute__((noinline)) RZInputSms::queueDrivingMotion(outputMode_t output,
                                                              uint8_t gain, bool reverse) {
  for (uint8_t port = 0; port < input_ports && port < MAX_USB_OUT; ++port) {
    drivingMotion[port].sample(port, drivingStep[port], drivingActive[port], output,
                               gain, reverse);
  }
}

// Runs from flash when a port's next Sports Pad packet is due. A port that
// reads idle, all four lines low or what the last probe left is probed at
// most every kProbeIntervalUs; consecutive all-zero packets identify a pad at
// rest. An identified port keeps TH high and is read every kPacketIntervalUs
// as a native mouse until a run of pad-like packets returns it to pad reading.
void __attribute__((noinline)) RZInputSms::serviceSportsPad(uint8_t port, uint32_t nowUs) {
  using namespace sms_sports_pad;
  if (port >= input_ports || port >= MAX_USB_OUT) return;
  SmsSportsPadPort& sports = sportsPad[port];
  const input_sms_config_t& pins = input_sms_config[port];
  controller_state_t& frame = inputFrame(port);
  if (!sports.active) {
    const uint8_t lines = readSmsLines(pins);
    if (drivingActive[port] || !physical_port_enabled(port) ||
        (lines != 0x0F && lines != 0 && lines != sports.tail)) {
      sports.confirm = 0;
      sports.dueUs = nowUs + kProbeIntervalUs;
      return;
    }
    const uint16_t packet = readSmsSportsPadPacket(pins);
    releaseSmsTh(pins.th);
    sports.tail = padLikePacket(packet) ? kNoTail : static_cast<uint8_t>(packet & 0x0F);
    if (packet != 0) {
      sports.confirm = 0;
      sports.dueUs = nowUs + kProbeIntervalUs;
      return;
    }
    sports.dueUs = nowUs + kPacketIntervalUs;
    if (++sports.confirm < kConfirmPackets) return;
    sports = {};
    sports.active = true;
    sports.dueUs = nowUs + kPacketIntervalUs;
    driveSmsTh(pins.th, true);
    resetClassicMouseMotion(port);
    resetState(port);
    frame.mouse_x = 0;
    frame.mouse_y = 0;
    setInputFrameTypeName(frame, "Sports Pad");
    setInputFrameConnected(frame, true);
    setClassicMouseInput(port, true);
    setUpdated(port);
    return;
  }

  const uint16_t packet = readSmsSportsPadPacket(pins);
  sports.dueUs = nowUs + kPacketIntervalUs;
  int16_t dx = 0;
  int16_t dy = 0;
  if (padLikePacket(packet)) {
    if (++sports.leave >= kLeavePackets) {
      // A pad, a Driving controller, a Genesis pad or an empty port.
      sports = {};
      sports.dueUs = nowUs + kProbeIntervalUs;
      releaseSmsTh(pins.th);
      resetClassicMouseMotion(port);
      setClassicMouseInput(port, false);
      setInputFrameConnected(frame, false);
      setInputFrameTypeName(frame, "Atari/C64/SMS");
      lastState[port] = 0xFF;
      lastDrivingState[port] = static_cast<uint8_t>((readSmsPin(pins.up) << 1) | readSmsPin(pins.down));
      setUpdated(port);
      return;
    }
    // Held until the run ends, so a swapped or unplugged port never moves
    // the pointer; a moving pad's rare equal nibbles still arrive.
    sports.deferX = classicMouseBoundedAdd(sports.deferX, usbDx(packet));
    sports.deferY = classicMouseBoundedAdd(sports.deferY, usbDy(packet));
  } else {
    dx = classicMouseBoundedAdd(sports.deferX, usbDx(packet));
    dy = classicMouseBoundedAdd(sports.deferY, usbDy(packet));
    sports.deferX = 0;
    sports.deferY = 0;
    sports.leave = 0;
  }
  if (dx != 0 || dy != 0) queueClassicMouseMotion(port, dx, dy);

  // Auto's frame clearing drops the identity; the pad is still here.
  if (!frame.connected) {
    setInputFrameTypeName(frame, "Sports Pad");
    setInputFrameConnected(frame, true);
    setUpdated(port);
  }
  setClassicMouseInput(port, true);
  const uint32_t buttons = (readSmsPin(pins.tl) == LOW ? INPUT_A : 0) |
                           (readSmsPin(pins.tr) == LOW ? INPUT_B : 0);
  const int8_t mouseX = classicMouseChunk(dx);
  const int8_t mouseY = classicMouseChunk(dy);
  if (dx != 0 || dy != 0 || frame.digital_buttons != buttons ||
      frame.mouse_x != mouseX || frame.mouse_y != mouseY) {
    frame.digital_buttons = buttons;
    frame.mouse_x = mouseX;
    frame.mouse_y = mouseY;
    setUpdated(port);
  }
}

void RZInputSms::afterOutputFrameSent(bool polled, bool updated) {
  (void)polled;
  (void)updated;
  flushPendingWebhidRawData();
}
