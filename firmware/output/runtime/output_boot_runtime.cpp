#include "../../product_config.h"

#include "output_boot_runtime.h"

#include <Adafruit_TinyUSB.h>

#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
  #include "hardware/watchdog.h"
#endif

#include "../../core/controller_frame_state.h"
#include "../../core/device_runtime_state.h"
#include "../../firmware_platform_config.h"
#include "../../input/runtime/input_adapter_runtime.h"
#include "../../input/runtime/input_frame_runtime.h"
#include "../../input/autodetect/input_autodetect_benchmark.h"
#include "../../input/autodetect/input_autodetect_runtime_state.h"
#include "input_usb_identity_runtime.h"
#include "output_boot_bridge.h"
#include "output_product_services.h"
#include "../output_capabilities.h"
#include "../output_runtime_state.h"
#include "../remote/output_remote_runtime.h"

namespace {

auto_output_boot_state_t getAutoOutputBootStateFromScratch() {
  auto_output_boot_state_t bootState = {
    .autoOutputProbeBoot =
      (configuredOutputMode == OUTPUT_AUTO) &&
      (watchdog_hw->scratch[AUTO_PROBE_SCRATCH_STAGE] != (uint32_t)AUTO_PROBE_GENERIC),
    .autoOutputResolvedBoot =
      (configuredOutputMode == OUTPUT_AUTO) &&
      (watchdog_hw->scratch[AUTO_DETECT_SCRATCH_MAGIC] == AUTO_DETECT_MAGIC) &&
      (sanitizeRuntimeOutputMode((outputMode_t)(
          watchdog_hw->scratch[AUTO_DETECT_SCRATCH_STATE] & 0xFFu)) != OUTPUT_AUTO)
  };
  return bootState;
}

bool shouldForceJvsioTwoPlayerUsbSlots(outputMode_t mode) {
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  switch (canonicalizeOutputMode(mode)) {
    case OUTPUT_HID:
    case OUTPUT_MISTER:
    case OUTPUT_KEYBOARD:
    case OUTPUT_XINPUTW:
    case OUTPUT_XINPUT2P:
      return true;
    default:
      return false;
  }
#else
  (void)mode;
  return false;
#endif
}

bool shouldReserveClassic2usbUsbSlots(outputMode_t mode) {
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  switch (canonicalizeOutputMode(mode)) {
    case OUTPUT_HID:
    case OUTPUT_MISTER:
    case OUTPUT_MISTER_JOGCON:
    case OUTPUT_MISTER_NEGCON:
    case OUTPUT_MISTER_PADDLE:
    case OUTPUT_MISTER_SPINNER:
    case OUTPUT_ROTARY_MOUSE:
    case OUTPUT_REMOTE:
    case OUTPUT_XINPUTW:
    case OUTPUT_XINPUT2P:
    case OUTPUT_SWITCH:
    case OUTPUT_SWITCHPRO:
      return true;
    default:
      return false;
  }
#else
  (void)mode;
  return false;
#endif
}

bool classic2usbInputCanReserveDetectedUsbSlots(DeviceEnum inputMode) {
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  switch (inputMode) {
    #ifdef ENABLE_INPUT_PSX
    case RZORD_PSX:
    #endif
    #ifdef ENABLE_INPUT_SATURN
    case RZORD_SATURN:
    #endif
    #ifdef ENABLE_INPUT_PCE
    case RZORD_PCE:
    #endif
    #ifdef ENABLE_INPUT_SNES
    case RZORD_SNES:
    #endif
      return true;
    default:
      return false;
  }
#else
  return false;
#endif
}

uint8_t classic2usbBootUsbSlotCountForInput(outputMode_t mode, DeviceEnum inputMode,
                                            uint8_t portCount,
                                            uint8_t connectedPlayers) {
  const uint8_t outputMax = get_max_output_for_mode(mode);
  const uint8_t physicalSlots = min(portCount, outputMax);
  // Captured native NSO profiles expose one USB HID interface. Presenting a
  // second identical native controller makes the Switch reject/retry the
  // profile handoff. NSO mode disabled retains normal two-player Pro output.
  if (canonicalizeOutputMode(mode) == OUTPUT_SWITCHPRO &&
      output_switch_profile_for_input_mode(
        inputMode,
        (nso_special_effective_for_input_mode(inputMode) &&
         input_mode_allows_nso_special_for_connected_players(
           inputMode, connectedPlayers))
          ? nso_special
          : SWITCH_DEVICE_PRO) !=
        SWITCHPRO_PRO) {
    return 1;
  }
  if (shouldReserveClassic2usbUsbSlots(mode)) {
    if (portCount > outputMax && classic2usbInputCanReserveDetectedUsbSlots(inputMode)) {
      return min(portCount, (uint8_t)MAX_USB_OUT);
    }
    return outputMax;
  }
  return physicalSlots;
}

void clearUnpolledUsbSlots(uint8_t firstSlot) {
  for (uint8_t i = firstSlot; i < max_devices && i < MAX_USB_OUT; ++i) {
    controllerFrame(i) = {};
  }
}

void restoreDetectedAutoOutputModeAtBoot() {
  outputMode_t detectedOutputMode = auto_detect_load_persisted_runtime_mode();
  if (detectedOutputMode != OUTPUT_AUTO) {
    auto_detect_restore_debug_snapshot();
    output_autodetect_transition_reason = auto_detect_load_transition_reason();
    autoDetectProbeStage = AUTO_PROBE_GENERIC;
    outputMode = detectedOutputMode;
    autoDetectState = auto_detect_load_persisted_state(detectedOutputMode);
  } else {
    autoDetectProbeStage = auto_detect_load_probe_stage();
    output_autodetect_transition_reason = auto_detect_load_transition_reason();
    outputMode = auto_detect_probe_output_mode(autoDetectProbeStage);
    autoDetectState = AUTO_STATE_IDLE;
  }
}

void clearAutoOutputScratchRegisters() {
  watchdog_hw->scratch[AUTO_DETECT_SCRATCH_MAGIC] = 0;
  watchdog_hw->scratch[AUTO_DETECT_SCRATCH_STATE] = 0;
  watchdog_hw->scratch[AUTO_PROBE_SCRATCH_STAGE] = 0;
  watchdog_hw->scratch[AUTO_PROBE_SCRATCH_AUX] = 0;
}

}  // namespace

