#include "../product_config.h"

#include "menu.h"
#include "menu_helpers.h"
#include "menu_list_navigation.h"
#include "menu_playable_games_internal.h"
#include "../platform/buzzer.h"

namespace {

const char* const game_names[] = {
  "Snake",
  "Pong",
  "Breakout",
  "Flappy Bird",
  "Dino Run",
  "Space Invaders",
  "Asteroids",
  "Reaction Test"
};

constexpr uint8_t GAME_COUNT = 8;
bool games_submenu_initialized = false;
bool games_render_valid = false;
uint8_t games_render_cursor = 0;

}  // namespace

bool handleGamesSubmenu(
    uint8_t& games_submenu_cursor,
    bool& games_submenu_active,
    uint8_t selected_visible,
    uint8_t scroll_offset,
    bool bottom_right,
    bool ctrlDownJust,
    bool ctrlUpJust,
    bool ctrlAJust,
    bool ctrlRightJust,
    bool ctrlBJust,
    bool btnNavigateJustPressed,
    bool btnChangeJustPressed) {
  if (!games_submenu_active) return false;

  if (!games_submenu_initialized) {
    games_submenu_initialized = true;
    games_render_valid = false;
    display.clear();
  }

  if (!games_render_valid ||
      games_render_cursor != games_submenu_cursor) {
    display.setFont(System5x7);
  display.set1X();
  printCenteredMenuTitle(display, "Games");

  const uint8_t visible_rows = 6;
  uint8_t total_items = GAME_COUNT + 1;
  const uint8_t scroll = menu_list_navigation::firstVisible(games_submenu_cursor, total_items, visible_rows);

  for (uint8_t row = 0; row < visible_rows; row++) {
    uint8_t idx = scroll + row;
    display.setCursor(0, row + 1);

    if (idx < GAME_COUNT) {
      bool selected = (idx == games_submenu_cursor);
      display.print(selected ? ">" : " ");
      display.print(game_names[idx]);
      display.clearToEOL();
    } else if (idx == GAME_COUNT) {
      printMenuButton(display, "Back", games_submenu_cursor == GAME_COUNT);
      display.clearToEOL();
    }
  }
    games_render_cursor = games_submenu_cursor;
    games_render_valid = true;
  }

  if (ctrlDownJust || btnNavigateJustPressed) {
    if (games_submenu_cursor < GAME_COUNT) {
      games_submenu_cursor++;
      buzzer.playMenuNav();
    }
  }
  if (ctrlUpJust) {
    if (games_submenu_cursor > 0) {
      games_submenu_cursor--;
      buzzer.playMenuNav();
    }
  }

  if (ctrlAJust || btnChangeJustPressed) {
    if (games_submenu_cursor < GAME_COUNT) {
      games_submenu_active = false;
      games_submenu_initialized = false;
      closeMenu();
      playable_games_internal::beginPlayableGameDisplay();
      switch (games_submenu_cursor) {
        case 0: runPlaySnake(); break;
        case 1: runPlayPong(); break;
        case 2: runPlayBreakout(); break;
        case 3: runPlayFlappy(); break;
        case 4: runPlayDino(); break;
        case 5: runPlayInvaders(); break;
        case 6: runPlayAsteroids(); break;
        case 7: runReactionTest(); break;
      }
      isMenuOpen = true;
      mainDisplayInitialized = false;
    } else {
      games_submenu_active = false;
      games_submenu_initialized = false;
      display.clear();
      renderMenu(selected_visible, scroll_offset, bottom_right);
    }
  }

  if (ctrlBJust) {
    games_submenu_active = false;
    games_submenu_initialized = false;
    display.clear();
    renderMenu(selected_visible, scroll_offset, bottom_right);
  }

  return true;
}
