#pragma once

// Output trial for remote testing; debug images only (see output_trial_record.h).
// Production images exclude this folder from the build, and every hook in
// shared code sits behind ADAPT_FEATURE_OUTPUT_TRIAL.

#include "../../product_config.h"

#if defined(ADAPT_FEATURE_OUTPUT_TRIAL)
#include <stdint.h>

#include "../feature_module.h"
#include "../../output/output_mode.h"

// Boot: the output to configure this boot, given the saved one.
outputMode_t outputTrialBootOutput(outputMode_t savedOutput);
// Settings saves: the output value to write in place of toWrite.
uint8_t outputTrialSavedOutputToWrite(uint8_t toWrite);
// Ends a running trial without restarting (OUTPUT SET saves its own choice).
void outputTrialEnd();
void outputTrialService();
// Keeps the record through reboot(), like the capture images' stores.
void outputTrialPreserveSram();

extern const FeatureModule kOutputTrialFeatureModule;
#endif
