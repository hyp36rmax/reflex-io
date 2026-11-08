#include "Input_GC64.h"

#include "../../core/classic_analog_range.h"
#include "../../core/classic_mouse_motion.h"
#include "../../core/classic_mouse_source.h"
#include "../../core/controller_settings_state.h"
#include "../../core/rumble_test_runtime.h"
#include "../shared/input_button_bits.h"
#include "../../platform/latency_trace_gpio.h"
#include "../../platform/latency_test.h"
#include "../../features/n64_capture/n64_capture.h"

namespace {

DeviceEnum joybusAnalogModeForType(JoybusDeviceType_Enum type) {
  switch (type) {
    #if defined(ENABLE_INPUT_N64)
    case JOYBUS_DEVICE_N64PAD:
    case JOYBUS_DEVICE_N64MOUSE:
      return RZORD_N64;
    #endif
    #if defined(ENABLE_INPUT_GAMECUBE)
    case JOYBUS_DEVICE_GCPAD:
    case JOYBUS_DEVICE_GCWIRELESS:
    case JOYBUS_DEVICE_GCWHEEL:
      return RZORD_GAMECUBE;
    #endif
    #if defined(ENABLE_INPUT_GBA)
    case JOYBUS_DEVICE_GBA:
      return RZORD_GBA;
    #endif
    default:
      return deviceMode;
  }
}

}  // namespace

void __not_in_flash_func(RZInputGC64::stageWebhidRawData)(uint8_t port, JoybusDeviceType_Enum type) {
  if (port != 0 || joybus[port] == nullptr) {
    return;
  }

  pendingWebhidRawPort = port;
  pendingWebhidRawType = type;
  pendingWebhidRawDirty = true;
}

void RZInputGC64::flushPendingWebhidRawData() {
  if (!pendingWebhidRawDirty) {
    return;
  }

  const uint8_t port = pendingWebhidRawPort;
  const JoybusDeviceType_Enum type = pendingWebhidRawType;
  pendingWebhidRawDirty = false;

  if (port != 0 || port >= input_ports || joybus[port] == nullptr) {
    return;
  }

  uint8_t raw[16] = {0};
  raw[0] = type;
  if (type == JOYBUS_DEVICE_N64PAD || type == JOYBUS_DEVICE_N64MOUSE) {
    const uint32_t payload = joybus[port]->getN64PadPayload();
    raw[1] = static_cast<uint8_t>(payload >> 16);
    raw[2] = static_cast<uint8_t>(payload >> 24);
    raw[3] = static_cast<uint8_t>(payload >> 8);
    raw[4] = static_cast<uint8_t>(payload);
  } else if (type == JOYBUS_DEVICE_GCPAD || type == JOYBUS_DEVICE_GCWIRELESS) {
    const NormalizedGCControllerState state = joybus[port]->getGCPadState();
    raw[1] = state.joystick_x;
    raw[2] = state.joystick_y;
    raw[3] = state.cstick_x;
    raw[4] = state.cstick_y;
    raw[5] = state.analog_l;
    raw[6] = state.analog_r;
    raw[7] = (state.buttons >> 8) & 0xFF;
    raw[8] = state.buttons & 0xFF;
  } else if (type == JOYBUS_DEVICE_GBA) {
    const GBAState state = joybus[port]->getGBAState();
    raw[3] = (state >> 8) & 0xFF;
    raw[4] = state & 0xFF;
  }
  raw[9] = port;
  raw[10] = getInternalMode();

  webhid_store_raw_data(raw, 16);
}

