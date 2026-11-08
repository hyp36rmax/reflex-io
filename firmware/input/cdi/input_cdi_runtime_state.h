#pragma once

#include "cdi_protocol.h"

extern cdi_protocol::Port cdi_ports[];
extern cdi_protocol::Output cdi_last_output[];
// The PIO receiver could not be started on that port.
extern bool cdi_receiver_failed[];
