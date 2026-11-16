#pragma once

#include <clib/u8x8.h>

// U8g2 sends its frames through its own SSD1306 driver, so they bypass the
// SSD1306Ascii hook that feeds the serial OLED frame (OLED FRAME, Adapt.html's
// live view). This wraps a U8g2 display callback so every tile it draws is
// copied there too. Call once, after the U8g2 object is constructed.
void installU8g2OledMirror(u8x8_t* u8x8);
