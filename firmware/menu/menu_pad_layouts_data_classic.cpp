#include "../product_config.h"

#include "menu_pad_layouts_internal.h"

namespace menu_pad_layouts_internal {

const PadButton padLayoutNES[] = {
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_SELECT, 2, 4 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_START, 2, 5 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_B, 2, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 2, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_NES_COUNT = sizeof(padLayoutNES) / sizeof(PadButton);

const PadButton padLayoutPowerPad[] = {
  { GPAD_A, 0, 1 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 0, 3 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_X, 0, 5 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_Y, 0, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_L1, 1, 1 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_R1, 1, 3 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_L2, 1, 5 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_R2, 1, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_SELECT, 2, 1 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_START, 2, 3 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_L3, 2, 5 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_R3, 2, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_POWERPAD_COUNT = sizeof(padLayoutPowerPad) / sizeof(PadButton);

const PadButton padLayoutGeneric[] = {
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_SELECT, 2, 4 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_START, 2, 5 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_B, 3, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 3, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_GENERIC_COUNT = sizeof(padLayoutGeneric) / sizeof(PadButton);

#ifdef ENABLE_INPUT_PCE
const PadButton padLayoutPCE[] = {
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_SELECT, 2, 4 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_START, 2, 5 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_B, 3, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 3, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_R1, 3, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_L1, 2, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_X, 2, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_Y, 2, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_PCE_COUNT = sizeof(padLayoutPCE) / sizeof(PadButton);

const PadButton padLayoutPCE2[] = {
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_SELECT, 2, 4 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_START, 2, 5 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_B, 3, 9 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 3, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_PCE2_COUNT = sizeof(padLayoutPCE2) / sizeof(PadButton);
#endif

#ifdef ENABLE_INPUT_NEOGEO
const PadButton padLayoutNeoGeo[] = {
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_SELECT, 2, 4 * 6, PAD_DASH_ON, PAD_DASH_OFF },
  { GPAD_START, 3, 4 * 6, PAD_DASH_ON, PAD_DASH_OFF },
  { GPAD_A, 3, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 2, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_X, 2, 6 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_Y, 1, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_NEOGEO_COUNT = sizeof(padLayoutNeoGeo) / sizeof(PadButton);
#endif

#ifdef ENABLE_INPUT_3DO
const PadButton padLayout3DO[] = {
  { GPAD_L1, 0, 1 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_R1, 0, 7 * 6, PAD_SHOULDER_ON, PAD_SHOULDER_OFF },
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_SELECT, 2, (4 * 6) - 3, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_START, 2, (5 * 6) - 3, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 3, 6 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 2, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_X, 1, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_3DO_COUNT = sizeof(padLayout3DO) / sizeof(PadButton);
#endif

#ifdef ENABLE_INPUT_JAGUAR
const PadButton padLayoutJaguar[] = {
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_START, 0, 3 * 6, PAD_DASH_ON, PAD_DASH_OFF },
  { GPAD_SELECT, 0, 5 * 6, PAD_DASH_ON, PAD_DASH_OFF },
  { GPAD_L3, 1, 3 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_EXTRA1, 1, 4 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_R3, 1, 5 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_L2, 2, 3 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_EXTRA4, 2, 4 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_R2, 2, 5 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_L1, 3, 3 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_Y, 3, 4 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_R1, 3, 5 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_EXTRA2, 4, 3 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_EXTRA0, 4, 4 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_EXTRA3, 4, 5 * 6, PAD_RECT_ON, PAD_RECT_OFF },
  { GPAD_X, 3, 6 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 2, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 1, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_JAGUAR_COUNT = sizeof(padLayoutJaguar) / sizeof(PadButton);

const PadButton padLayoutJaguarRotary[] = {
  { GPAD_START, 2, 0, PAD_DASH_ON, PAD_DASH_OFF },
  { GPAD_SELECT, 2, 2 * 6, PAD_DASH_ON, PAD_DASH_OFF },
  { GPAD_X, 2, 5 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_A, 2, 6 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 2, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_JAGUAR_ROTARY_COUNT = sizeof(padLayoutJaguarRotary) / sizeof(PadButton);
#endif

#ifdef ENABLE_INPUT_INTV
// Keypad grid as on the controller; the disc is the analog gauge.
const PadButton padLayoutIntv[] = {
  { GPAD_A,       1, 18, PAD_FACE_ON, PAD_FACE_OFF }, // top side buttons
  { GPAD_B,       2, 18, PAD_FACE_ON, PAD_FACE_OFF }, // lower left
  { GPAD_X,       2, 48, PAD_FACE_ON, PAD_FACE_OFF }, // lower right
  { GPAD_L1,      0, 27, PAD_RECT_ON, PAD_RECT_OFF }, // 1
  { GPAD_UP,      0, 33, PAD_RECT_ON, PAD_RECT_OFF }, // 2
  { GPAD_R1,      0, 39, PAD_RECT_ON, PAD_RECT_OFF }, // 3
  { GPAD_LEFT,    1, 27, PAD_RECT_ON, PAD_RECT_OFF }, // 4
  { GPAD_L3,      1, 33, PAD_RECT_ON, PAD_RECT_OFF }, // 5
  { GPAD_RIGHT,   1, 39, PAD_RECT_ON, PAD_RECT_OFF }, // 6
  { GPAD_L2,      2, 27, PAD_RECT_ON, PAD_RECT_OFF }, // 7
  { GPAD_DOWN,    2, 33, PAD_RECT_ON, PAD_RECT_OFF }, // 8
  { GPAD_R2,      2, 39, PAD_RECT_ON, PAD_RECT_OFF }, // 9
  { GPAD_SELECT,  3, 27, PAD_RECT_ON, PAD_RECT_OFF }, // Clear
  { GPAD_R3,      3, 33, PAD_RECT_ON, PAD_RECT_OFF }, // 0
  { GPAD_START,   3, 39, PAD_RECT_ON, PAD_RECT_OFF }, // Enter
};
const uint8_t PAD_LAYOUT_INTV_COUNT = sizeof(padLayoutIntv) / sizeof(PadButton);
#endif

#ifdef ENABLE_INPUT_COLECOVISION
// Keypad grid as on the controller; the joystick is the analog gauge.
const PadButton padLayoutColecoVision[] = {
  { GPAD_A,       1, 18, PAD_FACE_ON, PAD_FACE_OFF }, // left fire
  { GPAD_B,       2, 18, PAD_FACE_ON, PAD_FACE_OFF }, // right fire
  { GPAD_X,       1, 48, PAD_FACE_ON, PAD_FACE_OFF }, // Super Action 3
  { GPAD_Y,       2, 48, PAD_FACE_ON, PAD_FACE_OFF }, // Super Action 4
  { GPAD_L1,      0, 27, PAD_RECT_ON, PAD_RECT_OFF }, // 1
  { GPAD_UP,      0, 33, PAD_RECT_ON, PAD_RECT_OFF }, // 2
  { GPAD_R1,      0, 39, PAD_RECT_ON, PAD_RECT_OFF }, // 3
  { GPAD_LEFT,    1, 27, PAD_RECT_ON, PAD_RECT_OFF }, // 4
  { GPAD_L3,      1, 33, PAD_RECT_ON, PAD_RECT_OFF }, // 5
  { GPAD_RIGHT,   1, 39, PAD_RECT_ON, PAD_RECT_OFF }, // 6
  { GPAD_L2,      2, 27, PAD_RECT_ON, PAD_RECT_OFF }, // 7
  { GPAD_DOWN,    2, 33, PAD_RECT_ON, PAD_RECT_OFF }, // 8
  { GPAD_R2,      2, 39, PAD_RECT_ON, PAD_RECT_OFF }, // 9
  { GPAD_SELECT,  3, 27, PAD_RECT_ON, PAD_RECT_OFF }, // *
  { GPAD_R3,      3, 33, PAD_RECT_ON, PAD_RECT_OFF }, // 0
  { GPAD_START,   3, 39, PAD_RECT_ON, PAD_RECT_OFF }, // #
};
const uint8_t PAD_LAYOUT_COLECOVISION_COUNT = sizeof(padLayoutColecoVision) / sizeof(PadButton);
#endif

#ifdef ENABLE_INPUT_CDI
// Direction from pointer motion; buttons 1 and 2. The stick is the gauge.
const PadButton padLayoutCdi[] = {
  { GPAD_UP, 1, 1 * 6, PAD_UP_ON, PAD_UP_OFF },
  { GPAD_DOWN, 3, 1 * 6, PAD_DOWN_ON, PAD_DOWN_OFF },
  { GPAD_LEFT, 2, 0, PAD_LEFT_ON, PAD_LEFT_OFF },
  { GPAD_RIGHT, 2, 2 * 6, PAD_RIGHT_ON, PAD_RIGHT_OFF },
  { GPAD_A, 3, 7 * 6, PAD_FACE_ON, PAD_FACE_OFF },
  { GPAD_B, 3, 8 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_CDI_COUNT = sizeof(padLayoutCdi) / sizeof(PadButton);
#endif

#ifdef ENABLE_INPUT_DRIVING
const PadButton padLayoutDriving[] = {
  { GPAD_A, 2, 4 * 6, PAD_FACE_ON, PAD_FACE_OFF },
};
const uint8_t PAD_LAYOUT_DRIVING_COUNT = sizeof(padLayoutDriving) / sizeof(PadButton);
#endif

}  // namespace menu_pad_layouts_internal
