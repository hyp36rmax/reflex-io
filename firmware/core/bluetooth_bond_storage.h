#pragma once

#include <stdint.h>

static constexpr uint8_t BLUETOOTH_BOND_SLOT_COUNT = 8;
static constexpr uint8_t BLUETOOTH_BOND_FLAG_VALID = 0x01;
static constexpr uint8_t BLUETOOTH_BOND_FLAG_LE = 0x02;
static constexpr uint8_t BLUETOOTH_BOND_SLOT_NONE = 0xFF;
static constexpr uint8_t BLUETOOTH_BOND_KEY_TYPE_NONE = 0xFF;
static constexpr uint8_t BLUETOOTH_CONTROLLER_TYPE_UNKNOWN = 0;
static constexpr uint8_t BLUETOOTH_CONTROLLER_TYPE_DUALSHOCK3 = 1;
static constexpr uint8_t BLUETOOTH_CONTROLLER_TYPE_DUALSHOCK4 = 2;
static constexpr uint8_t BLUETOOTH_CONTROLLER_TYPE_DUALSENSE = 3;
static constexpr uint8_t BLUETOOTH_CONTROLLER_TYPE_SWITCH_PRO = 4;
static constexpr uint8_t BLUETOOTH_CONTROLLER_TYPE_XBOX = 5;
static constexpr uint8_t BLUETOOTH_CONTROLLER_TYPE_WII = 6;
static constexpr uint8_t BLUETOOTH_CONTROLLER_TYPE_WIIU_PRO = 7;
static constexpr uint8_t BLUETOOTH_CONTROLLER_TYPE_GENERIC = 8;
static constexpr uint8_t BLUETOOTH_CONTROLLER_TYPE_8BITDO = 9;
static constexpr uint8_t BLUETOOTH_CONTROLLER_TYPE_MAX_KNOWN = BLUETOOTH_CONTROLLER_TYPE_8BITDO;

struct BluetoothBondSlot {
  uint8_t addr[6];
  uint8_t link_key[16];
  uint8_t key_type;
  uint8_t class_of_device[3];
  uint8_t controller_type;
  uint8_t order;
  uint8_t flags;
  uint8_t reserved;
};

static_assert(sizeof(BluetoothBondSlot) == 30, "BluetoothBondSlot size changed unexpectedly");

struct BluetoothBondListRecord {
  BluetoothBondSlot slots[BLUETOOTH_BOND_SLOT_COUNT];
};

static_assert(sizeof(BluetoothBondListRecord) <= 255, "BluetoothBondListRecord must fit persisted header size");

// Historical chained end: positions the stores that follow the bond records.
uint16_t bluetoothBondStorageRequiredEnd();
// End of the EEPROM bytes bond storage actually uses inside the 4 KiB window.
// Raw-flash builds only touch the legacy EEPROM records that fit the window.
uint16_t bluetoothBondEepromOccupiedEnd();
bool bluetoothBondReadList(BluetoothBondListRecord& bonds);
bool bluetoothBondWriteList(const BluetoothBondListRecord& bonds);
bool bluetoothBondUpsert(const BluetoothBondSlot& bond, uint8_t* storedSlot);
bool bluetoothBondFindByAddress(const uint8_t addr[6], BluetoothBondSlot& bond, uint8_t* slotIndex);
uint8_t bluetoothBondValidCount(const BluetoothBondListRecord& bonds);
// Publishes an empty migration tombstone and removes every legacy EEPROM
// representation. False means durability could not be verified and callers
// must not claim a completed reset/reboot.
bool bluetoothBondClear();
bool bluetoothBondUpdaterStorageReady();
