#pragma once

#include <stdint.h>

#include "device_mode.h"

// Stable persisted input-mode IDs.
//
// DeviceEnum ordinals depend on which ENABLE_INPUT_* gates a build compiles,
// but the saved input mode and each per-mode EEPROM record address are stored
// on the device. These IDs are what storage uses instead. Their values are
// frozen to the ordinals every shipped build used on 2026-10-15, so adopting
// them moved no stored data:
//
//   Classic2USB (and its diagnostic/capture images): AUTODETECT=1, SMS=2 ...
//     JPC=19, then the appended INTV=20, COLECOVISION=21 and CDI=22.
//   Retro builds compiled without Auto input (classic2usb_retrozordhdmi): the
//     same retro IDs minus one, which is what that image has always stored.
//   USB2DB15 / USB2JVS / Modern2USB: USB=1, ESP32_SPI=2.
//   JVSIO2USB: JVS=1 (PS5 timing diagnostics with DUMMY=1 store JVS=2).
//   Direct-GPIO products (Reflex CTRL/Encode, MiSTercade): CUSTOM=1.
//
// Never renumber an entry. A newly enabled or new mode takes a fresh ID after
// the highest one in use and an appended per-mode record (settings_layout.h).
constexpr uint8_t PERSISTED_INPUT_MODE_ID_INVALID = 0xFF;

namespace persisted_input_mode_detail {

#if defined(ENABLE_INPUT_AUTODETECT)
constexpr uint8_t kRetroShift = 0;
#else
constexpr uint8_t kRetroShift = 1;
#endif

#if defined(ENABLE_INPUT_DUMMY)
constexpr uint8_t kJvsShift = 1;
#else
constexpr uint8_t kJvsShift = 0;
#endif

constexpr uint8_t retro(uint8_t classicId) {
  return (uint8_t)(classicId - kRetroShift);
}

}  // namespace persisted_input_mode_detail

constexpr uint8_t persistedInputModeId(DeviceEnum mode) {
  using namespace persisted_input_mode_detail;
  switch (mode) {
    case RZORD_NONE: return 0;
#ifdef ENABLE_INPUT_AUTODETECT
    case RZORD_AUTODETECT: return 1;
#endif
#ifdef ENABLE_INPUT_DUMMY
    case RZORD_DUMMY: return 1;
#endif
#ifdef ENABLE_INPUT_USB
    case RZORD_USB: return 1;
#endif
#ifdef ENABLE_INPUT_ESP32_SPI
    case RZORD_ESP32_SPI: return 2;
#endif
#ifdef ENABLE_INPUT_JVS
    case RZORD_JVS: return (uint8_t)(1 + kJvsShift);
#endif
#ifdef ENABLE_INPUT_CUSTOM
    case RZORD_CUSTOM: return 1;
#endif
#ifdef ENABLE_INPUT_SMS
    case RZORD_SMS: return retro(2);
#endif
#ifdef ENABLE_INPUT_DRIVING
    case RZORD_DRIVING: return retro(3);
#endif
#ifdef ENABLE_INPUT_MEGADRIVE
    case RZORD_MEGADRIVE: return retro(4);
#endif
#ifdef ENABLE_INPUT_SATURN
    case RZORD_SATURN: return retro(5);
#endif
#ifdef ENABLE_INPUT_DREAMCAST
    case RZORD_DREAMCAST: return retro(6);
#endif
#ifdef ENABLE_INPUT_NES
    case RZORD_NES: return retro(7);
#endif
#ifdef ENABLE_INPUT_SNES
    case RZORD_SNES: return retro(8);
#endif
#ifdef ENABLE_INPUT_N64
    case RZORD_N64: return retro(9);
#endif
#ifdef ENABLE_INPUT_GAMECUBE
    case RZORD_GAMECUBE: return retro(10);
#endif
#ifdef ENABLE_INPUT_WII
    case RZORD_WII: return retro(11);
#endif
#ifdef ENABLE_INPUT_VBOY
    case RZORD_VBOY: return retro(12);
#endif
#ifdef ENABLE_INPUT_PCE
    case RZORD_PCE: return retro(13);
#endif
#ifdef ENABLE_INPUT_PSX
    case RZORD_PSX: return retro(14);
#endif
#ifdef ENABLE_INPUT_PSX_JOG
    case RZORD_PSX_JOG: return retro(15);
#endif
#ifdef ENABLE_INPUT_NEOGEO
    case RZORD_NEOGEO: return retro(16);
#endif
#ifdef ENABLE_INPUT_3DO
    case RZORD_3DO: return retro(17);
#endif
#ifdef ENABLE_INPUT_JAGUAR
    case RZORD_JAGUAR: return retro(18);
#endif
#ifdef ENABLE_INPUT_JPC
    case RZORD_JPC: return retro(19);
#endif
#ifdef ENABLE_INPUT_INTV
    case RZORD_INTV: return retro(20);
#endif
#ifdef ENABLE_INPUT_COLECOVISION
    case RZORD_COLECOVISION: return retro(21);
#endif
#ifdef ENABLE_INPUT_CDI
    case RZORD_CDI: return retro(22);
#endif
    // GBA, PSX_DANCE, PADDLE, GAMEPORT and MEMCARD have never shipped and
    // have no ID yet; enabling one fails the checks below until it gets one.
    default: return PERSISTED_INPUT_MODE_ID_INVALID;
  }
}

