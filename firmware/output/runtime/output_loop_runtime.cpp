#include "../../product_config.h"

// Override TinyUSB defaults before including the output transport stack.
#if defined(ADAPT_OUTPUT_USB_DEVICE)
#define CFG_TUD_HID 6  // 6-player multitap support (Saturn, etc.)
#endif

#include "output_loop_runtime.h"

#include <Adafruit_TinyUSB.h>
#include <cstring>

#include "../auth/webhid_auth_runtime.h"
#include "../auth/ps_auth_dongle_runtime.h"
#include "../out_report.h"
#include "../output_capabilities.h"
#include "../../core/controller_delivery_state.h"
#include "../../core/controller_frame_state.h"
#include "../../core/neutral_frame_packet.h"
#include "../../core/rumble_test_runtime.h"
#include "../../core/usb_poll_alignment.h"
#include "../../core/usb_device_lock.h"
#include "../../input/runtime/input_frame_runtime.h"
#include "output_runtime_bridge.h"
#include "output_product_services.h"
#include "../output_runtime_state.h"
#include "../remote/output_remote_runtime.h"
#include "../../features/jogcon_capture/jogcon_capture.h"

#if defined(ADAPT_OUTPUT_USB_DEVICE)
#include "../usb/output_usb_player_count.h"
#include "../switch2/output_switch2pro_source_port.h"
#define ADAPT_USB_DELIVERY_HELPERS_ONLY
#include "../usb/output_usb_send_runtime.h"
#include "../switch/output_switch_nso_source_port.h"
#undef ADAPT_USB_DELIVERY_HELPERS_ONLY
#endif

#if defined(ADAPT_FEATURE_MISTER_SERIAL_STATUS) && defined(ADAPT_OUTPUT_USB_DEVICE)
#include "../usb/output_usb_hid_port_order_runtime.h"
#include "output_usb_interface_mapping.h"
#endif

#if !defined(ADAPT_OUTPUT_USB_DEVICE)
  #ifdef ENABLE_OUTPUT_ESP32_BT
    #include "../out_esp32_bt.h"
  #elif defined(ENABLE_OUTPUT_JVS)
    #include "../jvs/out_jvs.h"
  #elif defined(ENABLE_OUTPUT_CONSOLE)
    #include "../classic/out_classic.h"
  #elif defined(ENABLE_OUTPUT_DB15)
    #include "../db15/out_db15.h"
  #endif
#endif

#if defined(ADAPT_OUTPUT_USB_DEVICE)
static_assert(CFG_TUD_HID == 6, "CFG_TUD_HID must be 6 for 6-player multitap support");
#endif

namespace {

UsbDeviceRuntimeDiagnostics usbDeviceDiag{};
uint32_t lastUsbTaskUs = 0;
#ifdef ADAPT_OUTPUT_USB_DEVICE
uint32_t lastWakeButtons = 0;

// PC output modes may wake a sleeping host. Console identities keep their own
// wake behavior (XID already signals from its report path).
bool outputModeAllowsRemoteWakeup(outputMode_t mode) {
  switch (canonicalizeOutputMode(mode)) {
    case OUTPUT_HID:
    case OUTPUT_MISTER:
    case OUTPUT_XINPUT:
    case OUTPUT_XINPUT2P:
    case OUTPUT_KEYBOARD:
      return true;
    default:
      return false;
  }
}

// A newly pressed button (not one held into suspend, and never stick motion)
// asks a suspended host to resume.
bool usbRemoteWakeupShouldSignal(bool suspended, outputMode_t mode,
                                 uint32_t buttons, uint32_t previousButtons) {
  return suspended && outputModeAllowsRemoteWakeup(mode) &&
         (buttons & ~previousButtons) != 0;
}

uint8_t usbCurrentStateSourcePortForOutput(uint8_t outputPort,
                                           outputMode_t effectiveOutputMode) {
  if (effectiveOutputMode == OUTPUT_REMOTE) return remote_output_source_port();
  if (effectiveOutputMode == OUTPUT_SWITCH2PRO) {
    return switch2pro_output_source_port();
  }
#if defined(ENABLE_INPUT_USB) || defined(ENABLE_INPUT_JVS)
  if (effectiveOutputMode == OUTPUT_SWITCHPRO) {
    return switchpro_output_source_port(outputPort);
  }
#endif
  (void)effectiveOutputMode;
  return outputPort;
}

void requestFreshUsbCurrentStateDelivery() {
  const outputMode_t effectiveOutputMode = get_effective_output_mode();
  const uint8_t activeSourceCount = min(inputPortCount(), (uint8_t)MAX_USB_OUT);
  usb_delivery_runtime_detail::requestFreshCurrentState(
      output_usb_player_count(), activeSourceCount,
      [effectiveOutputMode](uint8_t outputSlot) {
        return usbCurrentStateSourcePortForOutput(outputSlot,
                                                  effectiveOutputMode);
      },
      [](uint8_t sourcePort) {
        requestControllerFrameDelivery(sourcePort);
      });
}
#endif

void __not_in_flash_func(noteUsbDeviceTask)() {
  const uint32_t nowUs = micros();
  if (lastUsbTaskUs != 0) {
    const uint32_t gap = nowUs - lastUsbTaskUs;
    usbDeviceDiag.last_task_gap_us = gap;
    if (gap > usbDeviceDiag.max_task_gap_us) {
      usbDeviceDiag.max_task_gap_us = gap;
    }
  }
  lastUsbTaskUs = nowUs;
  usbDeviceDiag.task_count++;
  usbDeviceDiag.last_task_ms = millis();
}

}  // namespace

const UsbDeviceRuntimeDiagnostics& usbDeviceRuntimeDiagnostics() {
  return usbDeviceDiag;
}

