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

enum class Shape : uint8_t {
  Circle, Rectangle, RoundedRectangle, LeftArrow, RightArrow, LeftPill, RightPill
};
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

// Famicom Network Controller (HVC-051), handed on as an NTT report and laid
// out as printed: the previous/next page keys (Select/Start) as arrow pills
// level with the top keypad row, the D-pad under them, the NTT keypad grid,
// End of Communication (END) under the logo on the right and B/A on the
// bottom row.
constexpr Button kFamicomNetworkButtons[] = {
  { INPUT_SELECT,  3, 0, 11, Shape::LeftPill },
  { INPUT_START,  16, 0, 11, Shape::RightPill },
  { INPUT_EXTRA1, 38, 0, kKeyWidth, Shape::Circle },
  { INPUT_EXTRA2, 50, 0, kKeyWidth, Shape::Circle },
  { INPUT_EXTRA3, 62, 0, kKeyWidth, Shape::Circle },
  { INPUT_L2,     74, 0, kKeyWidth, Shape::Circle }, // *
  { INPUT_R2,     86, 0, kKeyWidth, Shape::Circle }, // C
  { INPUT_EXTRA4, 38, 1, kKeyWidth, Shape::Circle },
  { INPUT_EXTRA5, 50, 1, kKeyWidth, Shape::Circle },
  { INPUT_EXTRA6, 62, 1, kKeyWidth, Shape::Circle },
  { INPUT_L3,     74, 1, kKeyWidth, Shape::Circle }, // #
  { INPUT_R3,     86, 1, kKeyWidth, Shape::Circle }, // .
  { INPUT_HOME,  106, 1, 11, Shape::RoundedRectangle }, // END
  { INPUT_EXTRA7, 38, 2, kKeyWidth, Shape::Circle },
  { INPUT_EXTRA8, 50, 2, kKeyWidth, Shape::Circle },
  { INPUT_EXTRA9, 62, 2, kKeyWidth, Shape::Circle },
  { INPUT_EXTRA0, 74, 2, kZeroWidth, Shape::Circle },
  { INPUT_B,    101, 2, kKeyWidth, Shape::Circle },
  { INPUT_A,    116, 2, kKeyWidth, Shape::Circle },
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
  if (button.shape == Shape::LeftPill || button.shape == Shape::RightPill) {
    // A pill as tall as the keypad circles around a small page arrow, which
    // shows as a hole in the filled pill while pressed.
    if (button.shape == Shape::RightPill) col = button.width - 1 - col;
    if (col == 0 || col + 1 == button.width) return 0x3E;
    const uint8_t tip = button.width / 2 - 1;
    const uint8_t arrow = col == tip ? 0x08 : (col == tip + 1 ? 0x1C : 0x00);
    return pressed ? static_cast<uint8_t>(0x7F & ~arrow) : static_cast<uint8_t>(0x41 | arrow);
  }
  // Seven-pixel circles and a capsule spanning both final keypad columns.
  const uint8_t edge = col < button.width / 2 ? col : button.width - 1 - col;
  constexpr uint8_t outline[] = {0x1C, 0x22, 0x41, 0x41};
  constexpr uint8_t filled[] = {0x1C, 0x3E, 0x7F, 0x7F};
  return pressed ? filled[edge < 3 ? edge : 3] : outline[edge < 3 ? edge : 3];
}

template <typename Display, unsigned N>
void drawPanel(Display& display, const Button (&buttons)[N], uint32_t state,
               uint32_t previous, bool force) {
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
  for (const auto& button : buttons) {
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

template <typename Display>
void draw(Display& display, uint32_t state, uint32_t previous, bool force) {
  drawPanel(display, kButtons, state, previous, force);
}

template <typename Display>
void drawFamicomNetwork(Display& display, uint32_t state, uint32_t previous, bool force) {
  drawPanel(display, kFamicomNetworkButtons, state, previous, force);
}

} // namespace ntt_data_home
