#pragma once

#include <stddef.h>
#include <stdint.h>

#if !defined(REFLEX_HOST_TEST_SETTINGS_JOURNAL)
#include "../product_config.h"
#endif

// Host builds define ARDUINO_ARCH_RP2040 too, with a fake EEPROM and no flash.
#if defined(ADAPT_CAP_SETTINGS_JOURNAL) && defined(ARDUINO_ARCH_RP2040) && \
    defined(__arm__)
#define ADAPT_SETTINGS_JOURNAL_ACTIVE 1
#endif

// Rewriting a flash sector erases it first, so losing power during a save
// leaves that sector blank or half written, and one sector holds every
// setting. On products with ADAPT_CAP_SETTINGS_JOURNAL a save instead copies
// the new image to a journal sector, seals the copy with a control record,
// rewrites the real sector, then erases the record. A record found at boot
// means the rewrite may not have finished, so settingsJournalRecover() does it
// again from the sealed copy. Power lost before the seal leaves the real sector
// untouched, so a save either happens whole or not at all.
//
// A record older firmware left unfinished (a power cut, then a downgrade before
// this firmware boots) would still restore on a later upgrade. That needs both
// to happen together, so it is accepted.

enum class SettingsJournalTarget : uint32_t {
  kEeprom = 1,
  kSharedProfiles = 2,
  // The fixed-end layout (MODERN2USB) also journals its learned HID mapping
  // sector and the sector that holds its analog profiles.
  kHidMappings = 3,
  kAuthSector = 4,
};

#if defined(REFLEX_HOST_TEST_SETTINGS_JOURNAL)
// Supplied by host_tests/settings_journal_tests.cpp in place of real flash.
const uint8_t* settingsJournalHostTargetBytes(SettingsJournalTarget target);
const uint8_t* settingsJournalHostDataBytes();
const uint8_t* settingsJournalHostControlBytes();
void settingsJournalHostErase(const uint8_t* sector);
void settingsJournalHostProgram(const uint8_t* address, const uint8_t* data, size_t length);
void settingsJournalHostWindowClosed();
#endif

#if defined(REFLEX_HOST_TEST_SETTINGS_JOURNAL) || defined(ADAPT_SETTINGS_JOURNAL_ACTIVE)
// Finishes a save that power loss cut short. Runs before anything reads the
// protected sectors, and before EEPROM.begin() copies one into RAM.
void settingsJournalRecover();
// length must be a whole number of 256-byte flash pages. True once the sector
// holds the image; writes nothing if it already does.
bool settingsJournalWriteSector(SettingsJournalTarget target, const uint8_t* image,
                                size_t length);
#if !defined(REFLEX_HOST_TEST_SETTINGS_JOURNAL)
bool settingsJournalCommitEeprom();
#endif
#else
inline void settingsJournalRecover() {}
#endif
