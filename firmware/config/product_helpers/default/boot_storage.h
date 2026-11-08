#pragma once

inline void productAdjustBootConfiguration(
    DeviceEnum&, DeviceEnum&, outputMode_t&) {}

inline void productInitializeOutputTransport(uint8_t, uint8_t) {}

inline uint16_t productStorageRequiredEnd(uint16_t currentEnd) {
  return currentEnd;
}

inline void productPreparePersistentStorage() {}

inline bool productClearPersistentStorage() { return true; }
