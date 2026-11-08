#pragma once

#include <stdint.h>
#include "menu_list_navigation.h"

// First-boot setup: which steps exist, what each offers and how the cursor
// moves. No display, storage or product headers, so host tests run it as is.
namespace setup_wizard {

enum Step : uint8_t {
  StepWelcome,
  StepOkBack,
  StepBrightness,
  StepSound,
  StepIdleTimer,
  StepIdleAction,
  StepAnimation,
  StepHotkeys,
  StepManualInputs,
  StepManualOutputs,
  StepFinish,
  StepCount,
};

enum IdleAction : uint8_t { IdleScreensaver, IdleDim, IdleBlank, IdleActionCount };
enum Result : uint8_t { ResultNone, ResultMoved, ResultStep, ResultSave, ResultSkip };

constexpr uint8_t kBrightnessLevels[] = {64, 128, 192, 254};
constexpr uint8_t kBrightnessCount = sizeof(kBrightnessLevels);
constexpr uint8_t kOkBackCount = 3;
constexpr uint8_t kSoundCount = 3;
constexpr uint8_t kIdleTimerCount = 4;

// One value per step, as the index of the highlighted option.
struct Choices {
  uint8_t okBack = 0;
  uint8_t brightness = 1;
  uint8_t sound = 1;
  uint8_t idleTimer = 1;
  uint8_t idleAction = IdleScreensaver;
  uint8_t animation = 0;  // index into the enabled-animation list
  uint8_t hotkeys = 1;
};

struct Config {
  bool hasBuzzer = true;
  uint8_t animationCount = 1;
  uint8_t inputCount = 0;
  uint8_t outputCount = 0;
};

struct State {
  Step step = StepWelcome;
  uint8_t cursor = 0;
  Choices choices{};
};

constexpr uint8_t brightnessIndexForContrast(uint8_t contrast) {
  return contrast <= 64 ? 0 : contrast <= 128 ? 1 : contrast <= 192 ? 2 : 3;
}

constexpr bool stepShown(Step step, const Choices& choices, const Config& config) {
  return step == StepManualInputs ? config.inputCount != 0 :
         step == StepManualOutputs ? config.outputCount != 0 :
         step == StepSound ? config.hasBuzzer :
         step == StepIdleAction ? choices.idleTimer != 0 :
         step == StepAnimation ? choices.idleTimer != 0 &&
                                 choices.idleAction == IdleScreensaver :
         step < StepCount;
}

constexpr uint8_t optionCount(Step step, const Config& config) {
  return step == StepWelcome ? 2 :      // Start, Skip
         step == StepOkBack ? kOkBackCount :
         step == StepBrightness ? kBrightnessCount :
         step == StepSound ? kSoundCount :
         step == StepIdleTimer ? kIdleTimerCount :
         step == StepIdleAction ? uint8_t(IdleActionCount) :
         step == StepAnimation ? (config.animationCount ? config.animationCount : uint8_t(1)) :
         step == StepHotkeys ? 2 :      // Off, On
         step == StepManualInputs ? uint8_t(config.inputCount + 1) :
         step == StepManualOutputs ? uint8_t(config.outputCount + 1) :
         step == StepFinish ? 2 : 0;    // Save, Change something
}

// The stored choice for a value step; Welcome and Finish have none.
inline uint8_t* choiceFor(Step step, Choices& choices) {
  switch (step) {
    case StepOkBack: return &choices.okBack;
    case StepBrightness: return &choices.brightness;
    case StepSound: return &choices.sound;
    case StepIdleTimer: return &choices.idleTimer;
    case StepIdleAction: return &choices.idleAction;
    case StepAnimation: return &choices.animation;
    case StepHotkeys: return &choices.hotkeys;
    default: return nullptr;
  }
}

inline void enterStep(State& state, Step step, const Config& config) {
  state.step = step;
  const uint8_t* choice = choiceFor(step, state.choices);
  const uint8_t count = optionCount(step, config);
  // Keep the all-shown default easy to accept; Left/Down wraps to the list.
  state.cursor = step == StepManualInputs || step == StepManualOutputs ? count - 1 :
                 choice && *choice < count ? *choice : 0;
}

// Numbered position among the value steps that are currently shown.
inline uint8_t stepNumber(const State& state, const Config& config) {
  uint8_t number = 0;
  for (uint8_t s = StepOkBack; s <= state.step && s < StepFinish; ++s) {
    if (stepShown(Step(s), state.choices, config)) ++number;
  }
  return number;
}

inline uint8_t stepTotal(const State& state, const Config& config) {
  uint8_t total = 0;
  for (uint8_t s = StepOkBack; s < StepFinish; ++s) {
    if (stepShown(Step(s), state.choices, config)) ++total;
  }
  return total;
}

// A value step's highlighted option is its choice, so previews follow it.
inline Result move(State& state, const Config& config, bool forward) {
  const uint8_t count = optionCount(state.step, config);
  if (count < 2) return ResultNone;
  state.cursor = forward ? uint8_t((state.cursor + 1) % count)
                         : uint8_t((state.cursor + count - 1) % count);
  if (uint8_t* choice = choiceFor(state.step, state.choices)) *choice = state.cursor;
  return ResultMoved;
}

inline Result accept(State& state, const Config& config) {
  if (state.step == StepWelcome && state.cursor == 1) return ResultSkip;
  if (state.step == StepFinish) {
    if (state.cursor == 0) return ResultSave;
    enterStep(state, StepOkBack, config);
    return ResultStep;
  }
  uint8_t next = uint8_t(state.step + 1);
  while (next < StepFinish && !stepShown(Step(next), state.choices, config)) ++next;
  enterStep(state, Step(next), config);
  return ResultStep;
}

inline Result back(State& state, const Config& config) {
  if (state.step == StepWelcome) return ResultNone;
  uint8_t previous = uint8_t(state.step - 1);
  while (previous > StepWelcome && !stepShown(Step(previous), state.choices, config)) --previous;
  enterStep(state, Step(previous), config);
  return ResultStep;
}

// First visible option for a list of `visible` rows.
constexpr uint8_t scrollFor(uint8_t cursor, uint8_t count, uint8_t visible) {
  return menu_list_navigation::firstVisible(cursor, count, visible);
}

}  // namespace setup_wizard
