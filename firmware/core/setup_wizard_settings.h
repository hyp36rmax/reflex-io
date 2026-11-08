#pragma once

#include <stdint.h>

// Global byte 19 stores both the logical OK/Back layout and first-boot setup
// state. Values written before setup existed remain completed installations.
constexpr uint8_t MENU_OK_BACK_LEGACY = 0;
constexpr uint8_t MENU_OK_BACK_EAST = 1;
constexpr uint8_t MENU_OK_BACK_SOUTH = 2;
constexpr uint8_t MENU_OK_BACK_STORED_EAST = 0xA1;
constexpr uint8_t MENU_OK_BACK_STORED_SOUTH = 0xA2;
constexpr uint8_t FIRST_BOOT_SETUP_PENDING_TAG = 0xB0;
constexpr uint8_t FIRST_BOOT_SETUP_COMPLETE_TAG = 0xC0;
constexpr uint8_t FIRST_BOOT_SETUP_VALUE_MASK = 0x03;

inline constexpr bool firstBootSetupPendingFromRaw(uint8_t raw) {
  return raw >= FIRST_BOOT_SETUP_PENDING_TAG &&
         raw <= FIRST_BOOT_SETUP_PENDING_TAG + MENU_OK_BACK_SOUTH;
}

inline constexpr bool firstBootSetupCompleteTagFromRaw(uint8_t raw) {
  return raw >= FIRST_BOOT_SETUP_COMPLETE_TAG &&
         raw <= FIRST_BOOT_SETUP_COMPLETE_TAG + MENU_OK_BACK_SOUTH;
}

inline constexpr uint8_t menuOkBackValueFromRaw(uint8_t raw) {
  if (raw == MENU_OK_BACK_STORED_EAST) return MENU_OK_BACK_EAST;
  if (raw == MENU_OK_BACK_STORED_SOUTH) return MENU_OK_BACK_SOUTH;
  if (firstBootSetupPendingFromRaw(raw) || firstBootSetupCompleteTagFromRaw(raw)) {
    return raw & FIRST_BOOT_SETUP_VALUE_MASK;
  }
  return MENU_OK_BACK_LEGACY;
}

inline constexpr uint8_t encodeMenuOkBackPreservingSetupState(
    uint8_t raw, uint8_t value, bool setupSupported) {
  const uint8_t logical = value <= MENU_OK_BACK_SOUTH
      ? value : MENU_OK_BACK_LEGACY;
  if (!setupSupported) {
    // Products without first-boot setup keep the byte's earlier encoding, so
    // their stored records do not change.
    return logical == MENU_OK_BACK_EAST ? MENU_OK_BACK_STORED_EAST :
           logical == MENU_OK_BACK_SOUTH ? MENU_OK_BACK_STORED_SOUTH :
           MENU_OK_BACK_LEGACY;
  }
  if (firstBootSetupPendingFromRaw(raw)) {
    return FIRST_BOOT_SETUP_PENDING_TAG + logical;
  }
  // Explicit complete tags stay complete. Valid legacy/unmarked records are
  // also complete and migrate when some later settings write publishes them.
  return FIRST_BOOT_SETUP_COMPLETE_TAG + logical;
}

inline constexpr uint8_t markFirstBootSetupPending(uint8_t raw) {
  return FIRST_BOOT_SETUP_PENDING_TAG + menuOkBackValueFromRaw(raw);
}

inline constexpr uint8_t markFirstBootSetupComplete(uint8_t raw) {
  return FIRST_BOOT_SETUP_COMPLETE_TAG + menuOkBackValueFromRaw(raw);
}
