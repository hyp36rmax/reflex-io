#pragma once

// Application identity is separate from USB gamepad/MiSTer identity. These
// read-only records describe the compiled target, not an independently sensed
// board revision, and are not a cryptographic attestation.
#include "../product_config.h"
#include "../firmware_build_info.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <pico/unique_id.h>

#ifndef PRODUCT_IDENTITY_KEY
#error "Product must define PRODUCT_IDENTITY_KEY"
#endif
#ifndef PRODUCT_HARDWARE_TARGET
#error "Product must define PRODUCT_HARDWARE_TARGET"
#endif
#ifndef ADAPT_IDENTITY_BUILD
#define ADAPT_IDENTITY_BUILD "unknown"
#endif

namespace adapt_identity {
constexpr uint8_t schema = 2;
constexpr uint8_t report_identity = 0xE1;
constexpr uint8_t report_build = 0xE2;
constexpr size_t report_size = 63;
// Capability support, not current USB-mode availability or release approval.
enum Capability : uint16_t {
  Serial = 1, Hid = 2, Settings = 4, Bootloader = 8,
  // MscOptIn: an opt-in USB drive (Classic2USB since 2026-10-03; every
  // product before 2026-09-26).
  Maintenance = 16, MscOptIn = 32,
};
constexpr uint16_t capabilities = 0
#if defined(ADAPT_FEATURE_USB_DRIVE)
    | MscOptIn
#endif
#if defined(ENABLE_RUNTIME_SERIAL_DEBUG_CDC)
    | Serial | Settings | Bootloader
#endif
#if defined(ADAPT_OUTPUT_USB_DEVICE)
    | Hid | Settings | Bootloader
#endif
#if defined(ENABLE_USB2DB15_SETTINGS_BENCH)
    | Serial | Maintenance
#endif
    ;
static_assert(sizeof(PRODUCT_IDENTITY_KEY) <= 16, "Identity product too long");
static_assert(sizeof(PRODUCT_HARDWARE_TARGET) <= 24, "Identity target too long");
static_assert(sizeof(FIRMWARE_VERSION_STRING) <= 24, "Identity version too long");
static_assert(sizeof(ADAPT_IDENTITY_BUILD) <= 29, "Identity build too long");

inline void uid(uint8_t* destination) {
  static_assert(PICO_UNIQUE_BOARD_ID_SIZE_BYTES == 8, "Identity UID wire size");
  pico_unique_board_id_t id{};
  pico_get_unique_board_id(&id);
  memcpy(destination, id.id, 8);
}

inline void uidHex(char (&destination)[17]) {
  uint8_t bytes[8]; uid(bytes);
  for (size_t i = 0; i < 8; ++i) snprintf(destination + i * 2, 3, "%02X", bytes[i]);
}

inline uint16_t report(uint8_t id, uint8_t* out) {
  memset(out, 0, report_size);
  out[0] = 0xAD; out[1] = schema;
  if (id == report_identity) {
    out[2] = capabilities & 255; out[3] = capabilities >> 8;
    uid(out + 4);
    memcpy(out + 12, PRODUCT_IDENTITY_KEY, sizeof(PRODUCT_IDENTITY_KEY));
    memcpy(out + 28, PRODUCT_HARDWARE_TARGET, sizeof(PRODUCT_HARDWARE_TARGET));
    out[52] = 1; // RP2040
    out[53] = ADAPT_PRODUCT_USB_VID & 255; out[54] = ADAPT_PRODUCT_USB_VID >> 8;
    out[55] = ADAPT_PRODUCT_USB_PID & 255; out[56] = ADAPT_PRODUCT_USB_PID >> 8;
  } else if (id == report_build) {
    uid(out + 2);
    memcpy(out + 10, FIRMWARE_VERSION_STRING, sizeof(FIRMWARE_VERSION_STRING));
    memcpy(out + 34, ADAPT_IDENTITY_BUILD, sizeof(ADAPT_IDENTITY_BUILD));
  } else return 0;
  return report_size;
}

template<class Stream> inline void write(Stream& stream) {
  char unit[17]; uidHex(unit);
  // IDENTITY remains schema 1 for existing deployment clients. IDENTITY2 is
  // additive, and supplies all fields from the same implementation as HID.
  stream.print("IDENTITY SCHEMA=2 PRODUCT=" PRODUCT_IDENTITY_KEY);
  stream.print(" TARGET=" PRODUCT_HARDWARE_TARGET " MCU=RP2040 UID=");
  stream.print(unit);
  char fields[64];
  snprintf(fields, sizeof(fields), " BASE_VID=%04X BASE_PID=%04X CAPS=%04X",
           ADAPT_PRODUCT_USB_VID, ADAPT_PRODUCT_USB_PID, capabilities);
  stream.print(fields);
  stream.println(" VERSION=" FIRMWARE_VERSION_STRING " BUILD=" ADAPT_IDENTITY_BUILD);
}
} // namespace adapt_identity