auto_output_boot_state_t restoreAutoOutputBootState() {
  auto_output_boot_state_t bootState = getAutoOutputBootStateFromScratch();

  if (configuredOutputMode == OUTPUT_AUTO) {
    restoreDetectedAutoOutputModeAtBoot();
  } else {
    autoDetectProbeStage = AUTO_PROBE_GENERIC;
    autoDetectState = AUTO_STATE_IDLE;
  }

  clearAutoOutputScratchRegisters();
  return bootState;
}

void restoreLockedOutputModeAfterInputSetup(bool outputModeLockedAtBoot, outputMode_t bootOutputMode) {
  if (!outputModeLockedAtBoot) {
    return;
  }

  // A resolved MiSTer/Linux DInput host starts on generic HID. PSX setup may then
  // identify a JogCon, neGcon, or GunCon before USB is configured. Preserve
  // that descriptor promotion; all other Auto-host modes remain locked to the
  // host detector's result.
  if (canonicalizeOutputMode(bootOutputMode) == OUTPUT_MISTER &&
      output_is_specialized_mister_psx_mode(outputMode)) {
    return;
  }
  outputMode = bootOutputMode;
}

uint8_t bootUsbSlotCountForModeAndInput(outputMode_t mode, DeviceEnum inputMode,
                                        uint8_t portCount, uint8_t connectedPlayers) {
#if defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT) && defined(MODERN2USB_AUTODETECT_PROBE_USB_SLOTS)
  if (configuredOutputMode == OUTPUT_AUTO && autoDetectState == AUTO_STATE_IDLE) {
    return min((uint8_t)MODERN2USB_AUTODETECT_PROBE_USB_SLOTS,
               get_max_output_for_mode(mode));
  }
#endif
  if (shouldForceJvsioTwoPlayerUsbSlots(mode)) {
    return min((uint8_t)2, get_max_output_for_mode(mode));
  }
  if (mode == OUTPUT_KEYBOARD) {
    return get_max_output_for_mode(mode);
  }
  if (mode == OUTPUT_SWITCH2PRO) {
    // One USB player, driven by the earliest connected port. Every frame stays
    // prepared, as for Remote, so a lone P2 gets remaps and menu capture. The
    // count is fixed: no descriptor depends on it, so a change of input must
    // not look like a player-count change.
    return MAX_USB_OUT;
  }

  return classic2usbBootUsbSlotCountForInput(
    mode, inputMode, portCount, connectedPlayers);
}

