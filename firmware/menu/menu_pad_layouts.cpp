#include "../product_config.h"

#include "menu_pad_layouts.h"
#include "menu_pad_layouts_internal.h"

#include "../platform/display_runtime_state.h"
#include "menu_home_pad_geometry.h"
#ifdef USE_I2C_DISPLAY
#include "../src/SSD1306Ascii/fonts/ReflexPad5x7.h"
#endif

namespace {

inline bool isDirectionalPadGlyph(const PadButton& button, char onChar, char offChar) {
  return button.on == onChar && button.off == offChar;
}

}  // namespace

bool isGameCubePadLayout(const PadButton* layout) {
  return layout == menu_pad_layouts_internal::padLayoutGC;
}

void drawDpadCentersForLayout(const PadButton* layout, uint8_t count, uint8_t colOffset, uint8_t rowOffset) {
  drawDpadCentersForLayoutState(layout, count, 0, colOffset, rowOffset);
}

void drawDpadCentersForLayoutState(const PadButton* layout, uint8_t count, uint32_t state, uint8_t colOffset, uint8_t rowOffset) {
#ifndef USE_I2C_DISPLAY
  (void)layout;
  (void)count;
  (void)state;
  (void)colOffset;
  (void)rowOffset;
  return;
#else
  if (!layout) return;
#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
  if (layout==menu_pad_layouts_internal::padLayoutDancePad) return;
  colOffset=classic_pad_art::column(layout,colOffset);
  colOffset+=classic_pad_art::directionShift(layout);
  // Pixel buttons and analog gauges need not select the pad font themselves.
  // Incremental center repair must not inherit System5x7 from an axis label.
  display.setFont(ReflexPad5x7);
#endif
  home_pad_geometry::Origin origin(layout, count, ReflexPad5x7, true);

  for (uint8_t i = 0; i < count; ++i) {
    if (!isDirectionalPadGlyph(layout[i], PAD_UP_ON, PAD_UP_OFF)) continue;

    const uint8_t centerRow = layout[i].row + 1;
    const uint8_t centerCol = layout[i].col;
    bool hasDown = false;
    bool hasLeft = false;
    bool hasRight = false;
    uint32_t leftMask = 0;
    uint32_t rightMask = 0;

    for (uint8_t j = 0; j < count; ++j) {
      if (isDirectionalPadGlyph(layout[j], PAD_DOWN_ON, PAD_DOWN_OFF) &&
          layout[j].row == centerRow + 1 && layout[j].col == centerCol) {
        hasDown = true;
      } else if (isDirectionalPadGlyph(layout[j], PAD_LEFT_ON, PAD_LEFT_OFF) &&
                 layout[j].row == centerRow && layout[j].col + 6 == centerCol) {
        hasLeft = true;
        leftMask |= layout[j].mask;
      } else if (isDirectionalPadGlyph(layout[j], PAD_RIGHT_ON, PAD_RIGHT_OFF) &&
                 layout[j].row == centerRow && layout[j].col == centerCol + 6) {
        hasRight = true;
        rightMask |= layout[j].mask;
      }
    }

    if (hasDown && hasLeft && hasRight) {
      pad_dpad_graphics::drawCenter(display, centerCol + colOffset, centerRow + rowOffset,
                                    (state & leftMask) != 0, (state & rightMask) != 0);
    }
  }
#endif
}
