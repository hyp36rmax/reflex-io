#pragma once

#include "rp2040_update_rollback_format.h"

// The signed MODERN2USB artifact is capped at 640 KiB even though Arduino-Pico's
// linker region extends to LittleFS. The otherwise-unmapped space is reserved
// for one exact primary-slot backup and two independent metadata sectors.
static_assert(
    MODERN2USB_RP2040_PRIMARY_SLOT_OFFSET +
            MODERN2USB_RP2040_PRIMARY_SLOT_SIZE <=
        MODERN2USB_RP2040_BACKUP_SLOT_OFFSET,
    "RP2040 primary and rollback slots overlap");
static_assert(
    MODERN2USB_RP2040_BACKUP_SLOT_OFFSET +
            MODERN2USB_RP2040_BACKUP_SLOT_SIZE <=
        MODERN2USB_RP2040_ROLLBACK_META_A_OFFSET,
    "RP2040 rollback image overlaps metadata");
static_assert(
    MODERN2USB_RP2040_ROLLBACK_META_B_OFFSET + 0x1000u <=
        MODERN2USB_RP2040_RECOVERY_META_A_OFFSET,
    "RP2040 rollback metadata overlaps recovery metadata");
static_assert(
    MODERN2USB_RP2040_RECOVERY_META_B_OFFSET + 0x1000u <=
        MODERN2USB_RP2040_FILESYSTEM_OFFSET,
    "RP2040 recovery metadata overlaps LittleFS");
static_assert(
    MODERN2USB_RP2040_RECOVERY_META_B_OFFSET + 0x1000u <=
            MODERN2USB_RP2040_SETTINGS_JOURNAL_DATA_OFFSET &&
        MODERN2USB_RP2040_SETTINGS_JOURNAL_DATA_OFFSET + 0x1000u <=
            MODERN2USB_RP2040_SETTINGS_JOURNAL_CONTROL_OFFSET &&
        MODERN2USB_RP2040_SETTINGS_JOURNAL_CONTROL_OFFSET + 0x1000u <=
            MODERN2USB_RP2040_FILESYSTEM_OFFSET,
    "RP2040 settings journal overlaps recovery metadata or LittleFS");
static_assert(
    MODERN2USB_RP2040_SETTINGS_JOURNAL_CONTROL_OFFSET + 0x1000u <=
            MODERN2USB_RP2040_SDL_TABLE_OFFSET &&
        MODERN2USB_RP2040_SDL_TABLE_OFFSET % 0x1000u == 0 &&
        MODERN2USB_RP2040_SDL_TABLE_OFFSET +
                MODERN2USB_RP2040_SDL_TABLE_SIZE <=
            MODERN2USB_RP2040_FILESYSTEM_OFFSET,
    "RP2040 SDL controller table overlaps the settings journal or LittleFS");
