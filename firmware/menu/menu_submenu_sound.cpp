#include "../product_config.h"

#include "menu.h"
#include "menu_helpers.h"
#include "menu_list_navigation.h"
#include "../core/settings_store.h"
#include "../platform/buzzer.h"
#include "../platform/buzzer_audition.h"

namespace {

const char* const buzzer_mode_names[] = { "Off", "Low", "High" };
bool sound_submenu_initialized = false;
bool boot_jingle_test_active = false;
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
bool sound_hold_snapshot_valid = false;
uint8_t sound_hold_original_mode = 0;
uint16_t sound_hold_original_events = 0;
#endif
uint8_t boot_jingle_test_cursor = 0;

struct SoundRenderSnapshot {
  bool valid;
  bool boot_jingle;
  uint8_t cursor;
  uint8_t mode;
  uint16_t events;
  uint8_t jingle_cursor;
};

SoundRenderSnapshot sound_render_snapshot{};

bool soundRenderSnapshotMatches(uint8_t cursor, uint8_t mode,
                                uint16_t events) {
  return sound_render_snapshot.valid &&
         sound_render_snapshot.boot_jingle == boot_jingle_test_active &&
         sound_render_snapshot.cursor == cursor &&
         sound_render_snapshot.mode == mode &&
         sound_render_snapshot.events == events &&
         sound_render_snapshot.jingle_cursor == boot_jingle_test_cursor;
}

void captureSoundRenderSnapshot(uint8_t cursor, uint8_t mode,
                                uint16_t events) {
  sound_render_snapshot = {
      true, boot_jingle_test_active, cursor, mode, events,
      boot_jingle_test_cursor};
}

void invalidateSoundRenderSnapshot() {
  sound_render_snapshot.valid = false;
}

void applyMenuBuzzerMode(uint8_t mode, uint16_t soundEvents) {
  buzzer.setEnabled(mode > 0);
  buzzer.setVolume((mode == 1) ? 10 : 100);
  buzzer.setEventMask(soundEvents);
}

void closeBootJingleTest() {
  buzzer.stop();
  boot_jingle_test_active = false;
  boot_jingle_test_cursor = 0;
  sound_submenu_initialized = false;
  invalidateSoundRenderSnapshot();
  display.clear();
}

void renderBootJingleTest() {
  display.setFont(System5x7);
  display.set1X();
  printCenteredMenuTitle(display, "Boot Jingle Test");

  const uint8_t visibleRows = 6;
  const uint8_t totalItems = buzzerAuditionCandidateCount() + 1;
  const uint8_t scroll = menu_list_navigation::firstVisible(boot_jingle_test_cursor, totalItems, visibleRows);

  for (uint8_t row = 0; row < visibleRows; ++row) {
    const uint8_t index = scroll + row;
    display.setCursor(0, row + 1);
    if (index < buzzerAuditionCandidateCount()) {
      display.print(index == boot_jingle_test_cursor ? ">" : " ");
      display.print(buzzerAuditionCandidateName(index));
    } else if (index == buzzerAuditionCandidateCount()) {
      printMenuButton(display, "Back", index == boot_jingle_test_cursor);
    }
    display.clearToEOL();
  }

  display.setCursor(0, 7);
  display.print(F("Select=Play"));
  display.clearToEOL();
  if (scroll > 0) {
    display.setCursor(120, 7);
    display.print("^");
  } else if (scroll + visibleRows < totalItems) {
    display.setCursor(120, 7);
    display.print("v");
  }
}

}  // namespace

bool finishSoundSubmenuForHold(bool save) {
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  if (!sound_submenu_active) return false;
  if (!save && sound_hold_snapshot_valid) {
    menu_buzzer_mode = sound_hold_original_mode;
    menu_sound_events = sound_hold_original_events;
  }
  buzzer.stop();
  applyMenuBuzzerMode(menu_buzzer_mode, menu_sound_events);
  if (save) saveSoundSettings(menu_buzzer_mode, menu_sound_events);
  sound_submenu_active = false;
  sound_submenu_initialized = false;
  sound_hold_snapshot_valid = false;
  boot_jingle_test_active = false;
  boot_jingle_test_cursor = 0;
  invalidateSoundRenderSnapshot();
  return true;
#else
  (void)save;
  return false;
#endif
}

