#include "../product_config.h"
#include "menu_jvs_status.h"

#if defined(PRODUCT_JVSIO2USB) && defined(ENABLE_INPUT_JVS) && \
    defined(ADAPT_FEATURE_OLED_DISPLAY)

#include <Arduino.h>
#include <stdio.h>

#include "../input/jvs/jvs_host_runtime.h"
#include "../platform/buzzer.h"
#include "../platform/display_runtime_state.h"
#include "menu_helpers.h"
#include "menu_jvs_analog.h"
#include "menu_ui_state.h"

namespace {

constexpr uint32_t kStatusRefreshMs = 250;
uint32_t lastStatusRenderMs = 0;
char statusRows[8][22] = {};
bool statusRowsValid = false;

void printStatusRow(uint8_t row, const char* text) {
  char visible[22] = {};
  for (uint8_t i = 0; i < 21 && text[i]; ++i) visible[i] = text[i];
  bool changed = !statusRowsValid;
  for (uint8_t i = 0; i < sizeof(visible); ++i) {
    if (statusRows[row][i] != visible[i]) changed = true;
    statusRows[row][i] = visible[i];
  }
  if (!changed) return;
  display.setRow(row);
  display.setCol(0);
  clearRow(row);
  display.print(visible);
}

}  // namespace

void renderJvsStatus(bool selectJustPressed, bool navigateJustPressed,
                     const MenuControllerInput& controllerInput) {
  if (selectJustPressed || controllerInput.aJust) {
    jvsStatusActive = false;
    jvsStatusInitialized = false;
    openJvsAnalogDiagnostics();
    return;
  }
  if (navigateJustPressed || controllerInput.bJust) {
    jvsStatusActive = false;
    jvsStatusInitialized = false;
    display.clear();
    buzzer.playMenuNav();
    return;
  }

  const uint32_t now = millis();
  if (jvsStatusInitialized &&
      now - lastStatusRenderMs < kStatusRefreshMs) {
    return;
  }
  if (!jvsStatusInitialized) statusRowsValid = false;
  jvsStatusInitialized = true;
  lastStatusRenderMs = now;

  const JvsHostState& state = getJvsHostState();
  const JvsBoardInfo& board = getJvsBoardInfo();
  const JvsDebugSnapshot debug = getJvsDebugSnapshot();
  const uint32_t syncAge =
      state.last_sync_at_ms == 0 ? 0xffffffffu : now - state.last_sync_at_ms;
  char line[32] = {};

  display.setFont(System5x7);
  display.set1X();
  printStatusRow(0, "-- JVS Status --");

  if (board.id_received && board.io_id[0] != '\0') {
    // Match printOledMarqueeText's 250 ms phase and end holds, caching only
    // the visible window so an unchanged hold does not write to the OLED.
    size_t len = 0;
    while (board.io_id[len]) ++len;
    size_t offset = 0;
    if (len > 21) {
      const uint8_t overflow = static_cast<uint8_t>(len - 21);
      const uint8_t phase = static_cast<uint8_t>((now / 250) % (overflow + 13));
      offset = phase < 6 ? 0 : phase < 6 + overflow ? phase - 6 : overflow;
    }
    printStatusRow(1, board.io_id + offset);
  } else {
    printStatusRow(1, "Board ID pending");
  }

  snprintf(line, sizeof(line), "Conn%u Ready%u State%u",
           state.connected ? 1u : 0u,
           state.ready ? 1u : 0u,
           debug.machine_state);
  printStatusRow(2, line);

  snprintf(line, sizeof(line), "P%u B%u C%u A%ux%u",
           board.players, getJvsPhysicalButtonsPerPlayer(board),
           board.coins, board.analog_channels, board.analog_bits);
  printStatusRow(3, line);

  if (syncAge == 0xffffffffu) {
    snprintf(line, sizeof(line), "Sync %lu age ---",
             static_cast<unsigned long>(debug.synced_count));
  } else {
    snprintf(line, sizeof(line), "Sync %lu age %lums",
             static_cast<unsigned long>(debug.synced_count),
             static_cast<unsigned long>(syncAge));
  }
  printStatusRow(4, line);

  snprintf(line, sizeof(line), "Reset%lu TO%lu Bad%lu RX%lu",
           static_cast<unsigned long>(debug.bus_reset_count),
           static_cast<unsigned long>(debug.reply_timeout_count),
           static_cast<unsigned long>(debug.bad_reply_count),
           static_cast<unsigned long>(debug.uart_rx_error_count));
  printStatusRow(5, line);

  snprintf(line, sizeof(line), "Sense %umV RX%lu TX%lu",
           debug.sense_mv,
           static_cast<unsigned long>(debug.uart_rx_byte_count),
           static_cast<unsigned long>(debug.uart_tx_byte_count));
  printStatusRow(6, line);
  printStatusRow(7, "Select=Axes Nav=Back");
  statusRowsValid = true;
}

#else

void renderJvsStatus(bool, bool, const MenuControllerInput&) {}

#endif
