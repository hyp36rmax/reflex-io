#pragma once

#include <stdint.h>

constexpr uint8_t MODERN2USB_HOTKEY_PROFILE_COUNT = 32;
constexpr uint8_t MODERN2USB_HOTKEY_ACTION_COUNT = 3;
enum Modern2UsbHotkeyTransport : uint8_t { MODERN2USB_HOTKEY_TRANSPORT_NONE = 0, MODERN2USB_HOTKEY_TRANSPORT_USB = 1, MODERN2USB_HOTKEY_TRANSPORT_BLUETOOTH = 2 };
enum Modern2UsbHotkeyHomePreference : uint8_t { MODERN2USB_HOTKEY_HOME_AUTO = 0, MODERN2USB_HOTKEY_HOME_OFF = 1, MODERN2USB_HOTKEY_HOME_ON = 2 };
enum Modern2UsbHotkeyProfileFlags : uint8_t { MODERN2USB_HOTKEY_PROFILE_VALID = 0x01 };

// USB uses VID/PID plus its active protocol/report personality. Bluetooth
// stores the exact remote address, never only an address hash.
struct __attribute__((packed, aligned(1))) Modern2UsbHotkeyProfileKey {
  uint16_t vid;
  uint16_t pid;
  uint32_t report_fingerprint;
  uint8_t transport;
  uint8_t protocol;
  uint8_t report_id;
  uint8_t interface_number;
  uint8_t identity[6];
};
struct __attribute__((packed, aligned(1))) Modern2UsbHotkeyProfile {
  Modern2UsbHotkeyProfileKey key;
  uint8_t actions[MODERN2USB_HOTKEY_ACTION_COUNT]; // Left, Right, Up.
  uint8_t home_preference; // Auto=0, Off=1, On=2.
  uint8_t flags;
  uint8_t order;
};
static_assert(sizeof(Modern2UsbHotkeyProfileKey) == 18, "hotkey key layout changed");
static_assert(sizeof(Modern2UsbHotkeyProfile) == 24, "hotkey profile layout changed");

// logical_player is a normalized output player, never a physical USB slot.
bool modern2usbHotkeyKeyForController(uint8_t logical_player, Modern2UsbHotkeyProfileKey& key);
bool modern2usbHotkeyControllerCapabilities(uint8_t logical_player, uint32_t& known_buttons, uint32_t& present_buttons, uint32_t& connection_token);
bool modern2usbHotkeyProfileRead(const Modern2UsbHotkeyProfileKey& key, Modern2UsbHotkeyProfile& profile);
inline bool modern2usbHotkeyProfileFind(const Modern2UsbHotkeyProfileKey& key, Modern2UsbHotkeyProfile& profile) {
  return modern2usbHotkeyProfileRead(key, profile);
}
bool modern2usbHotkeyProfileUpsert(const Modern2UsbHotkeyProfile& profile);
bool modern2usbHotkeyProfileRemove(const Modern2UsbHotkeyProfileKey& key);
bool modern2usbHotkeyProfilesClear();
uint32_t modern2usbHotkeyProfilesRevision();
bool modern2usbHotkeyProfilesStorageLayoutValid();
uint16_t modern2usbHotkeyProfilesStorageRequiredEnd();
uint16_t modern2usbHotkeyProfilesStorageRegionOffset();
uint16_t modern2usbHotkeyProfilesStorageRegionEnd();
void modern2usbHotkeyProfilesPersistenceService();
bool modern2usbHotkeyProfilesPersistenceFlushNow();
void modern2usbHotkeyProfilesPersistenceDiscardDeferred();
bool modern2usbHotkeyProfilesPersistenceHasPending();
