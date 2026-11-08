#include "../../product_config.h"

#ifdef ADAPT_FEATURE_MISTER_SERIAL_STATUS

#include "mister_status_feature.h"

#include <Arduino.h>
#include <string.h>

#include "../../core/controller_settings_state.h"
#include "../../core/mister_status_state.h"
#include "../../menu/menu_bridge.h"
#include "../../menu/menu_idle_runtime.h"
#include "../../menu/menu_system_screens.h"
#include "../../menu/menu_ui_state.h"
#include "../../platform/display_runtime_state.h"

namespace {

constexpr uint32_t kRenderIntervalMs = 125;

mister_serial::StatusState statusState;
bool wasVisible = false;
uint32_t lastRenderedRevision = 0;
uint32_t lastRenderMs = 0;

bool displayMustYield() {
  return menuScreensOwnControllerInput() || isIdleTimeoutReached() || idleAnimationActive ||
         idleDimActive || idleBlankActive || firmwareRollbackNoticeActive();
}

void renderArtwork() {
  for (uint8_t page = 0; page < 8; ++page) {
    display.setCursor(0, page);
    for (uint8_t x = 0; x < 128; ++x) {
      display.ssd1306WriteRam(statusState.artwork[(uint16_t)page * 128 + x]);
    }
  }
}

bool renderMisterStatus() {
  const uint32_t now = millis();
  statusState.expire(now);
  if (mister_display_mode != MISTER_DISPLAY_ARTWORK || !statusState.hasArtwork) {
    if (wasVisible) forceMainDisplayRefresh();
    wasVisible = false;
    return false;
  }
  if (renderFirmwareUpdateScreenIfActive() || renderFirmwareRollbackNoticeIfActive() ||
      renderMenuActionFeedback()) {
    wasVisible = false;
    return true;
  }
  if (displayMustYield()) {
    if (wasVisible) forceMainDisplayRefresh();
    wasVisible = false;
    return false;
  }

  const bool dirty = statusState.wakeRevision != lastRenderedRevision;
  if (!wasVisible || !mainDisplayInitialized ||
      (dirty && (uint32_t)(now - lastRenderMs) >= kRenderIntervalMs)) {
    beginDisplayWire();
    display.begin(&Adafruit128x64, I2C_ADDRESS);
    restoreOledPanelOrientation();
    u8g2.setDrawColor(1);
    display.invertDisplay(false);
    display.setInvertMode(false);
    display.setContrast(display_contrast);
    renderArtwork();
    Wire.end();
    mainDisplayInitialized = true;
    lastRenderedRevision = statusState.wakeRevision;
    lastRenderMs = now;
  }
  wasVisible = true;
  return true;
}

}  // namespace

void misterStatusReceive(char** fields, uint8_t count, uint32_t now) {
  const uint32_t before = statusState.wakeRevision;
  statusState.takeRecord(fields, count, now);
  if (mister_display_mode == MISTER_DISPLAY_ARTWORK && statusState.wakeRevision != before) {
    resetIdleTimer(IdleWakeSource::MisterArtwork);
  }
}

void misterStatusDisconnected() {
  statusState.disconnected();
}

const FeatureModule kMisterStatusFeatureModule = {
  "mister_status",
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  renderMisterStatus,
};

#endif  // ADAPT_FEATURE_MISTER_SERIAL_STATUS
