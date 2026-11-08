#pragma once

#include <stdint.h>
#include <string.h>

namespace psx_fishing {

// RAM-only evidence, separate from the controller frame and USB mapping.
// The histogram covers every accepted poll. Event details retain the first
// 32 and latest 32 nonzero reel replies; omitted middle events are counted.
class Capture {
 public:
  static constexpr uint8_t kEventCapacity = 64;
  static constexpr uint8_t kFirstEvents = 32;
  enum class State : uint8_t { Idle, Running, Frozen };
  enum class Reason : uint8_t { None, Time, Stop, Disconnect };
  struct Event { uint32_t elapsedUs; uint8_t raw[8]; };

  bool start(uint8_t slot, uint32_t durationMs, uint32_t nowUs) {
    if (state_ == State::Running || durationMs < 1000 || durationMs > 30000) return false;
    if (++generation_ == 0) generation_ = 1;
    slot_ = slot;
    durationMs_ = durationMs;
    startedUs_ = nowUs;
    elapsedUs_ = polls_ = nonzero_ = high_ = invalid_ = 0;
    memset(histogram_, 0, sizeof(histogram_));
    memset(minimum_, 0, sizeof(minimum_));
    memset(maximum_, 0, sizeof(maximum_));
    memset(first_, 0, sizeof(first_));
    memset(last_, 0, sizeof(last_));
    state_ = State::Running;
    reason_ = Reason::None;
    return true;
  }

  void tick(uint32_t nowUs) {
    if (state_ == State::Running && uint32_t(nowUs - startedUs_) >= durationMs_ * 1000) {
      elapsedUs_ = durationMs_ * 1000;
      state_ = State::Frozen;
      reason_ = Reason::Time;
    }
  }

  void sample(uint8_t slot, const uint8_t* raw, uint8_t length, uint32_t nowUs) {
    tick(nowUs);
    if (state_ != State::Running || slot != slot_) return;
    if (raw == nullptr || length != 8) { ++invalid_; return; }
    for (uint8_t i = 0; i < 8; ++i) {
      if (polls_ == 0 || raw[i] < minimum_[i]) minimum_[i] = raw[i];
      if (polls_ == 0 || raw[i] > maximum_[i]) maximum_[i] = raw[i];
    }
    if (polls_ == 0) memcpy(first_, raw, 8);
    memcpy(last_, raw, 8);
    ++polls_;
    ++histogram_[raw[7]];
    if (raw[7] >= 128) ++high_;
    if (raw[7] == 0) return;
    const uint32_t index = nonzero_ < kFirstEvents
        ? nonzero_ : kFirstEvents + (nonzero_ - kFirstEvents) % kFirstEvents;
    events_[index].elapsedUs = uint32_t(nowUs - startedUs_);
    memcpy(events_[index].raw, raw, 8);
    ++nonzero_;
  }

  void stop(uint32_t nowUs) { finish(nowUs, Reason::Stop); }
  void disconnect(uint8_t slot, uint32_t nowUs) {
    if (slot == slot_) finish(nowUs, Reason::Disconnect);
  }
  State state() const { return state_; }
  Reason reason() const { return reason_; }
  uint32_t generation() const { return generation_; }
  uint8_t slot() const { return slot_; }
  uint32_t durationMs() const { return durationMs_; }
  uint32_t elapsedUs(uint32_t nowUs) const {
    return state_ == State::Running ? uint32_t(nowUs - startedUs_) : elapsedUs_;
  }
  uint32_t polls() const { return polls_; }
  uint32_t nonzero() const { return nonzero_; }
  uint32_t high() const { return high_; }
  uint32_t invalid() const { return invalid_; }
  uint32_t histogram(uint8_t value) const { return histogram_[value]; }
  uint8_t eventCount() const { return nonzero_ < kEventCapacity ? uint8_t(nonzero_) : kEventCapacity; }
  uint32_t omitted() const { return nonzero_ - eventCount(); }
  const uint8_t* minimum() const { return minimum_; }
  const uint8_t* maximum() const { return maximum_; }
  const uint8_t* first() const { return first_; }
  const uint8_t* last() const { return last_; }

  const Event* event(uint8_t index) const {
    if (index >= eventCount()) return nullptr;
    if (index < kFirstEvents) return &events_[index];
    const uint32_t tailWrites = nonzero_ - kFirstEvents;
    const uint32_t tailCount = tailWrites < kFirstEvents ? tailWrites : kFirstEvents;
    return &events_[kFirstEvents + (tailWrites - tailCount + index - kFirstEvents) % kFirstEvents];
  }

 private:
  void finish(uint32_t nowUs, Reason why) {
    tick(nowUs);
    if (state_ != State::Running) return;
    elapsedUs_ = uint32_t(nowUs - startedUs_);
    state_ = State::Frozen;
    reason_ = why;
  }
  State state_ = State::Idle;
  Reason reason_ = Reason::None;
  uint8_t slot_ = 0;
  uint32_t generation_ = 0, durationMs_ = 0, startedUs_ = 0, elapsedUs_ = 0;
  uint32_t polls_ = 0, nonzero_ = 0, high_ = 0, invalid_ = 0;
  uint32_t histogram_[256] = {};
  uint8_t minimum_[8] = {}, maximum_[8] = {}, first_[8] = {}, last_[8] = {};
  Event events_[kEventCapacity] = {};
};

static_assert(sizeof(Capture) <= 2048, "Fishing capture exceeds the PSX-only RAM budget");
}  // namespace psx_fishing
