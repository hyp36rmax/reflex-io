#pragma once
#include "menu_pad_layouts_internal.h"
#include "oled_pixel_surface.h"
#include "menu_classic_pad_art.h"

namespace home_pad_geometry {
struct Placement { int8_t offset = 0, dpadOffset = 0; uint8_t bottom = 63; };
inline bool dpad(const PadButton& button) {
  return uint8_t(button.on) >= PAD_UP_ON && uint8_t(button.on) <= PAD_RIGHT_OFF;
}
inline Placement measure(const PadButton* layout, uint8_t count, const uint8_t* font,
                         bool labels = false) {
  Placement result;
  using namespace menu_pad_layouts_internal;
  if (mouse_art::selected(layout)) {
    result.offset = oled_pixel::centerOffset(24,
        layout==mouse_art::saturn || layout==mouse_art::pce ? 60 : 55);
    return result;
  }
#ifdef ENABLE_INPUT_JAGUAR
  if (layout == padLayoutJaguarRotary) {
    result.offset = oled_pixel::centerOffset(24, 55);
    return result;
  }
#endif
#ifdef ENABLE_INPUT_DRIVING
  if (layout == padLayoutDriving) {
    result.offset = oled_pixel::centerOffset(24, 58);
    return result;
  }
#endif
#if defined(ENABLE_INPUT_PSX) && (!defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB))
  // The turntable dial shares this panel offset (drawBeatmaniaTurntable).
  if (layout == classic_pad_art::beatmania) {
    result.offset = oled_pixel::centerOffset(24, classic_pad_art::kBeatmaniaPanelLast);
    return result;
  }
#endif
  // Measure the union of released/pressed glyphs, never live input state.
  // Both 1P and 2P own all 39 pixels beneath the single summary row.
  for (uint8_t pass = 0; pass < 2; ++pass) {
    int16_t first = 64, last = 0;
    for (uint8_t i = 0; i < count; ++i) {
      const auto& button = layout[i];
      oled_button_art::Button art;
      if (classic_pad_art::get(layout,button,art)) {
        if (art.shape==oled_button_art::Shape::Hidden) continue;
        if (labels) art=oled_button_art::labelBounds(art);
        const int16_t top=24+art.y, bottom=top+art.h-1;
        if (top<first) first=top;
        if (bottom>last) last=bottom;
        continue;
      }
      const int16_t y = 24 + button.row * 8 + (dpad(button) ? result.dpadOffset : 0);
      const uint8_t glyphs[] = {uint8_t(button.on), uint8_t(button.off)};
      for (uint8_t glyph : glyphs) {
        if (glyph < font[4] || glyph >= font[4] + font[5]) continue;
        uint8_t bits = 0;
        for (uint8_t x = 0; x < 5; ++x) bits |= font[6 + (glyph - font[4]) * 5 + x];
        if (labels && !dpad(button)) bits |= 0x7F; // Optional 5x7 output labels.
        // Analog trigger borders extend beyond the digital shoulder glyph.
        if ((button.mask == GPAD_L2 || button.mask == GPAD_R2) &&
            button.on == PAD_SHOULDER_ON) bits |= 0x3E;
        for (uint8_t bit = 0; bit < 8; ++bit) if (bits & (1u << bit)) {
          if (y + bit < first) first = y + bit;
          if (y + bit > last) last = y + bit;
        }
      }
    }
#ifdef ENABLE_INPUT_PSX
    // Include the neGcon twist gauge in its complete panel bounds.
    if (layout == padLayoutPSXNeGcon && last < 61) last = 61;
#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
    if (layout == classic_pad_art::gun) {first=26;last=59;}
#endif
#endif
    if (first > last) return result;
    if (pass == 0 && last - first + 1 > result.bottom - 25 + 1) {
      result.dpadOffset = -1;
      continue;
    }
    result.offset = oled_pixel::centerOffset(uint8_t(first), uint8_t(last), result.bottom);
    break;
  }
  return result;
}
inline Placement placement(const PadButton* layout, uint8_t count, const uint8_t* font) {
  if (!oled_pixel::homeSurface() || !oled_pixel::homeSurface()->active) return {};
  struct Cached { const PadButton* layout = nullptr; Placement value; bool labels = false; };
  static Cached cache[4];
  static uint8_t next = 0;
  if (!layout || !count) return {};
  const bool labels = oled_pixel::homeSurface()->buttonLabels;
  for (const auto& item : cache) if (item.layout == layout && item.labels == labels) return item.value;
  const Placement value = measure(layout, count, font, labels);
  cache[next] = {layout, value, labels}; next = (next + 1) % 4;
  return value;
}
class Origin : public oled_pixel::Origin {
public:
#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
  Origin(const PadButton* layout, uint8_t count, const uint8_t* font, bool direction = false)
      : oled_pixel::Origin(placement(layout, count, font).offset +
                           (direction ? placement(layout, count, font).dpadOffset : 0),
                           25, placement(layout, count, font).bottom) {}
#else
  Origin(const PadButton*, uint8_t, const uint8_t*, bool = false) : oled_pixel::Origin(0) {}
#endif
};
} // namespace home_pad_geometry
