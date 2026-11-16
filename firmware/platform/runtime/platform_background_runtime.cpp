#include "../../product_config.h"

#include "platform_background_runtime.h"
#include "platform_background_product.h"

#include "../buzzer.h"
#include "../latency_test.h"
#include "../rgb_led.h"
#include "../../core/rumble_test_runtime.h"
#include "../../output/output_runtime_state.h"

void runPlatformBackgroundTasks() {
  static bool ps5QuietWasActive = false;
  if (is_ps5_timing_quiet_mode_active()) {
    // PS5 auth is sensitive to unrelated timing work. Keep PWM/PIO LED
    // and latency bookkeeping out of the PS5 console loop. Nothing here
    // updates the buzzer, so it is silenced, not just stopped: a menu beep
    // started now would otherwise sound until the next save or restart.
    if (!ps5QuietWasActive) {
      buzzer.setQuiet(true);
      ps5QuietWasActive = true;
    }
    runProductTimingQuietBackgroundTasks();
    return;
  }

  if (ps5QuietWasActive) {
    buzzer.setQuiet(false);
    resetProductBackgroundAfterTimingQuiet();
  }
  ps5QuietWasActive = false;
  buzzer.update();
  latencyTest.update();
  rumbleTestUpdate();

  #ifdef USE_WS2812
  updateProductBackgroundTasks();
  rgbLed.update();
  #endif
}
