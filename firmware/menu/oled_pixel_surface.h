#pragma once
#include <stdint.h>
#include <string.h>

// Page-oriented callers retain their fonts/layouts. Only the origin is in
// pixels. Composition preserves neighboring page bits and batches changed
// columns, avoiding an erase/paint flash and extra I2C writes per glyph pixel.
namespace oled_pixel {
struct Surface {
  uint8_t pixels[1024]{};
  uint8_t first[8]{}, last[8]{};
  bool active = false;
  bool positioned = false;
  bool buttonLabels = false;
  int8_t offset = 0;
  uint8_t top = 0, bottom = 63;
  uint32_t clippedInk = 0;

  void begin(bool fullRefresh = false) {
    active = true;
    positioned = false;
    offset = 0; top = 0; bottom = 63;
    clippedInk = 0;
    for (uint8_t page = 0; page < 8; ++page) {
      first[page] = fullRefresh ? 0 : 128;
      last[page] = fullRefresh ? 127 : 0;
    }
  }
  bool write(uint8_t x, uint8_t page, uint8_t value) {
    if (x >= 128 || page >= 8) return false;
    if (!active) { pixels[page * 128 + x] = value; return false; }
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const int16_t y = int16_t(page) * 8 + bit + offset;
      if (y < top || y > bottom) {
        if (value & (1u << bit)) ++clippedInk;
        continue;
      }
      const uint8_t targetPage = uint8_t(y) / 8;
      const uint8_t mask = 1u << (y & 7);
      uint8_t& dest = pixels[targetPage * 128 + x];
      const uint8_t changed = (value & (1u << bit)) ? (dest | mask) : (dest & ~mask);
      if (dest != changed) {
        dest = changed;
        if (x < first[targetPage]) first[targetPage] = x;
        if (x > last[targetPage]) last[targetPage] = x;
      }
    }
    return true;
  }
  template<class Display> void end(Display& display) {
    active = false;
    for (uint8_t page = 0; page < 8; ++page) {
      if (first[page] == 128) continue;
      display.setCursor(first[page], page);
      for (uint16_t x = first[page]; x <= last[page]; ++x)
        display.ssd1306WriteRamBuf(pixels[page * 128 + x]);
    }
    display.flush();
  }
};
// The production instance is allocated only on products enabling pixel Home.
inline Surface*& sharedSurface() { static Surface* surface = nullptr; return surface; }
inline Surface* homeSurface() { return sharedSurface(); }

#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
class Origin {
  Surface* surface;
  int8_t previousOffset;
  uint8_t previousTop, previousBottom;
  bool previousPositioned;
public:
  Origin(int8_t offset, uint8_t top = 25, uint8_t bottom = 63, bool inherit = false)
      : surface(homeSurface()) {
    if (!surface || !surface->active || (inherit && surface->positioned)) { surface = nullptr; return; }
    previousOffset = surface->offset; previousTop = surface->top;
    previousBottom = surface->bottom; previousPositioned = surface->positioned;
    surface->offset = offset; surface->top = top; surface->bottom = bottom;
    surface->positioned = true;
  }
  ~Origin() {
    if (!surface) return;
    surface->offset = previousOffset; surface->top = previousTop;
    surface->bottom = previousBottom; surface->positioned = previousPositioned;
  }
  Origin(const Origin&) = delete;
  Origin& operator=(const Origin&) = delete;
};
#else
// Other production products retain page-aligned Home screens. Compile out
// origin state/branches rather than spending their flash budget on a no-op.
class Origin {
public:
  Origin(int8_t, uint8_t = 25, uint8_t = 63, bool = false) {}
};
#endif

inline int8_t centerOffset(uint8_t first, uint8_t last, uint8_t bottom = 63) {
  return int16_t(25) + (int16_t(bottom) - 25 + 1 - (last - first + 1)) / 2 - first;
}
} // namespace oled_pixel
