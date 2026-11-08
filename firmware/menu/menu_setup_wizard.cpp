#include "../product_config.h"

#include "menu_setup_wizard.h"

#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)

#include <stdio.h>

#include "menu.h"
#include "menu_helpers.h"
#include "menu_runtime_state.h"
#include "menu_ui_state.h"
#include "menu_working_state.h"
#include "screensaver_animation.h"
#include "setup_wizard_flow.h"
#include "setup_button_tour.h"
#include "menu_input_mode.h"
#include "menu_mode_state.h"
#include "../core/controller_frame_state.h"
#include "../core/controller_settings_state.h"
#include "../core/device_runtime_state.h"
#include "../core/settings_registry.h"
#include "../core/settings_store.h"
#include "../output/output_runtime_state.h"
#include "../platform/buzzer.h"
#include "../platform/display_runtime_state.h"

namespace {

using namespace setup_wizard;

// Rows: title, prompt, options, then the adapter-button hint on the last row.
constexpr uint8_t kHintRow = 7;
constexpr uint8_t kOptionRow = 2;
constexpr uint8_t kWelcomeOptionRow = 4;
constexpr uint8_t kPadHintRow = 6;

enum PendingState : uint8_t { PendingUnknown, PendingNo, PendingYes };

State wizard{};
Config config{};
setup_button_tour::State tour{};
MenuModeVisibilityRecord wizardVisibility{};
bool wizardActive = false;
bool offeredThisBoot = false;
PendingState pendingState = PendingUnknown;

// Values previewed live; put back if the wizard is left without saving.
struct Preview {
  uint8_t okBack;
  uint8_t contrast;
  uint8_t buzzerMode;
};
Preview beforeWizard{};

struct Rendered {
  bool valid;
  Step step;
  uint8_t cursor;
  bool pad;
};
Rendered rendered{};

const char* const kOkBackLabels[] = {
  "PS X, Nintendo A", "Right face button", "Bottom face button"};
const char* const kBrightnessLabels[] = {"25%", "50%", "75%", "100%"};
const char* const kSoundLabels[] = {"Off", "Low", "High"};
const char* const kIdleTimerLabels[] = {
  "Never", "1 minute", "5 minutes", "10 minutes"};
const char* const kIdleActionLabels[] = {
  "Screensaver", "Dim screen", "Blank screen"};
const char* const kOffOnLabels[] = {"Off", "On"};
const char* const kWelcomeLabels[] = {"Guided setup", "Skip"};
const char* const kFinishLabels[] = {"Save and finish", "Change something"};

const char* optionLabel(Step step, uint8_t index) {
  switch (step) {
    case StepWelcome: return kWelcomeLabels[index];
    case StepOkBack: return kOkBackLabels[index];
    case StepBrightness: return kBrightnessLabels[index];
    case StepSound: return kSoundLabels[index];
    case StepIdleTimer: return kIdleTimerLabels[index];
    case StepIdleAction: return kIdleActionLabels[index];
    case StepAnimation:
      return screensaver_anim_names[screensaver_enabled_anim_ids[index]];
    case StepHotkeys: return kOffOnLabels[index];
    case StepManualInputs: return index == config.inputCount ? "Continue" : getInputModeName(menuInputChoiceAt(index, true));
    case StepManualOutputs: return index == config.outputCount ? "Continue" : get_mode_name(menuOutputChoiceAt(index, true));
    case StepFinish: return kFinishLabels[index];
    default: return "";
  }
}

const char* promptFor(Step step) {
  switch (step) {
    case StepOkBack: return "Menu OK button";
    case StepBrightness: return "Screen brightness";
    case StepSound: return "Sound";
    case StepIdleTimer: return "Screen idle after";
    case StepIdleAction: return "When idle, show";
    case StepAnimation: return "Screensaver";
    case StepHotkeys: return "Start+D-pad hotkeys";
    case StepManualInputs: return "Show manual inputs";
    case StepManualOutputs: return "Show manual outputs";
    case StepFinish: return "Save these settings?";
    default: return "";
  }
}

bool padConnected() {
  return max_devices > 0 && controllerFrameConst(0).connected;
}

void applyBuzzerMode(uint8_t mode) {
  buzzer_enabled = mode > 0;
  buzzer.setEnabled(buzzer_enabled);
  buzzer.setVolume((mode == 1) ? 10 : 100);
}

uint8_t animationIndexFor(uint8_t animation) {
  for (uint8_t i = 0; i < SCREENSAVER_ENABLED_ANIM_COUNT; ++i) {
    if (screensaver_enabled_anim_ids[i] == animation) return i;
  }
  return 0;
}

void loadChoices(const GlobalSettingsRecord& record) {
  Choices& c = wizard.choices;
  c.okBack = (uint8_t)readRecordSettingValue(record, SettingId::MenuOkBack);
  c.brightness = brightnessIndexForContrast(
      (uint8_t)readRecordSettingValue(record, SettingId::DisplayContrast));
  const uint8_t sound = (uint8_t)readRecordSettingValue(record, SettingId::BuzzerMode);
  c.sound = sound < kSoundCount ? sound : 0;
  const uint8_t mode = (uint8_t)readRecordSettingValue(record, SettingId::ScreensaverMode);
  c.idleTimer = screensaverModeTimer(mode);
  c.idleAction = readRecordSettingValue(record, SettingId::OledInactive) ? IdleBlank
               : screensaverModeDims(mode) ? IdleDim : IdleScreensaver;
  c.animation = animationIndexFor(
      (uint8_t)readRecordSettingValue(record, SettingId::ScreensaverAnimation));
  c.hotkeys = readRecordSettingValue(record, SettingId::HomeHotkey) ? 1 : 0;
}

void storeChoices(GlobalSettingsRecord& record) {
  const Choices& c = wizard.choices;
  writeRecordSettingValue(record, SettingId::MenuOkBack, c.okBack);
  writeRecordSettingValue(record, SettingId::DisplayContrast, kBrightnessLevels[c.brightness]);
  writeRecordSettingValue(record, SettingId::BuzzerMode, c.sound);
  writeRecordSettingValue(record, SettingId::ScreensaverMode,
                          encodeScreensaverMode(c.idleTimer, c.idleAction == IdleDim));
  writeRecordSettingValue(record, SettingId::OledInactive, c.idleAction == IdleBlank ? 1 : 0);
  writeRecordSettingValue(record, SettingId::ScreensaverAnimation,
                          screensaver_enabled_anim_ids[c.animation]);
  writeRecordSettingValue(record, SettingId::HomeHotkey, c.hotkeys);
}

// Nothing reloads these after a save, so the saved record becomes the running
// configuration here, along with the System menu's working copies.
void applyRecordToRuntime(const GlobalSettingsRecord& record) {
  menu_ok_back = (uint8_t)readRecordSettingValue(record, SettingId::MenuOkBack);
  menu_menu_ok_back = menu_ok_back;
  display_contrast = (uint8_t)readRecordSettingValue(record, SettingId::DisplayContrast);
  menu_display_contrast = display_contrast;
  display.setContrast(display_contrast);
  menu_buzzer_mode = (uint8_t)readRecordSettingValue(record, SettingId::BuzzerMode);
  applyBuzzerMode(menu_buzzer_mode);
  screensaver_mode = (uint8_t)readRecordSettingValue(record, SettingId::ScreensaverMode);
  menu_screensaver = screensaver_mode;
  screensaver_animation =
      (uint8_t)readRecordSettingValue(record, SettingId::ScreensaverAnimation);
  menu_screensaver_anim = screensaver_animation;
  oled_inactive_mode = (uint8_t)readRecordSettingValue(record, SettingId::OledInactive);
  menu_oled_inactive = oled_inactive_mode;
  menu_home_hotkey = (uint8_t)readRecordSettingValue(record, SettingId::HomeHotkey);
  resetAnimationState();
}

void restorePreviews() {
  menu_ok_back = beforeWizard.okBack;
  display_contrast = beforeWizard.contrast;
  display.setContrast(display_contrast);
  applyBuzzerMode(beforeWizard.buzzerMode);
}

void applyPreview() {
  switch (wizard.step) {
    case StepOkBack:
      menu_ok_back = wizard.choices.okBack;
      break;
    case StepBrightness:
      display_contrast = kBrightnessLevels[wizard.choices.brightness];
      display.setContrast(display_contrast);
      break;
    case StepSound:
      applyBuzzerMode(wizard.choices.sound);
      // Menu-navigation beeps are masked by default; play the level itself.
      buzzer.play(MELODY_CONNECT);
      break;
    default:
      break;
  }
}

void printRow(uint8_t row, const char* text) {
  display.setCursor(0, row);
  display.print(text);
  display.clearToEOL();
}

void renderTour() {
  using namespace setup_button_tour;
  display.setFont(System5x7);
  display.set1X();
  if (tour.intro) {
    printRow(0, "LEFT            RIGHT");
    printRow(2, "Meet the buttons");
    printRow(3, "On top of the adapter");
    printRow(5, "Tap RIGHT to begin");
    printRow(7, "Practice only");
    return;
  }
  printCenteredMenuTitle(display, "Button tour");
  const char* prompts[] = {"Tap LEFT to scroll", "Tap RIGHT to toggle",
                          "Hold LEFT to cancel", "Hold RIGHT to save",
                          "Hold BOTH: defaults", "Practice complete!"};
  printRow(1, prompts[tour.lesson]);
#if ADAPT_POLICY_RESET_HOLD_AUTO_OUTPUT
  if (tour.lesson == Finished) {
    printRow(2, "Home: hold LEFT 3s");
    printRow(3, "Input+Output: Auto");
    printRow(4, "Other settings kept");
    printRow(5, "");
    printRow(6, "");
    printRow(7, "Tap RIGHT for setup");
    return;
  }
#endif
  printRow(2, tour.lesson == Save ? "No changes needed" :
              tour.lesson == Finished ? "Real reset asks first" : "Fake settings only");
  const char* names[] = {"Demo lights", "Demo sound", "Demo option"};
  char line[22];
  for (uint8_t i = 0; i < 3; ++i) {
    snprintf(line, sizeof(line), "%c%s: %s", tour.cursor == i ? '>' : ' ',
             names[i], (tour.edited & (1u << i)) ? "On" : "Off");
    printRow(3 + i, line);
  }
  printRow(6, tour.lesson == Defaults ? "Real reset asks first" : "Holds take 1.5 sec");
  printRow(7, tour.lesson == Finished ? "Tap RIGHT for setup" : "Try it on the device");
}

void renderWizard(bool pad) {
  display.setFont(System5x7);
  display.set1X();

  char line[22];
  if (wizard.step == StepWelcome) {
    snprintf(line, sizeof(line), "Welcome");
  } else if (wizard.step == StepFinish) {
    snprintf(line, sizeof(line), "All set");
  } else {
    snprintf(line, sizeof(line), "Setup %u/%u",
             (unsigned)stepNumber(wizard, config), (unsigned)stepTotal(wizard, config));
  }
  printCenteredMenuTitle(display, line);

  uint8_t firstOptionRow = kOptionRow;
  if (wizard.step == StepWelcome) {
    printRow(1, "Learn menu buttons,");
    printRow(2, "then set preferences.");
    printRow(3, "Change them any time.");
    firstOptionRow = kWelcomeOptionRow;
  } else {
    printRow(1, promptFor(wizard.step));
  }

  const bool modeList = wizard.step == StepManualInputs || wizard.step == StepManualOutputs;
  const uint8_t rows = (modeList ? kPadHintRow : kHintRow) - firstOptionRow;
  const uint8_t count = optionCount(wizard.step, config);
  const uint8_t scroll = scrollFor(wizard.cursor, count, rows);
  for (uint8_t r = 0; r < rows; ++r) {
    const uint8_t option = scroll + r;
    display.setCursor(0, firstOptionRow + r);
    if (option < count) {
      display.print(option == wizard.cursor ? ">" : " ");
      const bool inputList = wizard.step == StepManualInputs;
      const bool modeList = inputList || wizard.step == StepManualOutputs;
      const bool toggle = modeList && option < (inputList ? config.inputCount : config.outputCount);
      if (toggle) {
        const uint8_t mode = inputList ? (uint8_t)menuInputChoiceAt(option, true) : (uint8_t)menuOutputChoiceAt(option, true);
        const uint32_t mask = inputList ? wizardVisibility.hidden_input_modes : wizardVisibility.hidden_output_modes;
        display.print((mask & (uint32_t(1) << mode)) ? "[ ]" : "[x]");
      }
      const char* label = optionLabel(wizard.step, option);
      for (uint8_t c = 0; label[c] && c < (toggle ? 17 : 20); ++c) display.print(label[c]);
    }
    display.clearToEOL();
  }
  if (wizard.step == StepOkBack && pad) {
    // The highlighted layout is live, so this names the attached pad's buttons.
    snprintf(line, sizeof(line), "Pad: OK=%s Back=%s",
             getMenuConfirmButtonName(deviceMode), getMenuBackButtonName(deviceMode));
    printRow(kPadHintRow, line);
  }
  if (wizard.step == StepManualInputs || wizard.step == StepManualOutputs) {
    printRow(6, "Auto stays available");
  }
  printRow(kHintRow, "Left=Next  Right=OK");
}

bool commitWizard(bool withChoices) {
  GlobalSettingsRecord record{};
  (void)readGlobalSettings(record);
  if (withChoices) {
    storeChoices(record);
    sanitizeGlobalSettings(record);
  }
  const bool saved = withChoices ? saveFirstBootSetupSettings(record, wizardVisibility) : saveFirstBootSetupSettings(record);
  if (!saved) {
    return false;
  }
  pendingState = PendingNo;
  if (withChoices) {
    applyRecordToRuntime(record);
  }
  return true;
}

void leaveWizard(bool save) {
  const bool stored = commitWizard(save);
  if (!save || !stored) {
    // Only stored choices stay in effect; a failed write keeps none of them.
    restorePreviews();
  }
  wizardActive = false;
  rendered.valid = false;
  closeMenu();
  // A slow last step must not hand Home straight to the screensaver.
  resetIdleTimer(IdleWakeSource::Menu);
  if (!stored) {
    // Setup is still pending, so it is offered again on the next boot.
    buzzer.playError();
    showMenuActionFeedback(MenuActionFeedback::SaveFailed);
  } else if (save) {
    buzzer.playSave();
    showMenuActionFeedback(MenuActionFeedback::Saved);
  } else {
    forceMainDisplayRefresh();
  }
}

}  // namespace

