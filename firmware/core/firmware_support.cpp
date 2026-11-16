#include "firmware_support.h"

#include "../platform/buzzer.h"
#include "../platform/cooperative_oled.h"
#include "../platform/rgb_led.h"
#include "hardware/watchdog.h"
#include "configuration_persistence.h"
#include "product_reset_services.h"
#include "settings_store.h"
#include "turbo.h"
#include "../features/jogcon_capture/jogcon_capture.h"
#include "../features/n64_capture/n64_capture.h"
#include "../features/host_capture/host_capture.h"
#include "../features/output_trial/output_trial.h"

#ifdef ADAPT_FEATURE_RESET_BUTTON
ButtonHandler resetButton(PIN_RESET_BTN, true, BTN_RESET_LONG_PRESS_MS);
#else
BootselButtonHandler resetButton;
#endif
ButtonHandler modeButton(pinBtn, true, BTN_SYSTEM_MENU_HOLD_MS);

ps4_device_type_emum ps4_type = PS4_DEVICE_TYPE_GAMEPAD;

bool prepareForFirmwareReset(void) {
  if (!configurationPersistencePrepareForReset()) {
    // Keep running with the RAM overlay intact. A caller may report/retry the
    // reset, but it must never reboot and silently discard an accepted save.
    return false;
  }

  // PWM keeps running when the RP2040 enters its ROM bootloader. Silence any
  // boot/menu melody first so the current note cannot latch indefinitely.
  buzzer.stop();
  #ifdef USE_WS2812
  // WS2812 pixels retain their last frame while the RP2040 resets. Explicitly
  // latch black so a prior controller-family color cannot survive into boot.
  rgbLed.setEnabled(false);
  #endif
  return true;
}

void reboot(void) {
  if (productDeferIntentionalReset()) return;
  if (!prepareForFirmwareReset()) {
    return;
  }
  productMarkIntentionalReset();
#if defined(ADAPT_FEATURE_N64_CAPTURE)
  n64CaptureBeforeReboot();
#endif
#if defined(ADAPT_FEATURE_HOST_CAPTURE)
  hostCaptureBeforeReboot();
#endif
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
  jogconCaptureBeforeReboot();
#endif
  // watchdog_enable() stamps scratch[4] with the SDK's own marker. The
  // one-boot input-mode hint keeps its magic in that register, so put it back:
  // otherwise the next boot never sees which controller asked for this reset
  // and an Auto-input NSO identity change reboots forever.
  const uint32_t inputModeHint = watchdog_hw->scratch[AUTO_INPUT_MODE_SCRATCH_MAGIC];
  watchdog_enable(1, false);
  if (inputModeHint == AUTO_INPUT_MODE_MAGIC) {
    watchdog_hw->scratch[AUTO_INPUT_MODE_SCRATCH_MAGIC] = inputModeHint;
  }
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
  jogconCapturePreserveSram();
#endif
#if defined(ADAPT_FEATURE_N64_CAPTURE)
  n64CapturePreserveSram();
#endif
#if defined(ADAPT_FEATURE_HOST_CAPTURE)
  hostCapturePreserveSram();
#endif
#if defined(ADAPT_FEATURE_OUTPUT_TRIAL)
  outputTrialPreserveSram();
#endif
  for (;;) {
  }
}

void markIntentionalReset(void) { productMarkIntentionalReset(); }

void rebootToBootloader(void) {
  if (!prepareForFirmwareReset()) {
    return;
  }
  productMarkIntentionalReset();
  productPrepareBootloaderReset();
  rp2040.rebootToBootloader();
}

void setLed(uint8_t state) {
  #ifdef LED_BUILTIN
    gpio_put(LED_BUILTIN, state);
  #endif
}

void delayWithButtonPolling(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    modeButton.poll();
    resetButton.poll();
    buzzer.update();
    delayWithCooperativeOled(1);
  }
}

void waitWithBuzzerUpdates(uint32_t durationMs) {
  uint32_t waitStart = millis();
  while (millis() - waitStart < durationMs) {
    buzzer.update();
    delayWithCooperativeOled(5);
  }
}

void webhid_set_turbo_rate(uint8_t btn_index, uint8_t rate) {
  if (btn_index < TURBO_BTN_COUNT && rate < TURBO_RATE_LAST) {
    turbo.setButtonRate((TurboButton)btn_index, (TurboRate)rate);
  }
}

