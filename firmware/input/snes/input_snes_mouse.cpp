#include "Input_Snes.h"

bool RZInputSnes::tryPollSnesMouse(uint8_t port) {
#ifdef ENABLE_EXPERIMENTAL_CONSOLE_MOUSE
  const input_snes_config_t& pins = input_snes_config[port];
  struct Bus {
    const input_snes_config_t& pins;
    void clock(bool high) { digitalWrite(pins.clk, high ? HIGH : LOW); }
    void latch(bool high) { digitalWrite(pins.lat, high ? HIGH : LOW); }
    bool data() { return digitalRead(pins.dat0); }
    void waitUs(unsigned us) { delayMicroseconds(us); }
  } bus{pins};
  SnesMouseReport report;
  if (!readSnesMouseReport(bus, report)) return false;

  SnesController& mouse = snes_mouse[port];
  mouse.copyCurrentToLast();
  mouse.currentState.id = 0x8;
  mouse.currentState.digital = report.digital;
  mouse.currentState.extended = report.extended;
  mouse.currentState.mouseX = report.x;
  mouse.currentState.mouseY = report.y;
  mouse.currentState.mouseButtons = report.buttons;
  mouse.currentState.mouseSensitivity = report.sensitivity;
  snes_mouse_active[port] = true;
  return true;
#else
  (void)port;
  return false;
#endif
}