bool setupWizardShouldOpen() {
  if (offeredThisBoot || wizardActive || !mainDisplayInitialized) {
    return false;
  }
  if (pendingState == PendingUnknown) {
    pendingState = firstBootSetupPending() ? PendingYes : PendingNo;
  }
  if (pendingState != PendingYes) {
    offeredThisBoot = true;
    return false;
  }
  // Host detection reboots once it identifies the USB host; wait for it so
  // the wizard is not cut off part way through.
  return !(is_auto_output_mode_selected() && autoDetectState == AUTO_STATE_IDLE &&
           !auto_detect_no_data_host_fallback_active());
}

void beginSetupWizard() {
  GlobalSettingsRecord record{};
  (void)readGlobalSettings(record);
  wizard = State{};
  tour = setup_button_tour::State{};
  loadChoices(record);
  config.hasBuzzer = settingSupportedOnProduct(SettingId::BuzzerMode);
  config.animationCount = SCREENSAVER_ENABLED_ANIM_COUNT;
  config.inputCount = menuInputChoiceCount(true);
  config.outputCount = menuOutputChoiceCount(true);
  (void)readMenuModeVisibility(wizardVisibility);
  beforeWizard.okBack = menu_ok_back;
  beforeWizard.contrast = display_contrast;
  beforeWizard.buzzerMode = (uint8_t)readRecordSettingValue(record, SettingId::BuzzerMode);
  enterStep(wizard, StepWelcome, config);
  rendered.valid = false;
  wizardActive = true;
  offeredThisBoot = true;
}

