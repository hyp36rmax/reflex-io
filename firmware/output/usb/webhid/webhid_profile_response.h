#pragma once

#include <stdint.h>

struct WebhidRawProfileResponseDecision {
  uint8_t status;
  bool include_profile_payload;
};

// Profile-content reads derive their status from the selected slot. Mutating
// operations return their own acknowledgement even when the operation made the
// selected slot empty (delete/clear) or never selected a slot (commit failure).
inline WebhidRawProfileResponseDecision webhidResolveRawProfileResponse(
    bool operation_response, uint8_t operation_status,
    bool selected_profile_valid) {
  if (operation_response) {
    return {operation_status, false};
  }
  if (!selected_profile_valid) {
    return {1, false};
  }
  return {operation_status, operation_status == 0};
}
