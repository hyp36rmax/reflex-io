#pragma once
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
#include "../output_directional_hotkeys.h"
#endif

inline void map_xinputw_output(uint8_t port) {
  const controller_state_t& frame = controllerFrameConst(port);
  const analog_stick_precision input_precision = frame.sticks_precision_bits;
  const analog_stick_precision output_precision = ANALOG_STICK_PRECISION_16;
  uint32_t faceButtons = applyNintendoPositionButtonMap(
    frame.digital_buttons, isPositionButtonMapActiveForXInputOutput());
  const bool n64XinputLayout = !frame.n64_output_assigned && controllerFrameTypeNameIsN64(frame);
  faceButtons = output_apply_n64_c_buttons_to_xinput_face_buttons(faceButtons, frame);
  faceButtons = output_apply_vboy_right_dpad_to_xinput_face_buttons(faceButtons, frame);
  _xinputw_report[port].a = (faceButtons & INPUT_A) != 0;
  _xinputw_report[port].b = (faceButtons & INPUT_B) != 0;
  _xinputw_report[port].x = (faceButtons & INPUT_X) != 0;
  _xinputw_report[port].y = (faceButtons & INPUT_Y) != 0;

  _xinputw_report[port].lb = n64XinputLayout ? frame.L1
                                            : (isN64ZCombined() ? (frame.L1 || frame.L2) : frame.L1);
  _xinputw_report[port].rb = frame.R1;
  _xinputw_report[port].ls = n64XinputLayout ? 0 : frame.L3;
  _xinputw_report[port].rs = n64XinputLayout ? 0 : frame.R3;
  _xinputw_report[port].back = n64XinputLayout ? frame.L2 : frame.SELECT;
  _xinputw_report[port].start = frame.START;
  _xinputw_report[port].home = frame.HOME;

  int16_t lx = convertAnalogPrecision(frame.LX, input_precision, output_precision);
  int16_t ly = convertAnalogPrecision(frame.LY, input_precision, output_precision);
  int16_t rx = convertAnalogPrecision(frame.RX, input_precision, output_precision);
  int16_t ry = convertAnalogPrecision(frame.RY, input_precision, output_precision);

  apply_xinput_dpad_and_stick_modes(port, frame, _xinputw_report[port], lx, ly, rx, ry);

  _xinputw_report[port].lx = lx;
  _xinputw_report[port].ly = ~ly;
  _xinputw_report[port].rx = rx;
  _xinputw_report[port].ry = ~ry;

  bool has_analog_triggers = frame.HAS_ANALOG_TRIGGERS;
#ifdef ENABLE_INPUT_DREAMCAST
  if (deviceMode == RZORD_DREAMCAST) {
    has_analog_triggers =
      (trigger_mode == TRIGGER_MODE_ANALOG ||
       trigger_mode == TRIGGER_MODE_RSTICK ||
       trigger_mode == TRIGGER_MODE_BOTH);
  }
#endif
  if (triggerModeForFrame(frame) == TRIGGER_MODE_DIGITAL) {
    has_analog_triggers = false;
  }
  if (n64XinputLayout) {
    const bool cButtons = output_uses_n64_xinput_button_layout(frame);
    _xinputw_report[port].lt = cButtons && frame.B ? 0xff : 0x00;
    _xinputw_report[port].rt = cButtons && frame.A ? 0xff : 0x00;
  } else if (output_uses_vboy_xinput_button_layout(frame)) {
    _xinputw_report[port].lt = frame.B ? 0xff : 0x00;
    _xinputw_report[port].rt = frame.A ? 0xff : 0x00;
  } else if (has_analog_triggers) {
    _xinputw_report[port].lt = frame.ANALOG_L2;
    _xinputw_report[port].rt = frame.ANALOG_R2;
  } else {
    uint8_t lt_btn = isN64ZCombined() ? 0 : frame.L2;
    _xinputw_report[port].lt = lt_btn ? 0xff : 0x00;
    _xinputw_report[port].rt = frame.R2 ? 0xff : 0x00;
  }
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
  applyDirectionalXinputHotkeys(_xinputw_report[port], directionalHotkeyOutputButtons(port));
#endif
}
