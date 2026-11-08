#pragma once

#include "settings_store.h"
#include "settings_persistence_policy.h"

// Products with protected console sessions defer EEPROM publication while a live PS4/PS5
// console session owns device USB. Other products and output modes use the
// existing immediate persistence path through no-op stubs in the runtime.
bool settingsPersistenceProtectedSessionActive();
bool settingsPersistenceShouldStageWrites();
bool settingsPersistenceReadPendingGlobal(GlobalSettingsRecord& settings);
bool settingsPersistenceReadPendingPerMode(DeviceEnum mode, PerModeSettingsRecord& settings);
void settingsPersistenceStageGlobal(const GlobalSettingsRecord& settings);
void settingsPersistenceStagePerMode(DeviceEnum mode, const PerModeSettingsRecord& settings);
void settingsPersistenceService();
bool settingsPersistenceFlushNow();
void settingsPersistenceDiscardDeferred();
void settingsPersistenceBeginImmediate();
void settingsPersistenceEndImmediate();
bool settingsPersistenceHasPending();

#if defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
void settingsPersistenceHostSetProtected(bool protectedSessionActive);
#endif
