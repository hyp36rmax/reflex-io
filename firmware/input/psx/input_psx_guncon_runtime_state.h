#pragma once

#include <Arduino.h>

extern uint8_t calibrationStep;
extern bool enableReport;
extern bool enableJoystick;
extern bool joyOffScreenEdge;
extern int8_t offsetX;
extern int8_t offsetY;
extern uint8_t noLightCount;
extern uint16_t lastX;
extern uint16_t lastY;
// Mode-change cooldown: GunCon input is ignored until this millis() value.
extern uint32_t gunconHoldoffStartMs;
extern uint16_t gunconHoldoffMs;