bool __not_in_flash_func(RZInputGC64::poll)() {
  beginPollCycle();
  bool pollIntervalNeedsUpdate = false;

  for (uint8_t port = 0; port < input_ports; ++port) {
    uint8_t i = port;
    controller_state_t& frame = inputFrame(i);

    if (!should_poll_port(input_ports, port) || joybus[port] == nullptr) {
      continue;
    }

    // N64 accessory writes are serviced after the USB report, so its rumble
    // state can be sampled there without delaying the controller read. Keep
    // GameCube feedback here because it is part of the GameCube poll command.
    if (dtype[port] != JOYBUS_DEVICE_N64PAD) {
      uint8_t effective_left = 0;
      uint8_t effective_right = 0;
      rumbleRuntimeGetEffectiveFeedback(port, &effective_left, &effective_right);
      joybus[port]->setRumble((effective_left | effective_right) != 0);
    }
    {
      LatencyPhaseTraceScope gc64UpdateTrace(LATENCY_TRACE_PHASE_GC64_UPDATE);
      joybus[port]->update();
    }
    const JoybusDeviceType_Enum observed_type = joybus[port]->deviceType();
    setClassicMouseInput(i, observed_type == JOYBUS_DEVICE_N64MOUSE);
    if (joybus[port]->deviceJustChanged()) resetClassicMouseMotion(i);
#if defined(ENABLE_EXPERIMENTAL_CONSOLE_MOUSE)
    // Relative reports are events: equal successive deltas still both count.
    // Joybus retains its last valid state on a read failure, so only queue a
    // report that the current update actually received.
    if (observed_type == JOYBUS_DEVICE_N64MOUSE && joybus[port]->hasFreshN64MouseReport()) {
      const N64MouseState mouse = joybus[port]->getN64MouseState();
      queueClassicMouseMotion(i, mouse.movement_x, -static_cast<int16_t>(mouse.movement_y));
      if (mouse.movement_x || mouse.movement_y) setUpdated(i);
    }
#endif
    if (i == 0 && latencyTest.isEnabled() &&
        joybus[port]->deviceType() == JOYBUS_DEVICE_N64PAD) {
      latencyTest.observeRawButtons(
          0, static_cast<uint16_t>(joybus[port]->getN64PadPayload()), true);
    }
    if (joybus[port]->stateChanged()) {
      resetState(i);
      if (joybus[port]->deviceJustChanged()) {
        pollIntervalNeedsUpdate = true;
        JoybusDeviceType_Enum observedType = observed_type;
        if (observedType == JOYBUS_DEVICE_GCWIRELESS) {
          wavebirdReceiverSeen[port] = true;
        } else if (observedType != JOYBUS_DEVICE_GCPAD) {
          wavebirdReceiverSeen[port] = false;
        }
        dtype[port] = (wavebirdReceiverSeen[port] && observedType == JOYBUS_DEVICE_GCPAD)
                        ? JOYBUS_DEVICE_GCWIRELESS
                        : observedType;
        #if !defined(ENABLE_EXPERIMENTAL_CONSOLE_MOUSE)
        if (dtype[port] == JOYBUS_DEVICE_N64MOUSE) {
          dtype[port] = JOYBUS_DEVICE_NOTSUPPORTED;
        }
        #endif
        if (dtype[port] != JOYBUS_DEVICE_N64MOUSE) frame.HAS_BTN_START = true;
        resetClassicAnalogLearnState(joybusAnalogModeForType(dtype[port]), i);
        setInputFrameConnected(i, dtype[port] != JOYBUS_DEVICE_NONE);
        n64RumbleLabelShown[port] = false;

        switch (dtype[port]) {
          case JOYBUS_DEVICE_N64PAD:
            setInputFrameTypeName(i, "N64 Pad");
            break;
          case JOYBUS_DEVICE_N64MOUSE:
            setInputFrameTypeName(i, "N64 Mouse");
            break;
          case JOYBUS_DEVICE_GCPAD:
            setInputFrameTypeName(i, "GC Pad");
            break;
          case JOYBUS_DEVICE_GCWIRELESS:
            setInputFrameTypeName(i, "WaveBird");
            break;
          case JOYBUS_DEVICE_GBA:
            setInputFrameTypeName(i, "GBA");
            break;
          case JOYBUS_DEVICE_NONE:
          default:
            clearInputFrameTypeName(i);
            break;
        }
      }

      frame.HAS_ANALOG_TRIGGERS = false;
      frame.HAS_ANALOG_STICK_MAIN = false;
      frame.HAS_ANALOG_STICK_AUX = false;
      switch (dtype[port]) {
        case JOYBUS_DEVICE_NONE:
        case JOYBUS_DEVICE_NOTSUPPORTED:
          break;
        case JOYBUS_DEVICE_GBA:
        {
          const GBAState buttons = joybus[port]->getGBAState();
          frame.PAD_U = (buttons & GBAB_DPAD_UP) != 0;
          frame.PAD_D = (buttons & GBAB_DPAD_DOWN) != 0;
          frame.PAD_L = (buttons & GBAB_DPAD_LEFT) != 0;
          frame.PAD_R = (buttons & GBAB_DPAD_RIGHT) != 0;
          frame.A = (buttons & GBAB_B_BUTTON) != 0;
          frame.B = (buttons & GBAB_A_BUTTON) != 0;
          frame.L1 = (buttons & GBAB_L_TRIGGER) != 0;
          frame.R1 = (buttons & GBAB_R_TRIGGER) != 0;
          frame.SELECT = (buttons & GBAB_SELECT) != 0;
          frame.START = (buttons & GBAB_START) != 0;
          break;
        }
        case JOYBUS_DEVICE_N64MOUSE:
        {
          frame.HAS_BTN_START = false;
          frame.HAS_BTN_SELECT = false;
          frame.HAS_ANALOG_MAIN_BUTTONS = false;
          frame.HAS_ANALOG_DPAD = false;
          frame.analog_sticks = 0;
          frame.analog_buttons = 0;
          frame.analog_pad = 0;
          frame.paddle = 128;
          frame.spinner = 0;
          N64MouseState mouse = joybus[port]->getN64MouseState();
          if (mouse.valid) {
            frame.mouse_x = mouse.movement_x;
            const int16_t y = -static_cast<int16_t>(mouse.movement_y);
            frame.mouse_y = y > 127 ? 127 : y;
          }
          // Nintendo programming manual 26.5: left/right use A/B bits.
          frame.A = (mouse.buttons & N64B_A_BUTTON) ? 1 : 0;
          frame.B = (mouse.buttons & N64B_B_BUTTON) ? 1 : 0;
          break;
        }
        case JOYBUS_DEVICE_N64PAD:
        {
          LatencyPhaseTraceScope n64MapTrace(LATENCY_TRACE_PHASE_N64_MAP);
          constexpr DeviceEnum analogMode = RZORD_N64;
          frame.HAS_ANALOG_STICK_MAIN = true;
          const uint32_t payload = joybus[port]->getN64PadPayload();
          const uint16_t buttons = static_cast<uint16_t>(payload);
          uint32_t digital_buttons =
              ((buttons & N64B_DPAD_UP) ? INPUT_PAD_U : 0) |
              ((buttons & N64B_DPAD_DOWN) ? INPUT_PAD_D : 0) |
              ((buttons & N64B_DPAD_LEFT) ? INPUT_PAD_L : 0) |
              ((buttons & N64B_DPAD_RIGHT) ? INPUT_PAD_R : 0) |
              ((buttons & N64B_A_BUTTON) ? INPUT_A : 0) |
              ((buttons & N64B_B_BUTTON) ? INPUT_B : 0) |
              ((buttons & N64B_L_TRIGGER) ? INPUT_L1 : 0) |
              ((buttons & N64B_Z_TRIGGER) ? INPUT_L2 : 0) |
              ((buttons & N64B_R_TRIGGER) ? INPUT_R1 : 0) |
              ((buttons & N64B_START) ? INPUT_START : 0);
          int16_t raw_lx = static_cast<int8_t>(payload >> 16);
          int16_t raw_ly = invertSignedInt8Axis(
              static_cast<int8_t>(payload >> 24));
          int8_t learned_lx = 0;
          int8_t learned_ly = 0;
          applyClassicAnalogLearnN64Stick(analogMode, i, raw_lx, raw_ly, &learned_lx, &learned_ly);
          frame.LX = learned_lx;
          frame.LY = learned_ly;
          frame.RX = 0;
          frame.RY = 0;

          if (output_maps_n64_c_buttons_to_face_buttons()) {
            digital_buttons |=
                ((buttons & N64B_C_DOWN) ? INPUT_L3 : 0) |
                ((buttons & N64B_C_RIGHT) ? INPUT_R3 : 0) |
                ((buttons & N64B_C_UP) ? INPUT_X : 0) |
                ((buttons & N64B_C_LEFT) ? INPUT_Y : 0);
          } else {
            if (buttons & N64B_C_LEFT) {
              frame.RX = INT8_MIN;
            } else if (buttons & N64B_C_RIGHT) {
              frame.RX = INT8_MAX;
            }
            if (buttons & N64B_C_UP) {
              frame.RY = INT8_MIN;
            } else if (buttons & N64B_C_DOWN) {
              frame.RY = INT8_MAX;
            }
          }
          frame.digital_buttons = digital_buttons;
          break;
        }
        case JOYBUS_DEVICE_GCPAD:
        case JOYBUS_DEVICE_GCWIRELESS:
        {
          constexpr DeviceEnum analogMode = RZORD_GAMECUBE;
          frame.HAS_ANALOG_STICK_MAIN = true;
          const NormalizedGCControllerState state = joybus[port]->getGCPadState();
          const uint16_t buttons = state.buttons;
          frame.HAS_ANALOG_TRIGGERS = true;
          frame.HAS_ANALOG_STICK_AUX = true;
          frame.PAD_U = (buttons & GCB_DPAD_UP) != 0;
          frame.PAD_D = (buttons & GCB_DPAD_DOWN) != 0;
          frame.PAD_L = (buttons & GCB_DPAD_LEFT) != 0;
          frame.PAD_R = (buttons & GCB_DPAD_RIGHT) != 0;
          int16_t raw_lx = state.joystick_x;
          int16_t raw_ly = ~state.joystick_y;
          int16_t raw_rx = state.cstick_x;
          int16_t raw_ry = ~state.cstick_y;
          if (wii_analog_range == CLASSIC_ANALOG_RANGE_NORMALIZED) {
            frame.LX = normalizeGCStickAxis(raw_lx);
            frame.LY = normalizeGCStickAxis(raw_ly);
            frame.RX = normalizeGCStickAxis(raw_rx);
            frame.RY = normalizeGCStickAxis(raw_ry);
          } else if (wii_analog_range == CLASSIC_ANALOG_RANGE_LEARN) {
            int8_t learned_lx = 0;
            int8_t learned_ly = 0;
            int8_t learned_rx = 0;
            int8_t learned_ry = 0;
            applyClassicAnalogLearnStick(analogMode, i, CLASSIC_ANALOG_AXIS_LX, raw_lx, raw_ly,
                                         &learned_lx, &learned_ly);
            applyClassicAnalogLearnStick(analogMode, i, CLASSIC_ANALOG_AXIS_RX, raw_rx, raw_ry,
                                         &learned_rx, &learned_ry);
            frame.LX = learned_lx;
            frame.LY = learned_ly;
            frame.RX = learned_rx;
            frame.RY = learned_ry;
          } else {
            frame.LX = raw_lx;
            frame.LY = raw_ly;
            frame.RX = raw_rx;
            frame.RY = raw_ry;
          }
          recordClassicAnalogRangeAxis(analogMode, i, CLASSIC_ANALOG_AXIS_LX, raw_lx, frame.LX);
          recordClassicAnalogRangeAxis(analogMode, i, CLASSIC_ANALOG_AXIS_LY, raw_ly, frame.LY);
          recordClassicAnalogRangeAxis(analogMode, i, CLASSIC_ANALOG_AXIS_RX, raw_rx, frame.RX);
          recordClassicAnalogRangeAxis(analogMode, i, CLASSIC_ANALOG_AXIS_RY, raw_ry, frame.RY);
          frame.A = (buttons & GCB_A_BUTTON) != 0;
          frame.B = (buttons & GCB_B_BUTTON) != 0;
          frame.X = (buttons & GCB_X_BUTTON) != 0;
          frame.Y = (buttons & GCB_Y_BUTTON) != 0;
          frame.L1 = (buttons & GCB_L_TRIGGER) != 0;
          frame.R1 = (buttons & GCB_Z_TRIGGER) != 0;
          frame.SELECT = 0;
          frame.L2 = (buttons & GCB_L_TRIGGER) != 0;
          frame.R2 = (buttons & GCB_R_TRIGGER) != 0;
          frame.START = (buttons & GCB_START) != 0;
          uint8_t raw_l2 = state.analog_l;
          uint8_t raw_r2 = state.analog_r;
          frame.ANALOG_L2 = raw_l2;
          frame.ANALOG_R2 = raw_r2;
          if (wii_analog_range == CLASSIC_ANALOG_RANGE_NORMALIZED) {
            frame.ANALOG_L2 = normalizeGCTriggerAxis(frame.ANALOG_L2);
            frame.ANALOG_R2 = normalizeGCTriggerAxis(frame.ANALOG_R2);
          } else if (wii_analog_range == CLASSIC_ANALOG_RANGE_LEARN) {
            frame.ANALOG_L2 = applyClassicAnalogLearnTrigger(
                analogMode, i, CLASSIC_ANALOG_TRIGGER_L2, frame.ANALOG_L2,
                (buttons & GCB_L_TRIGGER) != 0);
            frame.ANALOG_R2 = applyClassicAnalogLearnTrigger(
                analogMode, i, CLASSIC_ANALOG_TRIGGER_R2, frame.ANALOG_R2,
                (buttons & GCB_R_TRIGGER) != 0);
          }
          recordClassicAnalogRangeTrigger(
              analogMode, i, CLASSIC_ANALOG_TRIGGER_L2, raw_l2, frame.ANALOG_L2);
          recordClassicAnalogRangeTrigger(
              analogMode, i, CLASSIC_ANALOG_TRIGGER_R2, raw_r2, frame.ANALOG_R2);
          break;
        }
        case JOYBUS_DEVICE_GCWHEEL:
        {
          break;
        }
      }
      decodedSticks[port].capture(frame);
      setUpdated(i);

      stageWebhidRawData(port, dtype[port]);
    } else {
      decodedSticks[port].restore(frame);
    }
#if defined(ADAPT_FEATURE_N64_CAPTURE)
    n64CaptureDecoded(port, frame, dtype[port] == JOYBUS_DEVICE_N64PAD);
#endif
  }

  if (pollIntervalNeedsUpdate) {
    constexpr uint16_t N64_COMPAT_POLL_INTERVAL_US = 500;
    pollInterval = 16000;
    bool found_16ms = false;
    bool found_8ms = false;
    bool found_n64 = false;
    bool found_1ms = false;
    for (uint8_t port = 0; port < input_ports; ++port) {
      switch (dtype[port]) {
        case JOYBUS_DEVICE_NONE:
        case JOYBUS_DEVICE_GCWHEEL:
        case JOYBUS_DEVICE_NOTSUPPORTED:
          break;
        case JOYBUS_DEVICE_GBA:
          found_16ms = true;
          break;
        case JOYBUS_DEVICE_GCWIRELESS:
          found_8ms = true;
          break;
        case JOYBUS_DEVICE_N64PAD:
        case JOYBUS_DEVICE_N64MOUSE:
          found_n64 = true;
          break;
        case JOYBUS_DEVICE_GCPAD:
          found_1ms = true;
          break;
      }
    }

    if (found_16ms) {
      pollInterval = 16000;
    } else if (found_8ms) {
      pollInterval = 8000;
    } else if (found_n64) {
      pollInterval = N64_COMPAT_POLL_INTERVAL_US;
    } else if (found_1ms) {
      pollInterval = 1000;
    }
  }

  return endPollCycle();
}

