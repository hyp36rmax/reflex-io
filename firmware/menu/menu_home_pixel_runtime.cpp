#include "menu_home_pixel_runtime.h"
#include "../platform/display_runtime_state.h"
#include "menu_runtime_state.h"

#if defined(PRODUCT_CLASSIC2USB) && defined(USE_I2C_DISPLAY)
namespace { oled_pixel::Surface surface; }
extern "C" bool reflex_oled_pixel_ram_write(uint8_t col, uint8_t row, uint8_t value) {
  return surface.write(col, row, value);
}
void beginHomePixelFrame(bool fullRefresh) {
  display.flush(); oled_pixel::sharedSurface() = &surface; surface.begin(fullRefresh);
  surface.buttonLabels = menu_home_button_labels != 0;
}
void endHomePixelFrame() {
  const uint8_t col = display.col(), row = display.row();
  surface.end(display);
  display.setCursor(col, row);
}
#else
void beginHomePixelFrame(bool) {}
void endHomePixelFrame() {}
#endif
