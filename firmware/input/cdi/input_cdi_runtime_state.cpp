#include "input_cdi_runtime_state.h"

namespace {
constexpr uint8_t kCdiPorts = 2;
}

cdi_protocol::Port cdi_ports[kCdiPorts] = {};
cdi_protocol::Output cdi_last_output[kCdiPorts] = {};
bool cdi_receiver_failed[kCdiPorts] = {false, false};