void RZInputGC64::afterOutputFrameSent(bool polled, bool updated) {
  (void)updated;
  if (polled) {
    for (uint8_t port = 0; port < input_ports; ++port) {
      if (joybus[port] == nullptr) {
        continue;
      }
      if (dtype[port] == JOYBUS_DEVICE_N64PAD) {
        uint8_t effective_left = 0;
        uint8_t effective_right = 0;
        rumbleRuntimeGetEffectiveFeedback(port, &effective_left, &effective_right);
        joybus[port]->setRumble((effective_left | effective_right) != 0);
      }
    }
  }
  if (polled) {
    for (uint8_t port = 0; port < input_ports; ++port) {
      if (joybus[port] == nullptr || dtype[port] != JOYBUS_DEVICE_N64PAD) {
        continue;
      }
      const bool rumblePakConnected = joybus[port]->isRumblePakConnected();
      if (rumblePakConnected != n64RumbleLabelShown[port]) {
        setInputFrameTypeName(port, rumblePakConnected ? "N64 Rumble" : "N64 Pad");
        n64RumbleLabelShown[port] = rumblePakConnected;
      }
    }
    for (uint8_t port = 0; port < input_ports && port < GC64_DEBUG_PORTS; ++port) {
      if (joybus[port] == nullptr) {
        continue;
      }
      gc64_debug_device_type[port] = (uint8_t)joybus[port]->deviceType();
      gc64_n64_accessory_aux[port] = joybus[port]->accessoryAux();
      gc64_n64_rumble_pak_detected[port] = joybus[port]->isRumblePakConnected();
      gc64_n64_rumble_command_pending[port] = joybus[port]->isRumbleCommandPending();
      gc64_n64_rumble_probe_attempts[port] = joybus[port]->rumbleProbeAttempts();
      gc64_n64_rumble_probe_result[port] = joybus[port]->rumbleLastProbeResult();
      gc64_n64_rumble_probe_byte[port] = joybus[port]->rumbleLastProbeByte();
      gc64_n64_rumble_motor_result[port] = joybus[port]->rumbleLastMotorResult();
      const JoybusMemoryWriteDiag& motorDiag = joybus[port]->rumbleLastMotorDiag();
      gc64_n64_rumble_motor_transport[port] = motorDiag.transport_result;
      gc64_n64_rumble_motor_expected[port] = motorDiag.expected_checksum;
      gc64_n64_rumble_motor_response[port] = motorDiag.response_checksum;
    }
  }
  flushPendingWebhidRawData();
}
