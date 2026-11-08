#pragma once

#include <stdint.h>

#include "../core/controller_state.h"
#include "../core/device_mode.h"

bool homeAnalogGaugeShouldShow(DeviceEnum mode, uint8_t player,
                               bool centeredSinglePad);
uint8_t homeAnalogGaugePadColumnOffset();
uint8_t homeAnalogGaugePadColumnOffset(DeviceEnum mode, const controller_state_t& frame);
void homeAnalogGaugeRender(DeviceEnum mode, const controller_state_t& frame,
                           bool forceRedraw);
void homeAnalogGaugeReset();
