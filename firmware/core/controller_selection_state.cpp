#include "controller_selection_state.h"

#include "controller_frame_state.h"
#include "../input/runtime/input_frame_runtime.h"
#include "../product_config.h"

namespace {
uint8_t selectedPlayer = 0;

uint8_t selectablePlayerCount() {
  uint8_t count = inputPortCount();
  if (count == 0 || count > MAX_USB_OUT) {
    count = MAX_USB_OUT;
  }
  return count == 0 ? 1 : count;
}
}  // namespace

uint8_t selectedControllerPlayer() {
  const uint8_t count = selectablePlayerCount();
  if (selectedPlayer >= count) {
    selectedPlayer = 0;
  }
#if !ADAPT_POLICY_QUICK_SELECTED_CONTROLLER
  // Nobody picks a player on these products, so menus follow the pad that is
  // plugged in: a lone P2 is not an empty P1.
  if (!controllerFrameConst(selectedPlayer).connected) {
    for (uint8_t i = 0; i < count; ++i) {
      if (controllerFrameConst(i).connected) return i;
    }
  }
#endif
  return selectedPlayer;
}

void setSelectedControllerPlayer(uint8_t player) {
  const uint8_t count = selectablePlayerCount();
  selectedPlayer = player < count ? player : 0;
}

void cycleSelectedControllerPlayer(bool forward) {
  const uint8_t count = selectablePlayerCount();
  const uint8_t current = selectedControllerPlayer();
  selectedPlayer = forward
      ? static_cast<uint8_t>((current + 1u) % count)
      : static_cast<uint8_t>(current == 0 ? count - 1u : current - 1u);
}

const controller_state_t& selectedControllerFrame() {
  return controllerFrameConst(selectedControllerPlayer());
}
