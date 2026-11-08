#include "input_colecovision_runtime_state.h"

namespace {
constexpr uint8_t kColecoVisionPorts = 2;
}

uint16_t colecovision_last_state[kColecoVisionPorts] = { 0 };
uint16_t colecovision_pending_state[kColecoVisionPorts] = { 0 };
