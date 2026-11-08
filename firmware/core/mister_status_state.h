#pragma once

#include <stdint.h>

namespace mister_serial {

// Presentation only. No settings, USB personality or controller input changes.
struct StatusState {
  static constexpr uint16_t kArtBytes = 1024;
  static constexpr uint32_t kLoadTimeoutMs = 15000;
  static constexpr uint32_t kPreviewTimeoutMs = 3500;
  struct Assignment { uint8_t interfaceNumber = 255, player = 0; };

  char core[96] = {};
  char game[128] = {};
  char gameCrc[9] = {};
  char gameSerial[64] = {};
  char loadingName[128] = {};
  char preview[128] = {};
  uint8_t artwork[kArtBytes] = {};
  Assignment assignments[6];
  uint32_t revision = 0;
  uint32_t wakeRevision = 0;
  uint32_t sequence = 0;
  uint32_t loadDeadline = 0, previewDeadline = 0;
  bool active = false, hasArtwork = false, loading = false, previewActive = false;
  uint8_t percent = 0;

  // Exact MISTER_STATUS/1 fields, split at TAB. Unknown records are ignored.
  void takeRecord(char** fields, uint8_t count, uint32_t now);
  void disconnected();
  void expire(uint32_t now);
};

}  // namespace mister_serial
