#include "../product_config.h"

#include "menu_pad_layouts_internal.h"

namespace menu_pad_layouts_internal {

const PadButton padLayoutWii[] = {
  { GPAD_L1, 0, 0 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_L2, 0, (1 * 6) + 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R2, 0, (8 * 6) - 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R1, 0, 9 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_SELECT, 2, (3 * 6) + 3, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_HOME, 2, (4 * 6) + 3, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_START, 2, (5 * 6) + 3, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_X, 1, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_Y, 2, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 2, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 3, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_WII_COUNT = sizeof(padLayoutWii) / sizeof(PadButton);

const PadButton padLayoutWiiNunchuk[] = {
  { GPAD_A, 1, 5 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 2, 5 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_WII_NUNCHUK_COUNT =
  sizeof(padLayoutWiiNunchuk) / sizeof(PadButton);

// Bluetooth Wii Remote held horizontally. BlueRetro rotates the native D-pad
// into the shared neutral directions and maps 1/2/B/A to X/A/B/Y.
const PadButton padLayoutWiiRemote[] = {
  { GPAD_UP,     1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN,   3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT,   2, 0 * 6, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT,  2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_SELECT, 3, 4 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_HOME,   2, 4 * 6, PAD_WIDE_CIRCLE_LEFT_ON, PAD_WIDE_CIRCLE_LEFT_OFF },
  { GPAD_START,  1, 4 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_Y,      2, 6 * 6, PAD_FACE_ON, PAD_FACE_OFF }, // Wii A
  { GPAD_B,      0, 3 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF }, // Wii B
  { GPAD_X,      2, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF }, // Wii 1
  { GPAD_A,      2, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF }, // Wii 2
};
const uint8_t PAD_LAYOUT_WII_REMOTE_COUNT =
    sizeof(padLayoutWiiRemote) / sizeof(PadButton);

// Four-shoulder layouts inset L1/R1 by half a 6-pixel character so the inner
// pair remains visually separate from L2/R2.
const PadButton padLayoutWiiUPro[] = {
  { GPAD_L2, 0, 0 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_L1, 0, (3 * 6) + 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R1, 0, (6 * 6) - 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R2, 0, 9 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_SELECT, 1, 3 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_HOME, 1, (4 * 6) + 1, PAD_WIDE_CIRCLE_LEFT_ON, PAD_WIDE_CIRCLE_LEFT_OFF },
  { GPAD_START, 1, 6 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_L3, 3, 3 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_R3, 3, 6 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_Y, 1, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_X, 2, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 2, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 3, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_WII_U_PRO_COUNT =
    sizeof(padLayoutWiiUPro) / sizeof(PadButton);

const PadButton padLayoutUsbModern[] = {
  { GPAD_L2, 0, 0 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_L1, 0, (3 * 6) + 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R1, 0, (6 * 6) - 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R2, 0, 9 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_SELECT, 1, 3 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_HOME, 1, (4 * 6) + 1, PAD_WIDE_CIRCLE_LEFT_ON, PAD_WIDE_CIRCLE_LEFT_OFF },
  { GPAD_START, 1, 6 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_L3, 3, 3 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_R3, 3, 6 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_Y, 1, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_X, 2, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 2, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 3, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_USB_MODERN_COUNT =
    sizeof(padLayoutUsbModern) / sizeof(PadButton);

// Generic six-face controllers keep ABXY plus explicit C/Z destinations.
// Capability filtering removes shoulders, triggers, grips, Home, and stick
// clicks that the user's classified controller does not expose.
const PadButton padLayoutUsbSixFace[] = {
  { GPAD_L2, 0, 0 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_L1, 0, (3 * 6) + 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R1, 0, (6 * 6) - 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R2, 0, 9 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_SELECT, 1, 3 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_HOME, 1, (4 * 6) + 1, PAD_WIDE_CIRCLE_LEFT_ON, PAD_WIDE_CIRCLE_LEFT_OFF },
  { GPAD_START, 1, 6 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_L3, 3, 3 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_R3, 3, 6 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_EXTRA2, 3, 4 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_EXTRA3, 3, 5 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_X, 1, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_Y, 1, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_EXTRA4, 1, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 3, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 3, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_EXTRA1, 3, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_USB_SIX_FACE_COUNT =
    sizeof(padLayoutUsbSixFace) / sizeof(PadButton);

// HORI OCTA Pro PS4/PS5 six-face PlayStation layout. It has one analog stick.
// R3/L3/L1/L2 form one upper row. The left stick click and upper L3 share one
// logical L3 bit, so both glyphs illuminate together. R1/R2 remain the right
// column of the six-button face cluster.
const PadButton padLayoutUsbOctaPro[] = {
  { GPAD_R3, 0, 0 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_L3, 0, 2 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_L1, 0, 7 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_L2, 0, 9 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_SELECT, 1, (3 * 6) + 2, PAD_VERTICAL_RECT_ON, PAD_VERTICAL_RECT_OFF },
  { GPAD_EXTRA0, 1, (4 * 6) + 3, PAD_WIDE_RECT_LEFT_ON, PAD_WIDE_RECT_LEFT_OFF },
  { GPAD_START, 1, (6 * 6) + 3, PAD_VERTICAL_RECT_ON, PAD_VERTICAL_RECT_OFF },
  { GPAD_HOME, 2, (4 * 6) + 3, PAD_WIDE_CIRCLE_LEFT_ON, PAD_WIDE_CIRCLE_LEFT_OFF },
  { GPAD_L3, 3, 3 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_X, 2, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_Y, 2, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_R1, 2, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 3, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 3, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_R2, 3, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_USB_OCTA_PRO_COUNT =
    sizeof(padLayoutUsbOctaPro) / sizeof(PadButton);

// XInput uses the PlayStation/Wii U half-character inward shoulder spacing.
const PadButton padLayoutUsbXInput[] = {
  { GPAD_L2, 0, 0 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_L1, 0, (3 * 6) + 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R1, 0, (6 * 6) - 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R2, 0, 9 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_SELECT, 1, 3 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_HOME, 1, (4 * 6) + 1, PAD_WIDE_CIRCLE_LEFT_ON, PAD_WIDE_CIRCLE_LEFT_OFF },
  { GPAD_START, 1, 6 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_L3, 3, 3 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_R3, 3, 6 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_Y, 1, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_X, 2, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 2, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 3, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_USB_XINPUT_COUNT =
    sizeof(padLayoutUsbXInput) / sizeof(PadButton);

// Xbox One/Series: Share is centered directly below the Guide button.
const PadButton padLayoutUsbXboxModern[] = {
  { GPAD_L2, 0, 0 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_L1, 0, (3 * 6) + 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R1, 0, (6 * 6) - 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R2, 0, 9 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_SELECT, 1, 3 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_HOME, 1, (4 * 6) + 1, PAD_WIDE_CIRCLE_LEFT_ON, PAD_WIDE_CIRCLE_LEFT_OFF },
  { GPAD_START, 1, 6 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_EXTRA0, 2, (4 * 6) + 4, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_L3, 3, 3 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_R3, 3, 6 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_Y, 1, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_X, 2, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 2, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 3, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_USB_XBOX_MODERN_COUNT =
    sizeof(padLayoutUsbXboxModern) / sizeof(PadButton);

// Amazon Luna: the wide Guide bitmap's visible center is two pixels right of
// its cursor origin, so offset the round Mic by two pixels to match it.
const PadButton padLayoutUsbLuna[] = {
  { GPAD_L2, 0, 0 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_L1, 0, (3 * 6) + 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R1, 0, (6 * 6) - 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R2, 0, 9 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_SELECT, 1, 3 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_HOME, 1, (4 * 6) + 1, PAD_WIDE_CIRCLE_LEFT_ON, PAD_WIDE_CIRCLE_LEFT_OFF },
  { GPAD_START, 1, 6 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_EXTRA0, 2, (4 * 6) + 3, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_L3, 3, 3 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_R3, 3, 6 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_Y, 1, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_X, 2, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 2, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 3, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_USB_LUNA_COUNT =
    sizeof(padLayoutUsbLuna) / sizeof(PadButton);

// Original Xbox Duke/Controller S: there is no Guide/Home button.
const PadButton padLayoutUsbXbox[] = {
  { GPAD_L2, 0, 0 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R2, 0, 9 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_UP, 1, 2 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 2 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 1 * 6, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 3 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_SELECT, 2, 0 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_START, 3, 1 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_L3, 1, 0 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_R3, 3, 5 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_Y, 1, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_X, 2, 6 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 2, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 3, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_L1, 3, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_R1, 2, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_USB_XBOX_COUNT =
    sizeof(padLayoutUsbXbox) / sizeof(PadButton);

const PadButton padLayoutUsbSwitch[] = {
  { GPAD_L2, 0, 0 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_L1, 0, (3 * 6) + 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R1, 0, (6 * 6) - 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R2, 0, 9 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  // Capture sits left of Home on the main row; Minus occupies the upper row.
  // Keep a one-character horizontal gap between the two system buttons.
  { GPAD_EXTRA0, 2, 4 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_SELECT, 1, 3 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_HOME, 2, 5 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_START, 1, 6 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_L3, 3, 3 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_R3, 3, 6 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  // Neutral face buttons retain their scancode identity. Draw them at the
  // corresponding physical Switch positions without relabeling the masks.
  { GPAD_Y, 1, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_X, 2, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 2, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 3, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_USB_SWITCH_COUNT =
    sizeof(padLayoutUsbSwitch) / sizeof(PadButton);

// Switch 2 Pro keeps the position-normalized neutral face buttons, but draws
// them at their physical locations. GL/GR sit below Capture/Home.
const PadButton padLayoutUsbSwitch2[] = {
  { GPAD_L2, 0, 0 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_L1, 0, (3 * 6) + 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R1, 0, (6 * 6) - 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R2, 0, 9 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_EXTRA0, 2, 4 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_SELECT, 1, 3 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_HOME, 2, 5 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_START, 1, 6 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_L3, 3, 3 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_R3, 3, 6 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_EXTRA2, 3, 4 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_EXTRA3, 3, 5 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_Y, 1, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_X, 2, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 2, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 3, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_USB_SWITCH2_COUNT =
    sizeof(padLayoutUsbSwitch2) / sizeof(PadButton);

const PadButton padLayoutSnes[] = {
  { GPAD_L1, 0, 1 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R1, 0, 8 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_SELECT, 2, 4 * 6, PAD_DASH_ON, PAD_DASH_OFF },
  { GPAD_START, 2, 5 * 6, PAD_DASH_ON, PAD_DASH_OFF },
  { GPAD_X, 1, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_Y, 2, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 2, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 3, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_SNES_COUNT = sizeof(padLayoutSnes) / sizeof(PadButton);

#ifdef ENABLE_INPUT_VBOY
const PadButton padLayoutVBoy[] = {
  { GPAD_L1, 0, 1 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R1, 0, 8 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_B, 3, (5 * 6) + 2, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 3, (6 * 6) + 2, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_SELECT, 3, (3 * 6) - 2, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_START, 3, (4 * 6) - 2, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_X, 1, 8 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_L2, 3, 8 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_Y, 2, 7 * 6, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_R2, 2, 9 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
};
const uint8_t PAD_LAYOUT_VBOY_COUNT = sizeof(padLayoutVBoy) / sizeof(PadButton);
#endif

const PadButton padLayoutN64[] = {
  { GPAD_L1, 0, (3 * 6) + 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R1, 0, (6 * 6) - 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_UP, 0, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 2, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 1, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 1, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_START, 2, 4 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_L2, 3, 4 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_B, 2, 6 * 6 - 2, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 3, 7 * 6 - 2, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_X, 0, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_R2, 2, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_Y, 1, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_SELECT, 1, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_N64_COUNT = sizeof(padLayoutN64) / sizeof(PadButton);

const PadButton padLayoutGC[] = {
  { GPAD_L2, 0, (1 * 6) + 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R2, 0, (8 * 6) - 3, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_SELECT, 1, 9 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_START, 2, (5 * 6) - 3, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_Y, 2, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 3, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 3, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_X, 3, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_GC_COUNT = sizeof(padLayoutGC) / sizeof(PadButton);

}  // namespace menu_pad_layouts_internal
