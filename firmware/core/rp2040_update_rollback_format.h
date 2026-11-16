#pragma once

#include <stdint.h>

// Shared, on-flash contract between the MODERN2USB application and its
// project-owned RP2040 stage3. Keep this header valid C as well as C++.
#define MODERN2USB_RP2040_ROLLBACK_RECORD_MAGIC 0x4B425252u
#define MODERN2USB_RP2040_ROLLBACK_RECORD_SCHEMA 2u

#define MODERN2USB_RP2040_ROLLBACK_STATE_NONE 0u
#define MODERN2USB_RP2040_ROLLBACK_STATE_PENDING 1u
#define MODERN2USB_RP2040_ROLLBACK_STATE_INSTALLED 2u
#define MODERN2USB_RP2040_ROLLBACK_STATE_BOOTED 3u

// A successful automatic restore writes a normal NONE tombstone with this
// notice in the otherwise-unused reserved field. Stage3 treats the candidate
// as retired immediately; the restored application keeps the notice until the
// user acknowledges it on the OLED or through management serial. Older
// applications safely ignore the reserved field.
#define MODERN2USB_RP2040_ROLLBACK_NOTICE_RECOVERED 0x5242u

// A direct BOOTSEL UF2 appends this marker to both metadata sectors after all
// primary-image blocks. Stage3 treats either valid copy as an explicit
// physical-recovery override; the recovered application removes stale OTA
// files before it retires both copies.
#define MODERN2USB_RP2040_RECOVERY_OVERRIDE_MAGIC 0x56524352u
#define MODERN2USB_RP2040_RECOVERY_OVERRIDE_SCHEMA 1u
#define MODERN2USB_RP2040_RECOVERY_OVERRIDE_MARKER 0xA5u
#define MODERN2USB_RP2040_RECOVERY_PRODUCT_MODERN2USB 1u
#define MODERN2USB_RP2040_RECOVERY_MCU_RP2040 1u
#define MODERN2USB_RP2040_RECOVERY_BOOT_LAYOUT_STAGE3_V2 2u

#define MODERN2USB_RP2040_PRIMARY_SLOT_OFFSET 0x000000u
#define MODERN2USB_RP2040_PRIMARY_SLOT_SIZE (640u * 1024u)
#define MODERN2USB_RP2040_BACKUP_SLOT_OFFSET 0x0A0000u
#define MODERN2USB_RP2040_BACKUP_SLOT_SIZE MODERN2USB_RP2040_PRIMARY_SLOT_SIZE
#define MODERN2USB_RP2040_ROLLBACK_META_A_OFFSET 0x140000u
#define MODERN2USB_RP2040_ROLLBACK_META_B_OFFSET 0x141000u
#define MODERN2USB_RP2040_RECOVERY_META_A_OFFSET 0x142000u
#define MODERN2USB_RP2040_RECOVERY_META_B_OFFSET 0x143000u
// The settings journal's data and control sectors (core/settings_journal.h).
#define MODERN2USB_RP2040_SETTINGS_JOURNAL_DATA_OFFSET 0x144000u
#define MODERN2USB_RP2040_SETTINGS_JOURNAL_CONTROL_OFFSET 0x145000u
// The Linux SDL controller table (input_usb/sdl_gamecontrollerdb_table_format.h).
// Only a BOOTSEL UF2 writes it, after the recovery tail; ESP32-fed updates and
// stage3 never touch it. 0x14E000-0x154000 stays unused.
#define MODERN2USB_RP2040_SDL_TABLE_OFFSET 0x146000u
#define MODERN2USB_RP2040_SDL_TABLE_SIZE (32u * 1024u)
#define MODERN2USB_RP2040_FILESYSTEM_OFFSET 0x154000u

typedef struct __attribute__((packed, aligned(1))) {
  uint32_t magic;
  uint8_t schema;
  uint8_t state;
  uint16_t reserved;
  uint32_t generation;
  uint32_t candidate_size;
  uint32_t candidate_crc32;
  uint32_t backup_crc32;
  uint32_t record_crc32;
} Modern2UsbRp2040RollbackRecord;

typedef struct __attribute__((packed, aligned(1))) {
  uint32_t magic;
  uint8_t schema;
  uint8_t marker;
  uint8_t product;
  uint8_t mcu;
  uint8_t boot_layout;
  uint8_t reserved[3];
  uint32_t primary_size;
  uint32_t primary_crc32;
  uint32_t record_crc32;
} Modern2UsbRp2040RecoveryOverride;

#if defined(__cplusplus)
static_assert(sizeof(Modern2UsbRp2040RollbackRecord) == 28,
              "Unexpected RP2040 rollback record size");
static_assert(sizeof(Modern2UsbRp2040RecoveryOverride) == 24,
              "Unexpected RP2040 recovery override size");
#else
_Static_assert(sizeof(Modern2UsbRp2040RollbackRecord) == 28,
               "Unexpected RP2040 rollback record size");
_Static_assert(sizeof(Modern2UsbRp2040RecoveryOverride) == 24,
               "Unexpected RP2040 recovery override size");
#endif