constexpr DeviceEnum deviceModeFromPersistedInputModeId(uint8_t id) {
  for (uint8_t value = 0; value < (uint8_t)RZORD_LAST; ++value) {
    if (persistedInputModeId((DeviceEnum)value) == id) {
      return (DeviceEnum)value;
    }
  }
  return RZORD_NONE;
}

// Per-mode records of the original layout are indexed by persisted ID below
// this count; appended modes (device_mode.h) follow every other store.
constexpr uint8_t persistedInputModeLayoutSlots() {
  uint8_t slots = 0;
  for (uint8_t value = 0; value < (uint8_t)RZORD_LAST; ++value) {
    const DeviceEnum mode = (DeviceEnum)value;
    if (isAppendedInputMode(mode)) continue;
    const uint8_t id = persistedInputModeId(mode);
    if (id != PERSISTED_INPUT_MODE_ID_INVALID && id + 1u > slots) {
      slots = (uint8_t)(id + 1u);
    }
  }
  return slots;
}

namespace persisted_input_mode_detail {

constexpr bool everyEnabledModeHasUniqueId() {
  for (uint8_t a = 0; a < (uint8_t)RZORD_LAST; ++a) {
    const uint8_t idA = persistedInputModeId((DeviceEnum)a);
    if (idA == PERSISTED_INPUT_MODE_ID_INVALID) return false;
    for (uint8_t b = (uint8_t)(a + 1); b < (uint8_t)RZORD_LAST; ++b) {
      if (persistedInputModeId((DeviceEnum)b) == idA) return false;
    }
  }
  return true;
}

// Stores not yet converted to persisted IDs (classic dual-merge masks, menu
// mode-visibility bits, per-output remap pairs, WebHID/serial raw values)
// still use DeviceEnum ordinals directly. Until they are converted, a gate
// change that renumbers DeviceEnum must fail here instead of scrambling them.
constexpr bool ordinalsMatchPersistedIds() {
  for (uint8_t value = 0; value < (uint8_t)RZORD_LAST; ++value) {
    if (persistedInputModeId((DeviceEnum)value) != value) return false;
  }
  return true;
}

}  // namespace persisted_input_mode_detail

static_assert(persisted_input_mode_detail::everyEnabledModeHasUniqueId(),
              "every enabled input mode needs its own persisted input-mode ID");
static_assert(persisted_input_mode_detail::ordinalsMatchPersistedIds(),
              "DeviceEnum ordinals changed: stores still keyed by ordinal "
              "would scramble saved settings (persisted_input_mode.h)");

#if defined(ENABLE_INPUT_SMS) && defined(ENABLE_INPUT_COLECOVISION)
// Retro products: the per-mode region holds the 20 original modes (19 without
// Auto input) and INTV/ColecoVision use appended records.
static_assert(persistedInputModeLayoutSlots() ==
                  20 - persisted_input_mode_detail::kRetroShift,
              "retro per-mode EEPROM region size moved");
#endif
