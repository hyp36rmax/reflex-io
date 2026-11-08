#pragma once

inline bool productOutputTransportAllowsUsbConnect() { return true; }
inline void productOutputTransportTask() {}
inline bool productOutputTransportIsActive() { return false; }
inline bool productOutputTransportSendPreparedFrame() { return false; }
