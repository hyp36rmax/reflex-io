#pragma once

#include <stdint.h>
#include <string.h>

#include "controller_state.h"
#include "device_mode.h"
#include "pointer_mouse_config.h"
#include "remap_table.h"
#include "rotary_output_transform.h"

// Mouse output settings for one input mode (Quick Menu -> Mouse): which
// controller button sends each mouse button, what turns the wheel, and
// whether a rotary controller turns the wheel instead of moving the pointer.
// They live in the remap pair store (remap_pair_store.h) under a reserved
// output key, one entry per input mode, so no EEPROM record moves. Byte i
// holds i plus field i: the all-default settings are the identity bytes,
// which the store keeps as no entry at all.

enum RotaryMouseButtonRole : uint8_t {
  ROTARY_MOUSE_LEFT = 0,
  ROTARY_MOUSE_MIDDLE,
  ROTARY_MOUSE_RIGHT,
  ROTARY_MOUSE_BUTTON4,  // Back
  ROTARY_MOUSE_BUTTON5,  // Forward
  ROTARY_MOUSE_ROLE_COUNT,
};

// What scrolls. Default is the right stick on a controller with one that is
// not moving the pointer, otherwise the D-pad (owner, 2026-10-25). Stored
// values keep their first meanings; 2 was L / R before L and R became
// buttons 4 and 5.
enum RotaryMouseWheelSource : uint8_t {
  ROTARY_MOUSE_WHEEL_DEFAULT = 0,
  ROTARY_MOUSE_WHEEL_LEFT_STICK,
  ROTARY_MOUSE_WHEEL_DPAD,  // Up scrolls up, Down down
  ROTARY_MOUSE_WHEEL_OFF,
  ROTARY_MOUSE_WHEEL_RIGHT_STICK,
  ROTARY_MOUSE_WHEEL_SOURCE_COUNT,
};

// A button field is the controller's default button, none, or a remap slot.
constexpr uint8_t ROTARY_MOUSE_BUTTON_DEFAULT = 0;
constexpr uint8_t ROTARY_MOUSE_BUTTON_NONE = 1;
constexpr uint8_t ROTARY_MOUSE_BUTTON_SLOT_BASE = 2;
constexpr uint8_t ROTARY_MOUSE_NO_SLOT = 0xFF;

// Real output modes stay far below this key.
constexpr uint8_t ROTARY_MOUSE_SETTINGS_PAIR_KEY = 0xF0;

struct RotaryMouseSettings {
  uint8_t buttons[ROTARY_MOUSE_ROLE_COUNT];
  uint8_t wheel;         // RotaryMouseWheelSource
  uint8_t rotary_wheel;  // 1: a rotary controller turns the wheel
  uint8_t two_players;   // 1: expose a separate mouse for each physical port
};

constexpr uint8_t ROTARY_MOUSE_SETTINGS_FIELDS = ROTARY_MOUSE_ROLE_COUNT + 3;
static_assert(ROTARY_MOUSE_SETTINGS_FIELDS <= REMAP_MAX_BUTTONS,
              "Mouse settings must fit one remap pair entry");

inline void rotaryMouseSettingsToBytes(const RotaryMouseSettings& settings,
                                       uint8_t (&bytes)[REMAP_MAX_BUTTONS]) {
  for (uint8_t i = 0; i < REMAP_MAX_BUTTONS; ++i) bytes[i] = i;
  for (uint8_t role = 0; role < ROTARY_MOUSE_ROLE_COUNT; ++role) {
    bytes[role] = static_cast<uint8_t>(role + settings.buttons[role]);
  }
  bytes[ROTARY_MOUSE_ROLE_COUNT] =
      static_cast<uint8_t>(ROTARY_MOUSE_ROLE_COUNT + settings.wheel);
  bytes[ROTARY_MOUSE_ROLE_COUNT + 1] =
      static_cast<uint8_t>(ROTARY_MOUSE_ROLE_COUNT + 1 + (settings.rotary_wheel ? 1 : 0));
  bytes[ROTARY_MOUSE_ROLE_COUNT + 2] =
      static_cast<uint8_t>(ROTARY_MOUSE_ROLE_COUNT + 2 + (settings.two_players == 1 ? 1 : 0));
}

