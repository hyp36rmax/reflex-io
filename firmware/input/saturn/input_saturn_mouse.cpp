#include "Input_Saturn.h"
#include "sega_mouse_probe.h"

bool RZInputSaturn::tryPollSaturnMouse(uint8_t port) {
#ifdef ENABLE_EXPERIMENTAL_CONSOLE_MOUSE
  const input_saturn_config_t& pins = input_saturn_config[port];
  struct Bus {
    const input_saturn_config_t& p;
    bool trWasOutput;
    bool thWentLow = false;
    bool trIsInput() { return !gpio_get_dir(p.tr); }
    bool trIsHigh() { return gpio_get(p.tr); }
    void begin() {
      trWasOutput = gpio_get_dir(p.tr);
      // pinMode(OUTPUT) calls gpio_init and clears the latch. Keep TR high
      // before enabling its driver so TH-high cannot request a mouse reset.
      gpio_set_function(p.tr, GPIO_FUNC_SIO);
      gpio_put(p.tr, 1);
      gpio_set_dir(p.tr, GPIO_OUT);
    }
    void end() {
      gpio_put(p.th, 1);
      gpio_put(p.tr, 1);
      if (!trWasOutput) {
        gpio_set_dir(p.tr, GPIO_IN);
        gpio_pull_up(p.tr);
      }
    }
    void th(bool value) { if (!value) thWentLow = true; gpio_put(p.th, value); }
    void tr(bool value) { gpio_put(p.tr, value); }
    void waitUs(unsigned us) { delayMicroseconds(us); }
    bool waitTl(bool value) {
      const uint32_t start = micros();
      while (gpio_get(p.tl) != value) {
        if (uint32_t(micros() - start) >= 100) return false;
      }
      return true;
    }
    uint8_t nibble() {
      const uint32_t pins = gpio_get_all();
      return ((pins >> p.d0) & 1) | (((pins >> p.d1) & 1) << 1) |
             (((pins >> p.d2) & 1) << 2) | (((pins >> p.d3) & 1) << 3);
    }
  } bus{pins, false};
  if (getInternalMode() == 1 && !passiveSegaMousePrefix(bus)) {
    // A rejected all-directions pad probe may have advanced its TH phase.
    // Let the six-button counter reset before the normal library polls it.
    if (bus.thWentLow) bus.waitUs(MEGADRIVE_MIN_TH_HIGH_US);
    return false;
  }
  SaturnMouseReport report;
  if (!readSaturnMouseReport(bus, report)) return false;
  // Sega mouse reports grow upward; USB Y grows down (HSS-0102 captured).
  const int16_t mouseY = -report.y;
  SaturnController& mouse = saturn_mouse[port];
  mouse.copyCurrentToLast();
  mouse.currentState.id = SAT_ID_MOUSE;
  mouse.currentState.mouseX = report.x < -127 ? -127 : (report.x > 127 ? 127 : report.x);
  mouse.currentState.mouseY = mouseY < -127 ? -127 : (mouseY > 127 ? 127 : mouseY);
  mouse.currentState.mouseFlags = report.flags;
  mouse.currentState.mouseButtons = report.buttons;
  mouse.currentState.mouseRawX = report.rawX;
  mouse.currentState.mouseRawY = report.rawY;
  mouse.currentState.mouseOverflow = (report.flags & 12) != 0;
  saturn_mouse_dx[port] = report.x;
  saturn_mouse_dy[port] = mouseY;
  saturn_mouse_active[port] = true;
  return true;
#else
  (void)port;
  return false;
#endif
}
