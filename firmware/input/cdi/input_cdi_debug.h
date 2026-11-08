#pragma once

#include <Arduino.h>

// Serial CDISTAT: per-port identification phase, device class, baud rate,
// RTS and data-line levels and packet/error counters.
void printCdiStatus(Print& out);