// A field out of range (foreign or damaged bytes) reads as its default.
inline RotaryMouseSettings rotaryMouseSettingsFromBytes(const uint8_t (&bytes)[REMAP_MAX_BUTTONS]) {
  RotaryMouseSettings settings{};
  for (uint8_t role = 0; role < ROTARY_MOUSE_ROLE_COUNT; ++role) {
    const uint8_t field = static_cast<uint8_t>(bytes[role] - role);
    settings.buttons[role] =
        field < ROTARY_MOUSE_BUTTON_SLOT_BASE + REMAP_MAX_BUTTONS ? field : ROTARY_MOUSE_BUTTON_DEFAULT;
  }
  const uint8_t wheel = static_cast<uint8_t>(bytes[ROTARY_MOUSE_ROLE_COUNT] - ROTARY_MOUSE_ROLE_COUNT);
  settings.wheel = wheel < ROTARY_MOUSE_WHEEL_SOURCE_COUNT
      ? wheel : static_cast<uint8_t>(ROTARY_MOUSE_WHEEL_DEFAULT);
  settings.rotary_wheel =
      static_cast<uint8_t>(bytes[ROTARY_MOUSE_ROLE_COUNT + 1] - (ROTARY_MOUSE_ROLE_COUNT + 1)) == 1 ? 1 : 0;
  settings.two_players =
      static_cast<uint8_t>(bytes[ROTARY_MOUSE_ROLE_COUNT + 2] - (ROTARY_MOUSE_ROLE_COUNT + 2)) == 1 ? 1 : 0;
  return settings;
}

inline uint8_t rotaryMousePlayerCount(const RotaryMouseSettings& settings) {
  return settings.two_players == 1 ? 2 : 1;
}

// Default buttons (owner, 2026-10-25). Pads with a West button (neutral X)
// and the neGcon click with West, South and East, and L and R are buttons 4
// and 5 (Back and Forward): SNES Y/B/A, Xbox X/A/B, PlayStation
// Square/Cross/Circle and neGcon II/I/A are left, middle and right. A Namco
// Volume (neGcon A and Start only) therefore clicks right with its B.
// Controllers without a West button, and the Jaguar (whose L1/R1 are keypad
// keys), keep the first layout, neutral A/X/B with Start and Select as
// buttons 4 and 5: the Driving fire button, NES A, SMS 1 and Jaguar B stay
// the left click. modeButtons is the input mode's remap slot mask
// (getInputButtonMask); a neGcon's L is a press there too, though its remap
// list leaves it out.
inline uint8_t rotaryMouseDefaultSlot(uint32_t modeButtons, bool keepFirstLayout,
                                      bool negcon, uint8_t role) {
  static constexpr uint8_t kWest[ROTARY_MOUSE_ROLE_COUNT] = {2, 0, 1, 4, 5};
  static constexpr uint8_t kFirst[ROTARY_MOUSE_ROLE_COUNT] = {0, 2, 1, 10, 11};
  if (role >= ROTARY_MOUSE_ROLE_COUNT) return ROTARY_MOUSE_NO_SLOT;
  const bool west = (modeButtons & (1UL << 2)) != 0;
  const uint8_t slot = negcon || (west && !keepFirstLayout) ? kWest[role] : kFirst[role];
  return negcon || (modeButtons & (1UL << slot)) != 0 ? slot : ROTARY_MOUSE_NO_SLOT;
}

// The pointer's stick on Mouse output. A mouse-only device has nothing else
// to move it, so Off (and Touchpad, which classic controllers lack) mean the
// left stick; Right falls back to the left stick on a controller without
// one, and a controller with only a right stick uses it.
inline bool rotaryMouseUsesRightStick(const controller_state_t& frame, PointerMouseSource source) {
  if (!frame.HAS_ANALOG_STICK_MAIN) return true;
  return frame.HAS_ANALOG_STICK_AUX && source == POINTER_MOUSE_RIGHT_STICK;
}

// The wheel source a setting means for this controller: Default is the right
// stick when it is there and not moving the pointer, otherwise the D-pad.
inline uint8_t rotaryMouseResolvedWheel(uint8_t wheel, const controller_state_t& frame,
                                        PointerMouseSource source) {
  if (wheel != ROTARY_MOUSE_WHEEL_DEFAULT) return wheel;
  const bool rightFree = rotaryMouseStickFrame(frame) && frame.HAS_ANALOG_STICK_AUX &&
                         !rotaryMouseUsesRightStick(frame, source);
  return rightFree ? ROTARY_MOUSE_WHEEL_RIGHT_STICK : ROTARY_MOUSE_WHEEL_DPAD;
}

inline bool rotaryMouseKeepsFirstLayout(uint8_t mode) {
#ifdef ENABLE_INPUT_JAGUAR
  return mode == RZORD_JAGUAR;
#else
  (void)mode;
  return false;
#endif
}

