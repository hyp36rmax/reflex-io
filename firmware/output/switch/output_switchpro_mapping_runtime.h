#pragma once
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
#include "../output_directional_hotkeys.h"
#endif

#include <cstring>

#include "output_switch_gamecube_mapping.h"
#include "output_switch_genesis_nso_mapping.h"
#include "output_switch_hotkey_policy.h"



// Fills a Switch Pro (057E:2009) report for one source frame. Switch 2 Pro
// output repacks the same report, so both personas map buttons alike.
inline void map_switchpro_report(SwitchReport& _switchReport, uint8_t port, uint8_t sourcePort) {
  (void)port;  // Read only by the Jaguar rotary check.
  const controller_state_t& frame = controllerFrameConst(sourcePort);
  analog_stick_precision input_precision = frame.sticks_precision_bits;
  analog_stick_precision output_precision = ANALOG_STICK_PRECISION_12;
  const uint16_t analog_mid = 0x800;
  bool virtual_boy_on_switch = false;
  bool gamecube_on_switch = false;
  bool megadrive_on_switch = false;
  bool saturn_digital_on_switch = false;
  bool nes_on_switch = false;
  bool nes_nso_on_switch = false;
  bool snes_nso_on_switch = false;
  bool genesis_nso_on_switch = false;
  bool generic_switch_input = false;
  bool generic_genesis_nso_on_switch = false;
  bool genesis_pro_layout_on_switch = false;
  bool pce_on_switch = false;
  bool pce_six_button_on_switch = false;
  bool n64_on_switch = false;
  bool n64_nso_on_switch = false;
  bool three_do_on_switch = false;
  bool jaguar_on_switch = false;
#ifdef ENABLE_INPUT_VBOY
  virtual_boy_on_switch = (deviceMode == RZORD_VBOY);
#endif
#ifdef ENABLE_INPUT_GAMECUBE
  gamecube_on_switch = (deviceMode == RZORD_GAMECUBE);
#endif
#ifdef ENABLE_INPUT_MEGADRIVE
  megadrive_on_switch = (deviceMode == RZORD_MEGADRIVE);
  genesis_nso_on_switch =
    megadrive_on_switch && output_uses_switch_genesis_profile();
  genesis_pro_layout_on_switch = megadrive_on_switch && !genesis_nso_on_switch;
#endif
#ifdef ENABLE_INPUT_USB
  generic_switch_input = generic_switch_input || deviceMode == RZORD_USB;
#endif
#ifdef ENABLE_INPUT_JVS
  generic_switch_input = generic_switch_input || deviceMode == RZORD_JVS;
#endif
#ifdef ENABLE_INPUT_SATURN
  saturn_digital_on_switch =
    (deviceMode == RZORD_SATURN) &&
    std::strcmp(frame.controller_type_name, "Saturn") == 0;
#endif
#ifdef ENABLE_INPUT_N64
  n64_on_switch = deviceMode == RZORD_N64 && !frame.n64_output_assigned;
  // Match the report mapping to the USB identity that is actually enumerated.
  n64_nso_on_switch =
    n64_on_switch && output_uses_switch_n64_profile();
#endif
#ifdef ENABLE_INPUT_PCE
  pce_on_switch = deviceMode == RZORD_PCE;
  pce_six_button_on_switch =
    pce_on_switch &&
    std::strcmp(frame.controller_type_name, "6-Button") == 0;
#endif
#ifdef ENABLE_INPUT_3DO
  three_do_on_switch = deviceMode == RZORD_3DO;
#endif
#ifdef ENABLE_INPUT_JAGUAR
  jaguar_on_switch = deviceMode == RZORD_JAGUAR;
#endif
#ifdef ENABLE_INPUT_NES
  nes_on_switch = deviceMode == RZORD_NES;
  nes_nso_on_switch =
    nes_on_switch && output_uses_switch_nes_profile();
#endif
#ifdef ENABLE_INPUT_SNES
  snes_nso_on_switch =
    (deviceMode == RZORD_SNES) && output_uses_switch_snes_profile();
#endif
  if (generic_switch_input && is_nso_special_active()) {
    nes_nso_on_switch = nes_nso_on_switch || output_uses_switch_nes_profile();
    snes_nso_on_switch = snes_nso_on_switch || output_uses_switch_snes_profile();
    n64_nso_on_switch = n64_nso_on_switch || output_uses_switch_n64_profile();
    generic_genesis_nso_on_switch = output_uses_switch_genesis_profile();
    genesis_nso_on_switch = genesis_nso_on_switch || generic_genesis_nso_on_switch;
  }

  const bool native_nso_face_layout =
    nes_nso_on_switch || snes_nso_on_switch ||
    genesis_nso_on_switch;

  uint8_t effective_dpad_mode = effective_dpad_mode_for_sticks(
    std::strcmp(frame.controller_type_name, "Fishing") == 0 ? uint8_t(0) : dpad_mode,
    frame.HAS_ANALOG_STICK_MAIN,
    frame.HAS_ANALOG_STICK_AUX);
  bool isPopnOrGuitarFreaks = frame.PAD_L && frame.PAD_R;
  bool dancePadDetected = frame.PAD_U && frame.PAD_D && !isPopnOrGuitarFreaks;
  if (dancePadDetected) {
    effective_dpad_mode = DPAD_MODE_BUTTONS;
  }
  if (generic_switch_input &&
      (nes_nso_on_switch || snes_nso_on_switch || genesis_nso_on_switch)) {
    effective_dpad_mode = DPAD_MODE_DPAD;
  }

  const bool position_button_map = isPositionButtonMapActive();
  const uint32_t faceButtons = output_apply_n64_c_buttons_to_face_buttons(
    frame.digital_buttons, frame);

  if (virtual_boy_on_switch) {
    // The right D-pad occupies the modern ABXY diamond while the two named
    // buttons remain available on matching A/B aliases. Name/Position affects
    // only the right-D-pad X/Y positions.
    _switchReport.a = frame.A || frame.R2;
    _switchReport.b = frame.B || frame.L2;
    _switchReport.x =
      position_button_map ? frame.Y : frame.X;
    _switchReport.y =
      position_button_map ? frame.X : frame.Y;
  } else if (n64_nso_on_switch) {
    // Native NSO N64 uses Y/X for C-Up/C-Left.
    _switchReport.a = frame.A;
    _switchReport.b = frame.B;
    _switchReport.x = frame.Y;
    _switchReport.y = frame.X;
  } else if (n64_on_switch) {
    // Preserve the named N64 A/B controls while placing C-Right, C-Down,
    // C-Up, and C-Left on the corresponding modern face-button positions.
    _switchReport.a =
      (position_button_map ? frame.B : frame.A) || frame.R3;
    _switchReport.b =
      (position_button_map ? frame.A : frame.B) || frame.L3;
    _switchReport.x = frame.X;
    _switchReport.y = frame.Y;
  } else if (native_nso_face_layout) {
    // Native NES, SNES, and Genesis controllers report their printed labels.
    _switchReport.a = frame.A;
    _switchReport.b = frame.B;
    _switchReport.x = frame.X;
    _switchReport.y = frame.Y;
  } else if (nes_on_switch) {
    // Keep physical NES A/B names when using the standard Switch Pro identity.
    _switchReport.a = frame.A;
    _switchReport.b = frame.B;
    _switchReport.x = frame.X;
    _switchReport.y = frame.Y;
  } else if (pce_on_switch && !pce_six_button_on_switch) {
    // A two-button PCE pad is laid out like NES: I is Switch A, II is Switch B.
    _switchReport.a = frame.B;
    _switchReport.b = frame.A;
    _switchReport.x = 0;
    _switchReport.y = 0;
  } else if (three_do_on_switch) {
    // Match the physical Genesis A/B/C row in NSO Genesis software.
    _switchReport.a = frame.X;  // 3DO C
    _switchReport.b = frame.B;
    _switchReport.x = 0;
    _switchReport.y = frame.A;
  } else if (position_button_map) {
    _switchReport.b = (faceButtons & INPUT_A) != 0;
    _switchReport.a = (faceButtons & INPUT_B) != 0;
    _switchReport.y = (faceButtons & INPUT_X) != 0;
    _switchReport.x = (faceButtons & INPUT_Y) != 0;
  } else {
    _switchReport.a = (faceButtons & INPUT_A) != 0;
    _switchReport.b = (faceButtons & INPUT_B) != 0;
    _switchReport.x = (faceButtons & INPUT_X) != 0;
    _switchReport.y = (faceButtons & INPUT_Y) != 0;
  }
  _switchReport.l =
    genesis_nso_on_switch
      ? frame.L1
      : (n64_nso_on_switch
          ? frame.L1
          : (isN64ZCombined() ? (frame.L1 || frame.L2) : frame.L1));
  _switchReport.r =
    genesis_nso_on_switch
      ? frame.R1
      : frame.R1;

  const bool triggers_to_right_stick = (trigger_mode == TRIGGER_MODE_RSTICK) && frame.HAS_ANALOG_TRIGGERS;

  if (virtual_boy_on_switch) {
    _switchReport.zl = frame.START && frame.L1;
    _switchReport.zr = frame.START && frame.R1;
  } else if (n64_nso_on_switch) {
    _switchReport.zl = frame.L2;
    _switchReport.zr = frame.L3;
  } else if (genesis_nso_on_switch) {
    _switchReport.zl = 0;
    _switchReport.zr = frame.R2 || frame.SELECT;  // Mode / Select
  } else if (saturn_digital_on_switch) {
    _switchReport.zl = frame.L2;
    _switchReport.zr = frame.R2;
  } else if (triggers_to_right_stick) {
    _switchReport.zl = 0;
    _switchReport.zr = 0;
  } else if (frame.HAS_ANALOG_TRIGGERS) {
    _switchReport.zl = frame.L2 | (frame.ANALOG_L2 > 100);
    _switchReport.zr =
      output_n64_c_backing_r2(frame) | (frame.ANALOG_R2 > 100);
  } else {
    _switchReport.zl = isN64ZCombined() ? 0 : frame.L2;
    _switchReport.zr = output_n64_c_backing_r2(frame);
  }

  if (gamecube_on_switch) {
    const switch_gamecube::LeftShoulderButtons leftShoulder =
        switch_gamecube::map_left_shoulder(
            frame.L1 || frame.L2,
            frame.ANALOG_L2 > TRIGGER_DIGITAL_THRESHOLD,
            triggers_to_right_stick,
            gamecube_l_switch_mode);
    _switchReport.l = leftShoulder.l;
    _switchReport.zl = leftShoulder.zl;
  }

  _switchReport.l3 = n64_nso_on_switch ? 0 : frame.L3;
  _switchReport.r3 = n64_nso_on_switch ? 0 : frame.R3;
  _switchReport.plus = frame.START;
  if (n64_nso_on_switch) {
    _switchReport.minus = frame.R3;
  } else if (nes_nso_on_switch || snes_nso_on_switch) {
    _switchReport.minus = frame.SELECT;
  } else if (pce_on_switch) {
    _switchReport.minus = frame.SELECT;
  } else if (genesis_nso_on_switch) {
    _switchReport.minus = 0;
  } else {
    _switchReport.minus =
      output_n64_c_backing_select(frame);
  }
  _switchReport.home =
    frame.HOME;
  _switchReport.capture = frame.CAPTURE;

  if (generic_genesis_nso_on_switch) {
    // Reverse USB-host positional normalization into the native 057E:201E
    // report so an attached official Genesis NSO pad round-trips losslessly.
    switch_genesis_nso::apply_neutral_native_button_bits(
      frame, _switchReport);
  } else if (genesis_pro_layout_on_switch) {
    // Make the empirically verified Switch Pro-identity compensation the final
    // authority for every button byte. D-pad-to-stick modes may still clear
    // the D-pad below.
    switch_genesis_nso::apply_button_bits(
      frame, _switchReport);
  } else if (saturn_digital_on_switch) {
    // Standard Saturn pads use the same compensated six-button layout as
    // Genesis, while Saturn L/R remain ZL/ZR.
    switch_genesis_nso::apply_six_button_position_bits(
      frame, _switchReport);
  } else if (pce_six_button_on_switch) {
    // Match the six physical PCE positions to compensated Genesis while
    // preserving PCE Select, Run, Home, Capture, and D-pad report bits.
    switch_genesis_nso::apply_pce_six_button_position_bits(
      frame, _switchReport);
  } else if (jaguar_on_switch) {
    // Match Jaguar Pro's six useful controls to the physical Genesis layout.
    switch_genesis_nso::apply_jaguar_six_button_position_bits(
      frame, _switchReport);
  }

  int16_t lx = convertAnalogPrecision(frame.LX, input_precision, output_precision);
  int16_t ly = convertAnalogPrecision(frame.LY, input_precision, output_precision);
  int16_t rx = convertAnalogPrecision(frame.RX, input_precision, output_precision);
  int16_t ry = convertAnalogPrecision(frame.RY, input_precision, output_precision);


  if (generic_switch_input &&
      (nes_nso_on_switch || snes_nso_on_switch || genesis_nso_on_switch)) {
    // Native NES/SNES/Genesis personalities are digital-only even when a
    // modern analog source deliberately emulates them.
    lx = 0;
    ly = 0;
    rx = 0;
    ry = 0;
  }
  if (effective_dpad_mode == DPAD_MODE_LEFT_STICK || effective_dpad_mode == DPAD_MODE_RIGHT_STICK) {
    int16_t dpad_x = 0;
    int16_t dpad_y = 0;
    if (frame.PAD_L) dpad_x = -2047;
    if (frame.PAD_R) dpad_x = 2047;
    if (frame.PAD_U) dpad_y = -2047;
    if (frame.PAD_D) dpad_y = 2047;

    if (effective_dpad_mode == DPAD_MODE_LEFT_STICK) {
      lx = dpad_x;
      ly = dpad_y;
    } else {
      rx = dpad_x;
      ry = dpad_y;
    }

    _switchReport.pad_u = 0;
    _switchReport.pad_d = 0;
    _switchReport.pad_l = 0;
    _switchReport.pad_r = 0;
  } else {
    _switchReport.pad_u = frame.PAD_U;
    _switchReport.pad_d = frame.PAD_D;
#ifdef ENABLE_INPUT_JAGUAR
    if (jaguarRotaryActiveOnPort(port)) {
      _switchReport.pad_l = 0;
      _switchReport.pad_r = 0;
    } else
#endif
    {
      _switchReport.pad_l = frame.PAD_L;
      _switchReport.pad_r = frame.PAD_R;
    }
  }

  // Virtual Boy's producer owns the Right D-pad preference: Buttons leaves
  // RX/RY centered, Stick supplies the axes above. Physical A/B are separate
  // controls, not right/down directions; never reconstruct axes from them.

  if (stick_invert & 0x01) ly = -ly;
  if (stick_invert & 0x02) ry = -ry;

  _switchReport.lx = lx + analog_mid;
  _switchReport.ly = ~ly + analog_mid;
  _switchReport.rx = rx + analog_mid;
  _switchReport.ry = ~ry + analog_mid;

  if (triggers_to_right_stick) {
    const int16_t gas = (int16_t)frame.ANALOG_R2 - 128;
    const int16_t brake = (int16_t)frame.ANALOG_L2 - 128;
    const int16_t combined = gas - brake;  // -255..+255
    const int32_t scaled = ((int32_t)combined + 255) * 4095 + 255;
    _switchReport.ry = (uint16_t)(scaled / 510);
  }
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
  auto& shortcutReport = _switchReport;
  const uint32_t shortcuts = directionalHotkeyOutputButtons(sourcePort);
  applyDirectionalSwitchHotkeys(shortcutReport, shortcuts, output_uses_switch_n64_profile());
  // Synthesized Select is the Minus bit on every Switch identity, never the
  // identity's NSO Menu button. Native Genesis has no Minus control of its
  // own; native N64 reports C-Right on this bit, so its app reads C-Right.
  if (shortcuts & INPUT_SELECT) shortcutReport.minus=1;
#endif
  // System actions follow the enumerated persona even after input remapping.
  const auto shortcutProfile = switch_hotkey::profile(
      output_uses_switch_nes_profile(), output_uses_switch_snes_profile(),
      output_uses_switch_genesis_profile(), output_uses_switch_n64_profile());
  bool capture = frame.CAPTURE;
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
  capture = capture || (shortcuts & INPUT_CAPTURE);
#endif
  switch_hotkey::applySystemButtons(_switchReport,
                                   frame.HOME, capture, shortcutProfile);
}

inline void map_switchpro_output(uint8_t port, uint8_t sourcePort) {
  map_switchpro_report(switchpro[port]->switchCommon->_switchReport, port, sourcePort);
}