bool setupWizardActive() {
  return wizardActive;
}

bool handleSetupWizardTutorialButtons(bool left, bool right,
                                     bool leftTap, bool rightTap, uint32_t now) {
  if (!wizardActive || !tour.active) return false;
  const auto previous = tour;
  const bool finished = setup_button_tour::update(tour, left, right, leftTap, rightTap, now);
  if (finished) {
    rendered.valid = false;
    applyPreview();
  } else if (previous.intro != tour.intro || previous.lesson != tour.lesson || previous.cursor != tour.cursor ||
             previous.edited != tour.edited || !rendered.valid) {
    display.clear();
    renderTour();
    rendered.valid = true;
  }
  return true;
}

void cancelSetupWizard() {
  if (!wizardActive) {
    return;
  }
  restorePreviews();
  tour.active = false;
  wizardActive = false;
  rendered.valid = false;
}

bool finishSetupWizard() {
  if (!wizardActive || tour.active) {
    return false;
  }
  leaveWizard(true);
  return pendingState == PendingNo;
}

bool handleSetupWizard(const MenuControllerInput& ctrl,
                       bool btnNavigateJustPressed,
                       bool btnChangeJustPressed) {
  if (!wizardActive) {
    return false;
  }
  if (tour.active) return true;

  const bool pad = padConnected();
  if (!rendered.valid || rendered.step != wizard.step ||
      rendered.cursor != wizard.cursor || rendered.pad != pad) {
    if (!rendered.valid || rendered.step != wizard.step) {
      display.clear();
    }
    renderWizard(pad);
    rendered = {true, wizard.step, wizard.cursor, pad};
  }

  // Until a layout is chosen either face button accepts. After that the
  // chosen OK accepts and Back returns to the previous step.
  const bool undecided = wizard.step == StepWelcome || wizard.step == StepOkBack;
  const bool acceptPressed = btnChangeJustPressed || ctrl.aJust || ctrl.startJust ||
                             (undecided && ctrl.bJust);
  const bool backPressed = !undecided && ctrl.bJust;

  Result result = ResultNone;
  if (btnNavigateJustPressed || ctrl.downJust) {
    result = move(wizard, config, true);
  } else if (ctrl.upJust) {
    result = move(wizard, config, false);
  } else if (acceptPressed) {
    const bool inputList = wizard.step == StepManualInputs;
    const bool outputList = wizard.step == StepManualOutputs;
    if ((inputList || outputList) && wizard.cursor < (inputList ? config.inputCount : config.outputCount)) {
      const uint8_t mode = inputList ? (uint8_t)menuInputChoiceAt(wizard.cursor, true) : (uint8_t)menuOutputChoiceAt(wizard.cursor, true);
      const bool autoMode = inputList ? mode == (uint8_t)RZORD_AUTODETECT : mode == (uint8_t)OUTPUT_AUTO;
      if (!autoMode) (inputList ? wizardVisibility.hidden_input_modes : wizardVisibility.hidden_output_modes) ^= uint32_t(1) << mode;
      rendered.valid = false;
      result = ResultMoved;
    } else result = accept(wizard, config);
  } else if (backPressed) {
    result = back(wizard, config);
  }

  switch (result) {
    case ResultMoved:
    case ResultStep:
      if (rendered.step == StepWelcome && wizard.step == StepOkBack) {
        setup_button_tour::begin(tour);
        display.clear();
        renderTour();
        rendered.valid = true;
        break;
      }
      applyPreview();
      buzzer.playMenuNav();
      break;
    case ResultSave:
      leaveWizard(true);
      break;
    case ResultSkip:
      leaveWizard(false);
      break;
    default:
      break;
  }
  return true;
}

#else

bool setupWizardShouldOpen() { return false; }
void beginSetupWizard() {}
bool setupWizardActive() { return false; }
bool handleSetupWizardTutorialButtons(bool, bool, bool, bool, uint32_t) { return false; }
void cancelSetupWizard() {}
bool finishSetupWizard() { return false; }
bool handleSetupWizard(const MenuControllerInput&, bool, bool) { return false; }

#endif