bool handleSoundSubmenu(
    uint8_t& sound_submenu_cursor,
    bool& sound_submenu_active,
    uint8_t& menu_buzzer_mode,
    uint16_t& menu_sound_events,
    uint8_t selected_visible,
    uint8_t scroll_offset,
    bool bottom_right,
    bool ctrlDownJust,
    bool ctrlUpJust,
    bool ctrlLeftJust,
    bool ctrlAJust,
    bool ctrlRightJust,
    bool ctrlBJust,
    bool btnNavigateJustPressed,
    bool btnChangeJustPressed) {
  if (!sound_submenu_active) {
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
    sound_hold_snapshot_valid = false;
#endif
    sound_submenu_initialized = false;
    boot_jingle_test_active = false;
    boot_jingle_test_cursor = 0;
    invalidateSoundRenderSnapshot();
    return false;
  }

  if (!sound_submenu_initialized) {
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
    if (!sound_hold_snapshot_valid) {
      sound_hold_original_mode = menu_buzzer_mode;
      sound_hold_original_events = menu_sound_events;
      sound_hold_snapshot_valid = true;
    }
#endif
    sound_submenu_initialized = true;
    invalidateSoundRenderSnapshot();
    display.clear();
  }

  if (boot_jingle_test_active) {
    if (!soundRenderSnapshotMatches(
            sound_submenu_cursor, menu_buzzer_mode, menu_sound_events)) {
      renderBootJingleTest();
      captureSoundRenderSnapshot(
          sound_submenu_cursor, menu_buzzer_mode, menu_sound_events);
    }
    const uint8_t totalItems = buzzerAuditionCandidateCount() + 1;

    if ((ctrlDownJust || btnNavigateJustPressed) &&
        boot_jingle_test_cursor < totalItems - 1) {
      ++boot_jingle_test_cursor;
      buzzer.playMenuNav();
    }
    if (ctrlUpJust && boot_jingle_test_cursor > 0) {
      --boot_jingle_test_cursor;
      buzzer.playMenuNav();
    }

    if (ctrlAJust || btnChangeJustPressed) {
      if (boot_jingle_test_cursor < buzzerAuditionCandidateCount()) {
        const buzzer_melody_t* melody =
            buzzerAuditionCandidateMelody(boot_jingle_test_cursor);
        if (melody) buzzer.play(*melody);
      } else {
        closeBootJingleTest();
      }
    }
    if (ctrlBJust) {
      closeBootJingleTest();
    }
    return true;
  }

  const uint8_t total_items = SOUND_EVENT_COUNT + 3;
  if (!soundRenderSnapshotMatches(
          sound_submenu_cursor, menu_buzzer_mode, menu_sound_events)) {
    display.setFont(System5x7);
    display.set1X();
    printCenteredMenuTitle(display, "Sound Events");

  const uint8_t visible_rows = 6;
  const uint8_t scroll = menu_list_navigation::firstVisible(sound_submenu_cursor, total_items, visible_rows);

  for (uint8_t row = 0; row < visible_rows; row++) {
    uint8_t idx = scroll + row;
    display.setCursor(0, row + 1);

    if (idx == 0) {
      bool selected = (sound_submenu_cursor == 0);
      display.print(selected ? ">" : " ");
      display.print(F("Mode"));
      display.clearToEOL();

      display.setCursor(98, row + 1);
      display.print(buzzer_mode_names[menu_buzzer_mode <= 2 ? menu_buzzer_mode : 0]);
    } else if (idx == 1) {
      const bool selected = (sound_submenu_cursor == 1);
      display.print(selected ? ">" : " ");
      display.print(F("Boot Jingles >"));
      display.clearToEOL();
    } else if (idx <= SOUND_EVENT_COUNT + 1) {
      const uint8_t soundIdx = idx - 2;
      bool selected = (idx == sound_submenu_cursor);
      bool enabled = (menu_sound_events & sound_event_items[soundIdx].mask) != 0;

      display.print(selected ? ">" : " ");
      display.print(sound_event_items[soundIdx].name);
      display.clearToEOL();

      display.setCursor(100, row + 1);
      display.print(enabled ? "On" : "Off");
    } else if (idx == SOUND_EVENT_COUNT + 2) {
      printMenuButton(display, "Back", sound_submenu_cursor == SOUND_EVENT_COUNT + 2);
      display.clearToEOL();
    }
  }

    display.clear(120, 127, 7, 7);
    if (total_items > visible_rows) {
      display.setCursor(120, 7);
      if (scroll > 0) display.print("^");
      if (scroll + visible_rows < total_items) display.print("v");
    }
    captureSoundRenderSnapshot(
        sound_submenu_cursor, menu_buzzer_mode, menu_sound_events);
  }

  if (ctrlDownJust || btnNavigateJustPressed) {
    if (sound_submenu_cursor < total_items - 1) {
      sound_submenu_cursor++;
      buzzer.playMenuNav();
    }
  }
  if (ctrlUpJust) {
    if (sound_submenu_cursor > 0) {
      sound_submenu_cursor--;
      buzzer.playMenuNav();
    }
  }

  const bool selectedValue = sound_submenu_cursor == 0 ||
                             (sound_submenu_cursor >= 2 &&
                              sound_submenu_cursor <= SOUND_EVENT_COUNT + 1);
  const bool activate = ctrlAJust || btnChangeJustPressed;
  const bool adjust = selectedValue && (ctrlRightJust || ctrlLeftJust);
  if (activate || adjust) {
    if (sound_submenu_cursor == 0) {
      menu_buzzer_mode = ctrlLeftJust
        ? (menu_buzzer_mode + 2) % 3
        : (menu_buzzer_mode + 1) % 3;
      applyMenuBuzzerMode(menu_buzzer_mode, menu_sound_events);
      buzzer.playMenuNav();
    } else if (sound_submenu_cursor == 1) {
      if (menu_buzzer_mode == 0) {
        menu_buzzer_mode = 1;
        applyMenuBuzzerMode(menu_buzzer_mode, menu_sound_events);
      }
      boot_jingle_test_active = true;
      boot_jingle_test_cursor = 0;
      sound_submenu_initialized = false;
      buzzer.playMenuEnter();
    } else if (sound_submenu_cursor <= SOUND_EVENT_COUNT + 1) {
      menu_sound_events ^= sound_event_items[sound_submenu_cursor - 2].mask;
      applyMenuBuzzerMode(menu_buzzer_mode, menu_sound_events);
      buzzer.playMenuNav();
    } else {
      applyMenuBuzzerMode(menu_buzzer_mode, menu_sound_events);
      saveSoundSettings(menu_buzzer_mode, menu_sound_events);
      buzzer.playSave();
      sound_submenu_active = false;
      sound_submenu_initialized = false;
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
      sound_hold_snapshot_valid = false;
#endif
      display.clear();
      renderMenu(selected_visible, scroll_offset, bottom_right);
    }
  }

  if (ctrlBJust) {
    applyMenuBuzzerMode(menu_buzzer_mode, menu_sound_events);
    saveSoundSettings(menu_buzzer_mode, menu_sound_events);
    buzzer.playSave();
    sound_submenu_active = false;
    sound_submenu_initialized = false;
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
    sound_hold_snapshot_valid = false;
#endif
    display.clear();
    renderMenu(selected_visible, scroll_offset, bottom_right);
  }

  return true;
}