int8_t outputUsbSourcePortForInterfaceNumber(uint8_t interfaceNumber) {
#if defined(ADAPT_FEATURE_MISTER_SERIAL_STATUS) && defined(ADAPT_OUTPUT_USB_DEVICE)
  const uint8_t* descriptor = tud_descriptor_configuration_cb(0);
  if (!descriptor || descriptor[0] < 9) {
    return output_usb_interface_mapping::kUnknownSourcePort;
  }
  const size_t length = (size_t)descriptor[2] | ((size_t)descriptor[3] << 8);
  const int8_t hidOrdinal =
    output_usb_interface_mapping::hidOrdinalForInterface(descriptor, length, interfaceNumber);
  if (hidOrdinal < 0 || (uint8_t)hidOrdinal >= MAX_USB_OUT) {
    return output_usb_interface_mapping::kUnknownSourcePort;
  }
  return (int8_t)output_usb_source_port_for_hid_interface((uint8_t)hidOrdinal);
#else
  (void)interfaceNumber;
  return -1;
#endif
}

#ifdef ADAPT_OUTPUT_USB_DEVICE
extern "C" void tud_mount_cb(void) {
  usbDeviceDiag.mount_count++;
  // Queue the active sources for the next normal prepare/send pass. Sending
  // here would expose raw frames before remaps, SOCD, turbo and hotkeys finish.
  requestFreshUsbCurrentStateDelivery();
  // The full-speed SOF is the upstream host's real 1 kHz schedule. Interrupt
  // endpoint polling has a stable offset within that frame, unlike changed HID
  // report completions, which can be many frames apart.
  tud_sof_cb_enable(usbPollAlignmentUsesSof());
}

extern "C" void tud_umount_cb(void) {
  tud_sof_cb_enable(false);
  usbDeviceDiag.umount_count++;
  rumbleRuntimeClearAllHostFeedback();
}

extern "C" void tud_suspend_cb(bool remote_wakeup_en) {
  usbDeviceDiag.remote_wakeup_armed = remote_wakeup_en ? 1 : 0;
  usbDeviceDiag.suspend_count++;
  rumbleRuntimeClearAllHostFeedback();
}

extern "C" void tud_resume_cb(void) {
  usbDeviceDiag.resume_count++;
  requestFreshUsbCurrentStateDelivery();
}
#endif

void __not_in_flash_func(runOutputTransportSyncTasks)() {
  productOutputTransportTask();
  #ifdef ADAPT_OUTPUT_USB_DEVICE
  if (productOutputTransportIsActive()) return;
  #if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
  TinyUSB_Device_Task();
  #else
  TinyUSBDevice.task();
  #endif
  #if defined(ADAPT_FEATURE_SWITCH_REPORT_STREAM)
  queuePeriodicSwitchReports();
  #endif
  queuePeriodicSwitch2ProReportsTask();
  #if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  queuePeriodicRotaryMouseStick();
  #endif
  #endif
}

void runOutputBackgroundTasks() {
  #ifdef ADAPT_OUTPUT_USB_DEVICE
  runOutputTransportSyncTasks();
  if (productOutputTransportIsActive()) return;
  // Diagnostics are not transport-critical. Record them after the input frame
  // has been polled and submitted so clock reads and counter maintenance cannot
  // delay the controller-to-USB path.
  noteUsbDeviceTask();

  uint32_t wakeButtons = 0;
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    wakeButtons |= neutralFrameButtonMask(controllerFrameConst(i));
  }
  // tud_remote_wakeup() itself refuses unless the host armed remote wakeup.
  if (usbRemoteWakeupShouldSignal(tud_suspended(), get_effective_output_mode(),
                                  wakeButtons, lastWakeButtons) &&
      tud_remote_wakeup()) {
    usbDeviceDiag.remote_wakeup_count++;
  }
  lastWakeButtons = wakeButtons;

  if (outputMode == OUTPUT_XINPUT || outputMode == OUTPUT_XINPUT2P) {
    xinput_auth_process();
  }

  auto_detect_process();
  if (!is_ps5_timing_quiet_mode_active()) {
    webhid_process_commands();
    webhid_process_rumble();
  }
  #endif
}

void processPendingOutputRuntimeTasks() {
  #ifdef ADAPT_OUTPUT_USB_DEVICE
  if (productOutputTransportIsActive()) return;
  // PS4 auth: perform RSA signing when nonce is ready (runs in main loop, ~150ms)
  #if !defined(ENABLE_INPUT_USB) && !defined(ENABLE_USB_AUTH_SIDECAR_CORE1)
  ps_auth_dongle_task();
  do_send_out_report();
  #endif

  if (!is_ps5_timing_quiet_mode_active() && ps4Auth.isReady()) {
    ps4Auth.process();
  }
  #endif
}

void __not_in_flash_func(sendPreparedOutputFrame)() {
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
  for (uint8_t slot = 0; slot < max_devices && slot < MAX_USB_OUT; ++slot)
    jogconCaptureOutput(slot, controllerFrameConst(slot));
#endif
  #ifdef ADAPT_OUTPUT_USB_DEVICE
  if (productOutputTransportSendPreparedFrame()) return;
  {
    UsbDeviceLock usbLock;  // every persona's send claims an IN endpoint
    send_usb_report();
  }
  #elif defined(ENABLE_OUTPUT_ESP32_BT)
  esp32_bt_send();
  #elif defined(ENABLE_OUTPUT_JVS)
  jvsOutput.update();
  #elif defined(ENABLE_OUTPUT_CONSOLE)
  classic_output_update();
  #elif defined(ENABLE_OUTPUT_DB15)
  db15_output_update();
  #endif
}
