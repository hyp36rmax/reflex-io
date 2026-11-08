#pragma once

#include <stdint.h>

#if defined(ARDUINO_ARCH_RP2040) && \
    !defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
#include <pico/mutex.h>
#define REFLEX_SETTINGS_SNAPSHOT_PICO_MUTEX 1
#endif

// Cross-core snapshot used where the USB-host decoder may read a pending
// configuration object from core1 while core0 stages or publishes it. The
// lock is held only for the RAM copy/revision update; flash erase/program
// always operates on an unlocked snapshot. RP2040 firmware uses the SDK mutex
// implementation while host models retain a compiler-atomic lock.
template <typename T>
class CrossCoreDeferredSnapshot {
 public:
#if defined(REFLEX_SETTINGS_SNAPSHOT_PICO_MUTEX)
  explicit CrossCoreDeferredSnapshot(mutex_t* mutex) : mutex_(mutex) {}
#else
  CrossCoreDeferredSnapshot() = default;
#endif

  void stage(const T& value) {
    lock();
    value_ = value;
    pending_ = true;
    ++revision_;
    unlock();
  }

  bool snapshot(T* value, uint32_t* revision = nullptr) const {
    lock();
    const bool pending = pending_;
    if (pending && value != nullptr) {
      *value = value_;
    }
    if (revision != nullptr) {
      *revision = revision_;
    }
    unlock();
    return pending;
  }

  bool clearIfRevision(uint32_t revision) {
    lock();
    const bool matches = pending_ && revision_ == revision;
    if (matches) {
      pending_ = false;
      ++revision_;
    }
    unlock();
    return matches;
  }

  void discard() {
    lock();
    pending_ = false;
    ++revision_;
    unlock();
  }

  bool hasPending() const {
    return snapshot(nullptr, nullptr);
  }

 private:
  void lock() const {
#if defined(REFLEX_SETTINGS_SNAPSHOT_PICO_MUTEX)
    mutex_enter_blocking(mutex_);
#else
    while (__atomic_test_and_set(&locked_, __ATOMIC_ACQUIRE)) {
    }
#endif
  }

  void unlock() const {
#if defined(REFLEX_SETTINGS_SNAPSHOT_PICO_MUTEX)
    mutex_exit(mutex_);
#else
    __atomic_clear(&locked_, __ATOMIC_RELEASE);
#endif
  }

  T value_{};
  bool pending_ = false;
  uint32_t revision_ = 0;
#if defined(REFLEX_SETTINGS_SNAPSHOT_PICO_MUTEX)
  mutable mutex_t* mutex_ = nullptr;
#else
  mutable bool locked_ = false;
#endif
};

#if defined(REFLEX_SETTINGS_SNAPSHOT_PICO_MUTEX)
#undef REFLEX_SETTINGS_SNAPSHOT_PICO_MUTEX
#endif
