#pragma once

#include <stdint.h>

// Per-button remap and turbo writes from WebHID and CDC arrive one byte at a
// time. Each used to commit a full per-mode EEPROM record, so a host writing
// 19 remaps cost 19 commits. Coalesce a burst into one commit once the host
// has been quiet for a short window; flush on demand before any mode change
// or reset so nothing is orphaned.
void requestDeferredRemapSave();
void requestDeferredTurboSave();
void deferredModeSavesService();
bool deferredModeSavesFlushNow();
bool deferredModeSavesHasPending();
void deferredModeSavesDiscard();
