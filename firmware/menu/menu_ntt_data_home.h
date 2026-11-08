#pragma once

#include <cstdint>
#include "../input/shared/input_button_bits.h"
#include "menu_dpad_graphics.h"
#include "oled_pixel_surface.h"

// A full-width, one-player-only panel. Compact multiplayer PadButton layouts
// remain limited to ten characters; this panel owns only OLED pages 3 through 6.
namespace ntt_data_home {

constexpr uint8_t kFirstPage = 3;
constexpr uint8_t kLastPage = 6;
constexpr uint8_t kKeyWidth = 7;
constexpr uint8_t kKeyPitch = 12;
constexpr uint8_t kZeroWidth = kKeyPitch + kKeyWidth;
constexpr uint8_t kDpadLeft = 6;
constexpr uint8_t kDpadTop = kFirstPage + 1;
constexpr uint32_t kDpadMask = INPUT_PAD_U | INPUT_PAD_D | INPUT_PAD_L | INPUT_PAD_R;

enum class Shape : uint8_t { Circle, Rectangle, RoundedRectangle, LeftArrow, RightArrow };
struct Button {
  uint32_t mask;
  uint8_t x;
  uint8_t page;
  uint8_t width;
  Shape shape;
};

constexpr Button kButtons[] = {
  { INPUT_L1,      9, 0, 13, Shape::Rectangle },
  { INPUT_R1,    110, 0, 13, Shape::Rectangle },
  { INPUT_SELECT, 50, 0,  7, Shape::LeftArrow },
  { INPUT_START,  62, 0,  7, Shape::RightArrow },
  { INPUT_HOME,   84, 0, 11, Shape::RoundedRectangle }, // communication end
  // Keypad, following the physical NTT Data rows exactly:
  // 1 2 3 * C / 4 5 6 # . / 7 8 9 [ 0 ].
  { INPUT_EXTRA1, 38, 1, kKeyWidth, Shape::Circle },
  { INPUT_EXTRA2, 50, 1, kKeyWidth, Shape::Circle },
  { INPUT_EXTRA3, 62, 1, kKeyWidth, Shape::Circle },
  { INPUT_L2,     74, 1, kKeyWidth, Shape::Circle }, // *
  { INPUT_R2,     86, 1, kKeyWidth, Shape::Circle }, // C
  { INPUT_EXTRA4, 38, 2, kKeyWidth, Shape::Circle },
  { INPUT_EXTRA5, 50, 2, kKeyWidth, Shape::Circle },
  { INPUT_EXTRA6, 62, 2, kKeyWidth, Shape::Circle },
  { INPUT_L3,     74, 2, kKeyWidth, Shape::Circle }, // #
  { INPUT_R3,     86, 2, kKeyWidth, Shape::Circle }, // .
  { INPUT_EXTRA7, 38, 3, kKeyWidth, Shape::Circle },
  { INPUT_EXTRA8, 50, 3, kKeyWidth, Shape::Circle },
  { INPUT_EXTRA9, 62, 3, kKeyWidth, Shape::Circle },
  { INPUT_EXTRA0, 74, 3, kZeroWidth, Shape::Circle },
  { INPUT_X,    108, 1, kKeyWidth, Shape::Circle },
  { INPUT_Y,    100, 2, kKeyWidth, Shape::Circle },
  { INPUT_A,    116, 2, kKeyWidth, Shape::Circle },
  { INPUT_B,    108, 3, kKeyWidth, Shape::Circle },
};

inline uint8_t buttonColumn(const Button& button, uint8_t col, bool pressed) {
  if (button.shape == Shape::Rectangle || button.shape == Shape::RoundedRectangle) {
    const bool edge = col == 0 || col + 1 == button.width;
    if (button.shape == Shape::RoundedRectangle && edge) return 0x1C;
    return pressed || edge ? 0x3E : 0x22;
  }
  if (button.shape == Shape::LeftArrow || button.shape == Shape::RightArrow) {
    if (button.shape == Shape::RightArrow) col = button.width - 1 - col;
    constexpr uint8_t outline[] = {0x08, 0x14, 0x22, 0x41, 0x41, 0x41, 0x7F};
    constexpr uint8_t filled[] = {0x08, 0x1C, 0x3E, 0x7F, 0x7F, 0x7F, 0x7F};
    return pressed ? filled[col] : outline[col];
  }
  // Seven-pixel circles and a capsule spanning both final keypad columns.
  const uint8_t edge = col < button.width / 2 ? col : button.width - 1 - col;
  constexpr uint8_t outline[] = {0x1C, 0x22, 0x41, 0x41};
  constexpr uint8_t filled[] = {0x1C, 0x3E, 0x7F, 0x7F};
  return pressed ? filled[edge < 3 ? edge : 3] : outline[edge < 3 ? edge : 3];
}

template <typename Display>
void draw(Display& display, uint32_t state, uint32_t previous, bool force) {
  if (force) {
#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
    if (oled_pixel::homeSurface() && oled_pixel::homeSurface()->active) {
      // A previous pad can reach either edge of the expanded Home region.
      // Clear the whole region, not just the NTT panel's centered glyph rows.
      oled_pixel::Origin clearOrigin(0, 25, 63);
      display.clear(0, 127, kFirstPage, 7);
    } else
#endif
    display.clear(0, 127, kFirstPage, kLastPage);
  }
  oled_pixel::Origin origin(oled_pixel::centerOffset(24, 55));
  for (const auto& button : kButtons) {
    if (!force && ((state ^ previous) & button.mask) == 0) continue;
    display.setCursor(button.x, kFirstPage + button.page);
    for (uint8_t col = 0; col < button.width; ++col) {
      display.ssd1306WriteRamBuf(buttonColumn(button, col, (state & button.mask) != 0));
    }
  }
  if (force || ((state ^ previous) & kDpadMask) != 0) {
    pad_dpad_graphics::draw(display, kDpadLeft, kDpadTop,
        (state & INPUT_PAD_U) != 0, (state & INPUT_PAD_D) != 0,
        (state & INPUT_PAD_L) != 0, (state & INPUT_PAD_R) != 0);
  }
}

} // namespace ntt_data_home
