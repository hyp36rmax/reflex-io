#pragma once
#include "menu_pad_layouts_internal.h"
namespace fishing_pad_art {
// DualShock button positions with its D-pad replaced by the real stick.
// The reel gauge sits to the stick's left, below the shoulder-button row.
// Fishing has no L3/R3, so only its ten physical buttons are shown.
inline const PadButton layout[] = {
 {GPAD_L2,0,0,PAD_SHOULDER_ON,PAD_SHOULDER_OFF},
 {GPAD_L1,0,15,PAD_SHOULDER_ON,PAD_SHOULDER_OFF},
 {GPAD_R1,0,39,PAD_SHOULDER_ON,PAD_SHOULDER_OFF},
 {GPAD_R2,0,54,PAD_SHOULDER_ON,PAD_SHOULDER_OFF},
 {GPAD_SELECT,2,24,PAD_RECT_ON,PAD_RECT_OFF},
 {GPAD_START,2,33,PAD_PS_TRIANGLE_ON,PAD_PS_TRIANGLE_OFF},
 {GPAD_Y,1,48,PAD_FACE_ON,PAD_FACE_OFF},
 {GPAD_X,2,42,PAD_FACE_ON,PAD_FACE_OFF},
 {GPAD_B,2,54,PAD_FACE_ON,PAD_FACE_OFF},
 {GPAD_A,3,48,PAD_FACE_ON,PAD_FACE_OFF},
};
}
