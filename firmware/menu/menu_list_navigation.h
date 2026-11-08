#pragma once

#include <cstdint>

// Pure list geometry shared by menus and product selectors. Even-height lists
// use the upper middle row; clamp at both ends and keep the tail at a footer.
namespace menu_list_navigation {
constexpr uint8_t kVisibleRows = 6;
constexpr uint8_t kCursorRow = 2;
constexpr uint16_t firstVisible(uint16_t cursor, uint16_t count,
                                uint8_t rows = kVisibleRows) {
  if (!rows || count <= rows) return 0;
  const uint8_t middle = (rows - 1) / 2;
  const uint16_t candidate = cursor > middle ? cursor - middle : 0;
  return candidate < count - rows ? candidate : count - rows;
}
constexpr uint16_t step(uint16_t cursor, uint16_t count, int16_t delta) {
  if (!count) return 0;
  const int32_t result = (int32_t(cursor) + delta) % count;
  return result < 0 ? result + count : result;
}
// Page jumps stop at list boundaries; single-row navigation may still wrap.
constexpr uint16_t page(uint16_t cursor, uint16_t count, bool next) {
  if (!count) return 0;
  const int32_t result = int32_t(cursor) + (next ? kVisibleRows : -kVisibleRows);
  return result < 0 ? 0 : result >= count ? count - 1 : result;
}
} // namespace menu_list_navigation
