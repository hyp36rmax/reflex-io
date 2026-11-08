#pragma once

#include "buzzer.h"

uint8_t buzzerAuditionCandidateCount();
const char* buzzerAuditionCandidateName(uint8_t index);
const buzzer_melody_t* buzzerAuditionCandidateMelody(uint8_t index);
