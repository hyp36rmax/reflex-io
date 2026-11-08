#pragma once

#include <stdint.h>

// Output-owned runtime services for transport housekeeping, queued host
// transfers, and output-send completion.

void runOutputBackgroundTasks();
void runOutputTransportSyncTasks();
void processPendingOutputRuntimeTasks();
void sendPreparedOutputFrame();

struct UsbDeviceRuntimeDiagnostics {
  uint32_t task_count;
  uint32_t mount_count;
  uint32_t umount_count;
  uint32_t suspend_count;
  uint32_t resume_count;
  uint32_t max_task_gap_us;
  uint32_t last_task_gap_us;
  uint32_t last_task_ms;
  // USB remote wakeup: whether the host armed it at the last suspend, and how
  // many wake requests a button press has sent.
  uint8_t remote_wakeup_armed;
  uint32_t remote_wakeup_count;
};

const UsbDeviceRuntimeDiagnostics& usbDeviceRuntimeDiagnostics();

// Resolve a USB descriptor interface number supplied by MiSTer to Adapt's
// physical source-port ordinal. Returns -1 when this output has no HID device
// interface at that number.
int8_t outputUsbSourcePortForInterfaceNumber(uint8_t interfaceNumber);