outputMode_t finalizeBootOutputMode() {
  refineRemoteOutputModeAtBoot();
  outputMode_t effectiveOutputMode = get_effective_output_mode();
  uint8_t connectedPlayers = connectedInputFrameCount(inputPortCount());
  const uint8_t preservedConnectedPlayers = inputReenumReservedConnectedCount();
  if (connectedPlayers == 0 && preservedConnectedPlayers != 0) {
    // A descriptor reboot can complete before the first controller poll does.
    // Preserve the live count that requested the reboot so N64 selects its
    // native NSO PID instead of returning to generic Pro and rebooting again.
    connectedPlayers = preservedConnectedPlayers;
  }
  clearInputReenumReservedConnectedCount();
  latch_nso_special_connected_players(deviceMode, connectedPlayers);
  output_apply_automatic_switch_profile_for_input_mode(deviceMode);
  if (shouldForceJvsioTwoPlayerUsbSlots(effectiveOutputMode)) {
    // Dedicated JVSIO firmware represents the cabinet as P1/P2 for every
    // Windows-family two-player output, even while the JVS board is still
    // finishing its first post-boot sync.
    max_devices = min((uint8_t)2, get_max_output_for_mode(effectiveOutputMode));
  } else if (effectiveOutputMode == OUTPUT_KEYBOARD) {
    // Keep fixed P1/P2 keyboard slots available even when the input mode
    // discovers extra ports after boot, such as JVS boards coming online.
    max_devices = get_max_output_for_mode(effectiveOutputMode);
  } else {
    // Count slots from the same players that selected the profile above. A
    // live count that is still zero would give native N64 two slots and
    // request the same re-enumeration again.
    max_devices = bootUsbSlotCountForModeAndInput(
        effectiveOutputMode, deviceMode, inputPortCount(), connectedPlayers);
  }
  if (max_devices == 0) {
    max_devices = 1;
  }
  if (max_devices > inputPortCount()) {
    clearUnpolledUsbSlots(inputPortCount());
  }
  configureActiveInputAdapterBcdDeviceVersion();
  applyInputModeBcdDeviceVersion(deviceMode);

  #ifdef ADAPT_OUTPUT_USB_DEVICE
  if (must_set_serial_string(effectiveOutputMode)) {
    TinyUSBDevice.setSerialDescriptor(get_reflex_input_usb_serial_descriptor());
  }
  #endif

  return effectiveOutputMode;
}

void connectConfiguredOutputUsb() {
  #ifdef ADAPT_OUTPUT_USB_DEVICE
  if (!productOutputTransportAllowsUsbConnect()) return;
  #ifdef ENABLE_OUTPUT_JVS
  if (outputMode != OUTPUT_JVS)
  #endif
  {
    autoDetectBenchmarkMark(ADBENCH_USB_CONNECT);
    #if defined(AUTODETECT_DONOR_ATTACH_SEQUENCE)
    tud_disconnect();
    delay(250);
    #endif
    tud_connect();
    #if defined(AUTODETECT_DONOR_ATTACH_SEQUENCE)
    delay(50);
    #endif
  }
  #endif
}
