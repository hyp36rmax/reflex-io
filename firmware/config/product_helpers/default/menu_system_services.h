#pragma once

inline void productFirmwareUpdatePoll() {}
inline void productFirmwareUpdateOnClear() {}
inline bool productFirmwareRollbackNoticeActive() { return false; }
inline bool productAcknowledgeFirmwareRollbackNotice() { return true; }
