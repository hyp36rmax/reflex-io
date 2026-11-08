#pragma once

#include <stdint.h>

// PIO receivers for the CD-i data lines, one state machine per port. The
// program (cdi_uart_rx.pio) is loaded once per PIO block that hosts a port.
// Nothing is claimed until start(); stop() releases the state machine, the
// program and the pin's input inversion.
namespace cdi_receiver {

bool start(uint8_t port, uint8_t pin, uint32_t baud);
// Clears the FIFO and the framing flag and restarts at a new baud rate.
void restart(uint8_t port, uint32_t baud);
void stop(uint8_t port);
bool running(uint8_t port);
// Next received 7-bit character, or -1. Characters with a bad stop bit are
// not delivered; they set the framing flag.
int read(uint8_t port);
// True once per framing error or break since the previous call.
bool takeFramingError(uint8_t port);
// The pad level before the input inversion: low is mark.
bool lineAtMark(uint8_t pin);

}  // namespace cdi_receiver
