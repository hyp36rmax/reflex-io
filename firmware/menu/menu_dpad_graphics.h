#pragma once

#include <stdint.h>

// Shared direction and center tiles in the ReflexPad5x7 font.
#define PAD_UP_ON 34
#define PAD_UP_OFF 35
#define PAD_DOWN_ON 36
#define PAD_DOWN_OFF 37
#define PAD_LEFT_ON 38
#define PAD_LEFT_OFF 39
#define PAD_RIGHT_ON 40
#define PAD_RIGHT_OFF 41
#define PAD_DPAD_CENTER '*'

namespace pad_dpad_graphics {

// Caller selects ReflexPad5x7. Keep the connector columns pressed/released
// along with the side tiles, including incremental redraws after a release.
template <typename Display>
void drawCenter(Display& display, uint8_t centerCol, uint8_t centerRow,
                bool leftPressed, bool rightPressed) {
  display.setCursor(centerCol, centerRow);
  display.print(PAD_DPAD_CENTER);
  if (centerCol > 0) {
    display.setCursor(centerCol - 1, centerRow);
    display.ssd1306WriteRamBuf(leftPressed ? 0x7F : 0x41);
  }
  if (centerCol + 5 < 128) {
    display.setCursor(centerCol + 5, centerRow);
    display.ssd1306WriteRamBuf(rightPressed ? 0x7F : 0x41);
  }
}

// Same 6x8 tile geometry as the standard controller layouts (18x24 total).
template <typename Display>
void draw(Display& display, uint8_t leftCol, uint8_t topRow,
          bool up, bool down, bool left, bool right) {
  display.setCursor(leftCol + 6, topRow);
  display.print(static_cast<char>(up ? PAD_UP_ON : PAD_UP_OFF));
  display.setCursor(leftCol + 6, topRow + 2);
  display.print(static_cast<char>(down ? PAD_DOWN_ON : PAD_DOWN_OFF));
  display.setCursor(leftCol, topRow + 1);
  display.print(static_cast<char>(left ? PAD_LEFT_ON : PAD_LEFT_OFF));
  display.setCursor(leftCol + 12, topRow + 1);
  display.print(static_cast<char>(right ? PAD_RIGHT_ON : PAD_RIGHT_OFF));
  drawCenter(display, leftCol + 6, topRow + 1, left, right);
}

} // namespace pad_dpad_graphics
