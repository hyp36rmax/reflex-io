#include "mister_status_state.h"

#include <string.h>

#include "mister_serial_device.h"

namespace mister_serial {
namespace {
char* value(char** fields, uint8_t count, const char* key) {
  const size_t n = strlen(key);
  for (uint8_t i = 3; i < count; ++i)
    if (strncmp(fields[i], key, n) == 0 && fields[i][n] == '=') return fields[i] + n + 1;
  return nullptr;
}
bool equals(const char* a, const char* b) { return a != nullptr && strcmp(a, b) == 0; }
bool number(const char* text, uint32_t maximum, uint32_t& out) {
  if (!text || !*text) return false;
  uint32_t result = 0;
  for (const char* p = text; *p; ++p) {
    if (*p < '0' || *p > '9' || result > maximum / 10 ||
        (result == maximum / 10 && uint32_t(*p - '0') > maximum % 10)) return false;
    result = result * 10 + uint32_t(*p - '0');
  }
  out = result;
  return true;
}
template <size_t N> void text(char (&out)[N], const char* in) {
  const size_t n = in ? strlen(in) : 0;
  const size_t copied = n < N - 1 ? n : N - 1;
  if (copied) memcpy(out, in, copied);
  out[copied] = '\0';
}
template <size_t N> bool sameText(const char (&stored)[N], const char* incoming) {
  return incoming && strncmp(stored, incoming, N - 1) == 0;
}
bool validStatusSession(const char* session) {
  if (!session) return true;  // Older status-only Main has no request session.
  if (strlen(session) != 16) return false;
  for (uint8_t i = 0; i < 16; ++i)
    if (!((session[i] >= '0' && session[i] <= '9') ||
          (session[i] >= 'a' && session[i] <= 'f') ||
          (session[i] >= 'A' && session[i] <= 'F'))) return false;
  return true;
}
}  // namespace

void StatusState::takeRecord(char** fields, uint8_t count, uint32_t now) {
  uint32_t next = 0;
  if (count < 3 || !equals(fields[0], "MISTER_STATUS/1") ||
      !number(fields[1], UINT32_MAX, next) || next == 0) return;
  const char* event = fields[2];
  if (equals(event, "HELLO")) {
    if (!validStatusSession(value(fields, count, "session"))) return;
    // Keep the last committed view until the reconnect snapshot replaces it.
    loading = previewActive = false;
    loadingName[0] = preview[0] = '\0';
    for (auto& assignment : assignments) assignment = Assignment{};
    ++revision;
    sequence = next;
    active = true;
    return;
  }
  if (!active || (int32_t)(next - sequence) <= 0) return;
  if (equals(event, "CORE")) {
    const char* name = value(fields, count, "name");
    if (!name || strlen(name) >= sizeof(core)) return;
    if (!equals(core, name)) {
      text(core, name);
      game[0] = gameCrc[0] = gameSerial[0] = loadingName[0] = preview[0] = '\0';
      hasArtwork = loading = previewActive = false;
      ++revision;
      ++wakeRevision;
    }
  } else if (equals(event, "ART")) {
    if (!core[0] || !equals(value(fields, count, "core"), core)) return;
    const char* state = value(fields, count, "state");
    if (equals(state, "none")) {
      if (hasArtwork) { hasArtwork = false; ++revision; }
    } else if (equals(state, "set") &&
               equals(value(fields, count, "format"), "ssd1306-128x64")) {
      char* data = value(fields, count, "data");
      size_t decoded = 0;
      if (!data || strlen(data) != 1368 || !base64DecodeInPlace(data, &decoded) ||
          decoded != kArtBytes) return;
      if (!hasArtwork || memcmp(artwork, data, kArtBytes) != 0) ++wakeRevision;
      memcpy(artwork, data, kArtBytes);
      hasArtwork = true;
      ++revision;
    } else return;
  } else if (equals(event, "GAME")) {
    const char* state = value(fields, count, "state");
    const char* name = value(fields, count, "name");
    if (equals(state, "loading") && name) {
      if (name[0] && (!loading || !sameText(loadingName, name))) ++wakeRevision;
      text(loadingName, name);
      loading = true;
      percent = 0;
      loadDeadline = now + kLoadTimeoutMs;
    } else if (equals(state, "loaded") && name) {
      const char* crc = value(fields, count, "crc32");
      if (crc && crc[0]) {
        if (strlen(crc) != 8) return;
        for (const char* p = crc; *p; ++p) {
          if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
                (*p >= 'A' && *p <= 'F'))) return;
        }
      }
      const char* serial = value(fields, count, "serial");
      // Filling in an ID for the same title is metadata, while a replacement
      // of an already known ID can identify a different image with that name.
      const bool changedCrc = gameCrc[0] && crc && crc[0] && !sameText(gameCrc, crc);
      const bool changedSerial = gameSerial[0] && serial && serial[0] && !sameText(gameSerial, serial);
      if (name[0] && (loading || !sameText(game, name) || changedCrc || changedSerial)) {
        ++wakeRevision;
      }
      text(game, name);
      text(gameCrc, crc);
      text(gameSerial, serial);
      loading = false;
      loadingName[0] = '\0';
    } else if (equals(state, "empty")) {
      game[0] = gameCrc[0] = gameSerial[0] = loadingName[0] = '\0';
      loading = false;
    } else return;
    ++revision;
  } else if (equals(event, "LOAD")) {
    const char* state = value(fields, count, "state");
    if (equals(state, "done")) {
      loading = false;
      loadingName[0] = '\0';
    } else if (equals(state, "start") || equals(state, "progress")) {
      uint32_t parsed = 0;
      if (!number(value(fields, count, "percent"), 100, parsed)) return;
      percent = (uint8_t)parsed;
      loading = true;
      loadDeadline = now + kLoadTimeoutMs;
    } else return;
    ++revision;
  } else if (equals(event, "PREVIEW")) {
    const char* state = value(fields, count, "state");
    if (equals(state, "clear")) {
      previewActive = false;
      preview[0] = '\0';
    } else if (equals(state, "active") && value(fields, count, "name")) {
      text(preview, value(fields, count, "name"));
      previewActive = preview[0] != '\0';
      previewDeadline = now + kPreviewTimeoutMs;
    } else return;
    ++revision;
  } else if (equals(event, "OSD") && equals(value(fields, count, "visible"), "0")) {
    if (previewActive) { previewActive = false; ++revision; }
  } else if (equals(event, "CONTROLLER")) {
    uint32_t itf = 0, player = 0;
    if (!equals(value(fields, count, "local"), "1") ||
        !number(value(fields, count, "interface"), 254, itf)) return;
    const char* state = value(fields, count, "state");
    if (!equals(state, "disconnected") && !equals(state, "assigned") &&
        !equals(state, "present") && !equals(state, "connected")) return;
    if (!equals(state, "disconnected") &&
        !number(value(fields, count, "player"), 16, player)) return;
    Assignment* destination = nullptr;
    for (auto& assignment : assignments) {
      if (assignment.interfaceNumber == itf) { destination = &assignment; break; }
      if (!destination && assignment.interfaceNumber == 255) destination = &assignment;
    }
    if (destination) {
      destination->interfaceNumber = (uint8_t)itf;
      destination->player = (uint8_t)player;
      ++revision;
    } else return;
  } else return;
  // Invalid or unsupported records cannot poison the ordering gate.
  sequence = next;
}

void StatusState::disconnected() {
  active = loading = previewActive = false;
  for (auto& assignment : assignments) assignment = Assignment{};
  ++revision;
}

void StatusState::expire(uint32_t now) {
  if (loading && (int32_t)(now - loadDeadline) >= 0) { loading = false; ++revision; }
  if (previewActive && (int32_t)(now - previewDeadline) >= 0) { previewActive = false; ++revision; }
}

}  // namespace mister_serial
