#pragma once

#include <stdint.h>

// Coordinates every user-facing RP2040 configuration store which may erase
// or program flash. Auth, updater, and rollback state deliberately remain on
// their own immediate durability paths.
void configurationPersistenceService();
bool configurationPersistenceFlushNow();
bool configurationPersistenceFlushBounded(uint8_t attempts,
                                          uint16_t retryDelayMs);
// Production reset gate and host-test seam. A reset may proceed only after
// every accepted user configuration write has been durably verified.
bool configurationPersistencePrepareForReset();
void configurationPersistenceDiscardDeferred();
void configurationPersistenceBeginImmediate();
void configurationPersistenceEndImmediate();
bool configurationPersistenceHasPending();
