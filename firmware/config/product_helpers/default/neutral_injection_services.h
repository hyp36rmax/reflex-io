#pragma once

inline bool productNeutralInjectionBlocked(Print&) { return false; }
inline bool productNeutralTrainBlocked(Print&) { return false; }
inline bool productNeutralReleaseMustCancel() { return false; }
inline bool productNeutralRuntimeBlocked() { return false; }
