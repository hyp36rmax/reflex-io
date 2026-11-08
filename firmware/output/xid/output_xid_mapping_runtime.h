#pragma once
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
#include "../output_directional_hotkeys.h"
#endif

inline void map_xid_output(uint8_t port) {
  const controller_state_t& frame = controllerFrameConst(port);
  analog_stick_precision input_precision = frame.sticks_precision_bits;
  analog_stick_precision output_precision = ANALOG_STICK_PRECISION_16;
  xpad_data.dButtons = 0;
  if (frame.PAD_U) xpad_data.dButtons |= XID_DUP;
  if (frame.PAD_D) xpad_data.dButtons |= XID_DDOWN;
  if (frame.PAD_L) xpad_data.dButtons |= XID_DLEFT;
  if (frame.PAD_R) xpad_data.dButtons |= XID_DRIGHT;
  if (frame.START) xpad_data.dButtons |= XID_START;
  if (output_n64_c_backing_select(frame)) xpad_data.dButtons |= XID_BACK;
  if (frame.L3) xpad_data.dButtons |= XID_LS;
  if (frame.R3) xpad_data.dButtons |= XID_RS;

  const bool positionButtonMapActive = isPositionButtonMapActive();
  const uint32_t faceButtons = output_apply_n64_c_buttons_to_face_buttons(
    applyNintendoPositionButtonMap(frame.digital_buttons, positionButtonMapActive),
    frame);

  if (frame.HAS_ANALOG_MAIN_BUTTONS) {
    uint8_t analogA = frame.ANALOG_A;
    uint8_t analogB = frame.ANALOG_B;
    uint8_t analogX = frame.ANALOG_X;
    uint8_t analogY = frame.ANALOG_Y;
    if (positionButtonMapActive) {
      const uint8_t originalA = analogA;
      const uint8_t originalX = analogX;
      analogA = analogB;
      analogB = originalA;
      analogX = analogY;
      analogY = originalX;
    }
    xpad_data.A = analogA;
    xpad_data.B = analogB;
    xpad_data.X = analogX;
    xpad_data.Y = analogY;
    xpad_data.BLACK = frame.ANALOG_L1;
    xpad_data.WHITE = frame.ANALOG_R1;
  } else {
    xpad_data.A = (faceButtons & INPUT_A) ? 0xFF : 0x00;
    xpad_data.B = (faceButtons & INPUT_B) ? 0xFF : 0x00;
    xpad_data.X = (faceButtons & INPUT_X) ? 0xFF : 0x00;
    xpad_data.Y = (faceButtons & INPUT_Y) ? 0xFF : 0x00;
    xpad_data.BLACK = frame.L1 ? 0xFF : 0x00;
    xpad_data.WHITE = frame.R1 ? 0xFF : 0x00;
  }

  if (frame.HAS_ANALOG_TRIGGERS) {
    xpad_data.L = frame.ANALOG_L2;
    xpad_data.R = frame.ANALOG_R2;
  } else {
    xpad_data.L = frame.L2 ? 0xFF : 0x00;
    xpad_data.R = output_n64_c_backing_r2(frame) ? 0xFF : 0x00;
  }

  int16_t lx = convertAnalogPrecision(frame.LX, input_precision, output_precision);
  int16_t ly = convertAnalogPrecision(frame.LY, input_precision, output_precision);
  int16_t rx = convertAnalogPrecision(frame.RX, input_precision, output_precision);
  int16_t ry = convertAnalogPrecision(frame.RY, input_precision, output_precision);

  if (stick_invert & 0x01) ly = -ly;
  if (stick_invert & 0x02) ry = -ry;

  xpad_data.leftStickX = lx;
  xpad_data.leftStickY = ~ly;
  xpad_data.rightStickX = rx;
  xpad_data.rightStickY = ~ry;
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
  applyDirectionalXidHotkeys(xpad_data, directionalHotkeyOutputButtons(port));
#endif
}