inline bool rotaryMouseNativeFrame(const controller_state_t& frame) {
  return strcmp(frame.controller_type_name, "SNES Mouse") == 0 ||
         strcmp(frame.controller_type_name, "Saturn Mouse") == 0 ||
         strcmp(frame.controller_type_name, "Sega Mouse") == 0 ||
         strcmp(frame.controller_type_name, "PSX Mouse") == 0 ||
         strcmp(frame.controller_type_name, "N64 Mouse") == 0 ||
         strcmp(frame.controller_type_name, "3DO Mouse") == 0 ||
         strcmp(frame.controller_type_name, "PCE Mouse") == 0 ||
         strcmp(frame.controller_type_name, "DC Mouse") == 0 ||
         strcmp(frame.controller_type_name, "CD-i Mouse") == 0 ||
         strcmp(frame.controller_type_name, "Sports Pad") == 0;
}

inline uint8_t rotaryMouseButtonSlot(const RotaryMouseSettings& settings, uint32_t modeButtons,
                                     bool keepFirstLayout, bool negcon, uint8_t role,
                                     const controller_state_t* frame = nullptr) {
  if (role >= ROTARY_MOUSE_ROLE_COUNT) return ROTARY_MOUSE_NO_SLOT;
  const uint8_t field = settings.buttons[role];
  if (field == ROTARY_MOUSE_BUTTON_DEFAULT) {
    if (frame && rotaryMouseNativeFrame(*frame)) {
      const bool fourButtons = strcmp(frame->controller_type_name, "Saturn Mouse") == 0 ||
                               strcmp(frame->controller_type_name, "Sega Mouse") == 0 ||
                               strcmp(frame->controller_type_name, "DC Mouse") == 0;
      const bool middle = fourButtons || strcmp(frame->controller_type_name, "3DO Mouse") == 0;
      const bool extra = fourButtons || strcmp(frame->controller_type_name, "PCE Mouse") == 0;
      static constexpr uint8_t slots[] = {0, 2, 1, 10, ROTARY_MOUSE_NO_SLOT};
      if (role == ROTARY_MOUSE_MIDDLE && !middle) return ROTARY_MOUSE_NO_SLOT;
      if (role == ROTARY_MOUSE_BUTTON4 && !extra) return ROTARY_MOUSE_NO_SLOT;
      if (role == ROTARY_MOUSE_BUTTON5 && strcmp(frame->controller_type_name, "PCE Mouse") == 0) return 11;
      return slots[role];
    }
    return rotaryMouseDefaultSlot(modeButtons, keepFirstLayout, negcon, role);
  }
  if (field == ROTARY_MOUSE_BUTTON_NONE) return ROTARY_MOUSE_NO_SLOT;
  return static_cast<uint8_t>(field - ROTARY_MOUSE_BUTTON_SLOT_BASE);
}

// The remapped frame's state of one remap slot. The extra slots follow the
// remap menu's EXTRA bit order (button_remap_menu.cpp).
inline bool rotaryMouseSlotPressed(const controller_state_t& frame, uint8_t slot) {
  switch (slot) {
    case 0: return frame.A;
    case 1: return frame.B;
    case 2: return frame.X;
    case 3: return frame.Y;
    case 4: return frame.L1;
    case 5: return frame.R1;
    case 6: return frame.L2;
    case 7: return frame.R2;
    case 8: return frame.L3;
    case 9: return frame.R3;
    case 10: return frame.START;
    case 11: return frame.SELECT;
    case 12: return frame.HOME;
    case 13: return frame.CAPTURE;
    case 14: return (frame.EXTRA & (1u << 1)) != 0;
    case 15: return (frame.EXTRA & (1u << 4)) != 0;
    case 16: return (frame.EXTRA & (1u << 0)) != 0;
    case 17: return (frame.EXTRA & (1u << 2)) != 0;
    case 18: return (frame.EXTRA & (1u << 3)) != 0;
    case 19: return frame.PAD_U;
    case 20: return frame.PAD_D;
    case 21: return frame.PAD_L;
    case 22: return frame.PAD_R;
    default: return false;
  }
}

// C++ linkage even where out_usb.h includes this inside extern "C".
// The stored settings of an input mode; defaults when it has none.
extern "C++" RotaryMouseSettings rotaryMouseSettingsRead(uint8_t input);
// Stages them in the remap pair store; false when the store is full.
extern "C++" bool rotaryMouseSettingsWrite(uint8_t input, const RotaryMouseSettings& settings);
// Changes whenever a write succeeds, so readers can cache.
extern "C++" uint16_t rotaryMouseSettingsGeneration();
extern "C++" uint8_t rotaryMouseOutputPlayerCount();
