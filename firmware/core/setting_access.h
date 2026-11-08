#pragma once

#include <stdint.h>

#include "settings_store.h"
#include "../output/output_mode.h"

// One read/write/visibility path for the config tools. Serial SET (Adapt
// Manager) and the WebHID setting channel (web config) both use it, so a value
// is sanitized, persisted and applied exactly as the OLED menus would. It is
// implemented beside the serial SET command in serial_core_commands.cpp.
enum class SettingAccessStatus : uint8_t {
  Ok = 0,
  Unsupported,
  UseModeCommand,     // input/output mode have their own commands
  NeedsConcreteMode,  // per-mode setting asked for a non-concrete mode
  WriteFailed,        // read-back did not match the sanitized value
};

struct SettingWriteResult {
  SettingAccessStatus status;
  int32_t sanitized;
  int32_t stored;
  bool runtimeKnown;
  int32_t runtime;
  bool rebootNow;       // takes effect only after the caller reboots
  bool rebootRequired;  // USB descriptor changes at the next reboot
};

bool settingAccessSupported(SettingId id);
SettingWriteResult settingAccessWrite(SettingId id, DeviceEnum mode, int32_t value);
bool settingAccessRuntimeValue(SettingId id, DeviceEnum mode, int32_t* value);
// Marks the settings the Quick Menu shows for this input and output mode.
void settingAccessVisible(DeviceEnum mode, outputMode_t output,
                          bool (&visible)[static_cast<uint8_t>(SettingId::Count)]);
