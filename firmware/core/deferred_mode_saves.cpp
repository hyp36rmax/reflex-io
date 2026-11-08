#include "deferred_mode_saves.h"

#include <Arduino.h>

#include "device_runtime_state.h"
#include "settings_store.h"
#include "settings_store_internal.h"
#include "turbo.h"

namespace {

constexpr uint32_t kQuietMs = 150;
bool remapPending = false;
bool turboPending = false;
uint32_t lastRequestMs = 0;

void commit() {
  if (remapPending) {
    remapPending = false;
    saveButtonRemaps();
  }
  if (turboPending) {
    turboPending = false;
    uint8_t rates[TURBO_BTN_COUNT];
    for (uint8_t i = 0; i < TURBO_BTN_COUNT; ++i) {
      rates[i] = (uint8_t)turbo.getButtonRate((TurboButton)i);
    }
    saveTurboSettingsForMode(deviceMode, rates);
  }
}

}  // namespace

void requestDeferredRemapSave() {
  remapPending = true;
  lastRequestMs = millis();
}

void requestDeferredTurboSave() {
  turboPending = true;
  lastRequestMs = millis();
}

void deferredModeSavesService() {
  if ((remapPending || turboPending) &&
      (uint32_t)(millis() - lastRequestMs) >= kQuietMs) {
    commit();
  }
}

bool deferredModeSavesFlushNow() {
  commit();
  return true;
}

bool deferredModeSavesHasPending() {
  return remapPending || turboPending;
}

void deferredModeSavesDiscard() {
  remapPending = false;
  turboPending = false;
}
