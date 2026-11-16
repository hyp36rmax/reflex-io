#include "u8g2_oled_mirror.h"

extern "C" void reflex_oled_mirror_ram_write(uint8_t col, uint8_t row, uint8_t value)
    __attribute__((weak));

namespace {

constexpr uint16_t kPanelWidth = 128;
u8x8_msg_cb forwardDisplayCallback = nullptr;

// The SSD1306 driver writes a DRAW_TILE as arg_int copies (at least one) of
// cnt tiles, left to right from column x_pos * 8 on page y_pos.
uint8_t mirrorDisplayCallback(u8x8_t* u8x8, uint8_t message, uint8_t argInt, void* argPtr) {
  if (message == U8X8_MSG_DISPLAY_DRAW_TILE && reflex_oled_mirror_ram_write) {
    const auto* tile = static_cast<const u8x8_tile_t*>(argPtr);
    const uint16_t bytes = uint16_t(tile->cnt) * 8;
    const uint8_t copies = argInt ? argInt : 1;
    uint16_t column = uint16_t(tile->x_pos) * 8;
    for (uint8_t copy = 0; copy < copies && column < kPanelWidth; ++copy) {
      for (uint16_t i = 0; i < bytes && column < kPanelWidth; ++i, ++column) {
        reflex_oled_mirror_ram_write(uint8_t(column), tile->y_pos, tile->tile_ptr[i]);
      }
    }
  }
  return forwardDisplayCallback(u8x8, message, argInt, argPtr);
}

}  // namespace

void installU8g2OledMirror(u8x8_t* u8x8) {
  if (u8x8 == nullptr || u8x8->display_cb == mirrorDisplayCallback) return;
  forwardDisplayCallback = u8x8->display_cb;
  u8x8->display_cb = mirrorDisplayCallback;
}
