#include "../../../product_config.h"

#include "../../../core/button_remap.h"
#include "../../../core/deferred_mode_saves.h"
#include "../../../core/firmware_support.h"
#include "../../../core/remap_pairs.h"
#include "../../../core/settings_store.h"
#include "../../../core/settings_store_internal.h"
#include "../../output_runtime_state.h"
#include "webhid_protocol.h"
#include "webhid_write_reports.h"

void webhid_write_turbo_report(const uint8_t* buffer, uint16_t bufsize) {
  if (bufsize < 3 || buffer[0] != WEBHID_MAGIC_BYTE) return;

  if (buffer[1] == 0xFF && bufsize >= 2 + TURBO_BTN_COUNT) {
    uint8_t turbo_rates[TURBO_BTN_COUNT];
    for (uint8_t i = 0; i < TURBO_BTN_COUNT; ++i) {
      const uint8_t rate = buffer[2 + i];
      turbo_rates[i] = (rate < TURBO_RATE_LAST) ? rate : (uint8_t)TURBO_OFF;
    }
    turbo.setAllRates(turbo_rates);
    saveTurboSettingsForMode(deviceMode, turbo_rates);
    return;
  }

  uint8_t btn_index = buffer[1];
  uint8_t rate = buffer[2];
  if (btn_index >= TURBO_BTN_COUNT || rate >= TURBO_RATE_LAST) return;

  webhid_set_turbo_rate(btn_index, rate);
  requestDeferredTurboSave();
}

// Controller slots and N64 assignments; USB2DB15 combined bindings are not
// written here.
static bool webhidRemapValueAllowed(uint8_t source, uint8_t value) {
  return remapValueAllowed(deviceMode, source, value) &&
         (value < REMAP_MAX_BUTTONS || isN64StoredAssignment(deviceMode, source, value));
}

void webhid_write_remap_report(const uint8_t* buffer, uint16_t bufsize) {
  if (bufsize < 3 || buffer[0] != WEBHID_MAGIC_BYTE) return;

  // A full remap store keeps the old map, so the reread shows what is saved.
  uint8_t remaps[REMAP_MAX_BUTTONS];
  memcpy(remaps, active_remaps, REMAP_MAX_BUTTONS);
  if (buffer[1] == 0xFF && bufsize >= 2 + REMAP_RECORD_BUTTONS) {
    for (uint8_t i = 0; i < REMAP_RECORD_BUTTONS; i++) {
      uint8_t val = buffer[2 + i];
      remaps[i] = webhidRemapValueAllowed(i, val) ? val : i;
    }
#if defined(ADAPT_CAP_DPAD_REMAP)
    if (bufsize > WEBHID_REMAP_DPAD_OFFSET + REMAP_DPAD_SLOTS &&
        buffer[WEBHID_REMAP_DPAD_OFFSET] == WEBHID_REMAP_DPAD_MARKER) {
      for (uint8_t d = 0; d < REMAP_DPAD_SLOTS; ++d) {
        const uint8_t slot = REMAP_DPAD_FIRST_SLOT + d;
        const uint8_t val = buffer[WEBHID_REMAP_DPAD_OFFSET + 1 + d];
        remaps[slot] = webhidRemapValueAllowed(slot, val) ? val : slot;
      }
    }
#endif
    if (!remapsFitForPair(deviceMode, configuredOutputMode, remaps)) return;
    memcpy(active_remaps, remaps, REMAP_MAX_BUTTONS);
    (void)saveButtonRemaps();
    return;
  }

  uint8_t btn_index = buffer[1];
  uint8_t target = buffer[2];
  if (btn_index >= REMAP_MAX_BUTTONS || !webhidRemapValueAllowed(btn_index, target)) return;

  remaps[btn_index] = target;
  if (!remapsFitForPair(deviceMode, configuredOutputMode, remaps)) return;
  active_remaps[btn_index] = target;
  requestDeferredRemapSave();
}
