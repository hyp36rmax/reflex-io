#pragma once

#include <stdint.h>

#include "controller_state.h"

// Player currently targeted by controller-specific menus and feedback tests.
// This is intentionally runtime-only: connection order owns P1-P4 and the
// selected menu player must never rewrite slot assignment.
uint8_t selectedControllerPlayer();
void setSelectedControllerPlayer(uint8_t player);
void cycleSelectedControllerPlayer(bool forward);
const controller_state_t& selectedControllerFrame();
