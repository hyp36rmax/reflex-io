#pragma once

#include <stdint.h>
#if defined(ADAPT_FEATURE_JAGUAR_TRACE)
#include "../../features/jaguar_trace/jaguar_trace.h"
#endif
#if defined(ADAPT_FEATURE_N64_CAPTURE)
#include "../../features/n64_capture/n64_capture.h"
#endif

extern "C++" {
namespace usb_delivery_runtime_detail {

template <typename SourcePortForOutput, typename RequestDelivery>
inline void requestFreshCurrentState(uint8_t outputSlotCount,
                                     uint8_t activeSourceCount,
                                     SourcePortForOutput sourcePortForOutput,
                                     RequestDelivery requestDelivery) {
  for (uint8_t outputSlot = 0; outputSlot < outputSlotCount; ++outputSlot) {
    const uint8_t sourcePort = sourcePortForOutput(outputSlot);
    if (sourcePort < activeSourceCount) {
      requestDelivery(sourcePort);
    }
  }
}

template <typename CompleteDelivery>
inline bool completeIfSubmitted(bool submitted,
                                CompleteDelivery completeDelivery) {
  if (!submitted) {
    return false;
  }
  completeDelivery();
  return true;
}

}  // namespace usb_delivery_runtime_detail
}  // extern "C++"

#if !defined(ADAPT_USB_DELIVERY_HELPERS_ONLY)

// Internal USB send/runtime helpers for the USB device output stack.
// This header is included from out_usb.h after the report mappers and shared
// USB output objects have been defined.

#include "../../platform/latency_test.h"
#include "output_usb_diagnostics_runtime.h"
#include "../../core/usb_latency_trace.h"
#include "output_usb_player_count.h"
#include "output_usb_xinput2p_slots_runtime.h"

#if defined(ADAPT_FEATURE_SWITCH_REPORT_STREAM)
#include "../switch/output_switch_report_stream.h"
static SwitchReportStream switch_report_stream[MAX_USB_OUT];

void queuePeriodicSwitchReports() {
  const bool active = get_effective_output_mode() == OUTPUT_SWITCHPRO &&
      tud_mounted() && !tud_suspended();
  const uint8_t players = active ? output_usb_player_count() : 0;
  const uint32_t now = millis();
  for (uint8_t port = 0; port < MAX_USB_OUT; ++port) {
    if (port >= players) {
      switch_report_stream[port].reset();
    } else if (switch_report_stream[port].due(now)) {
      // Queue before preparation. Forcing a send later could expose restored
      // physical buttons instead of remaps and held shortcut outputs.
      requestControllerFrameDelivery(switchpro_output_source_port(port));
    }
  }
}
#endif

void queuePeriodicSwitch2ProReportsTask() {
  queuePeriodicSwitch2ProReports(millis());
}

#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
void queuePeriodicRotaryMouseStick() {
  queueRotaryMouseStickMotion(get_effective_output_mode(), millis());
}
#endif

void send_usb_grouped_report(uint8_t group) {
  uint8_t device_index = group;
  group = group + 1;
  // Effective mode: Switch output with the GC Adapter selection is a WUP-028.
  if (get_effective_output_mode() == OUTPUT_GCWIIU) {
    for (uint8_t i = 0; i < 4; ++i) {
      uint8_t input_index = group * i;
      map_gcwiiu_output(i, input_index);
    }
    bool hid_ready = usb_device_hid[device_index]->ready();
    if (hid_ready && usb_device_hid[device_index]->sendReport(0, &_gcwiiu, sizeof(_gcwiiu))) {
      for (uint8_t i = 0; i < 4; ++i) {
        uint8_t input_index = group * i;
        completeControllerFrameDelivery(input_index);
      }
    }
  }
}

bool keyboard_report_needs_delivery(uint8_t portCount) {
  const uint8_t checkedPorts = min((uint8_t)2, portCount);
  for (uint8_t i = 0; i < checkedPorts; ++i) {
    if (controllerFrameNeedsDelivery(i)) {
      return true;
    }
  }
  return false;
}

void complete_keyboard_report_delivery(uint8_t portCount) {
  const uint8_t checkedPorts = min((uint8_t)2, portCount);
  for (uint8_t i = 0; i < checkedPorts; ++i) {
    if (controllerFrameNeedsDelivery(i)) {
      if (latencyTest.isEnabled()) {
        latencyTest.noteUsbSubmit(i, controllerFrameConst(i).digital_buttons);
      }
    }
    completeControllerFrameDelivery(i);
  }
}

void send_usb_keyboard_report() {
  const uint8_t keyboardPorts = min((uint8_t)2, max_devices);
  if (!keyboard_report_needs_delivery(keyboardPorts)) {
    return;
  }
  if (!usb_device_hid[0] || !usb_device_hid[0]->ready()) {
    return;
  }

  map_keyboard_output_combined(keyboardPorts);
  // A BIOS in boot protocol expects the plain 8-byte report, with no ID.
  const uint8_t keyboardReportId =
      usb_device_hid[0]->getProtocol() == HID_PROTOCOL_BOOT ? 0 : KEYBOARD_REPORT_ID;
  if (usb_device_hid[0]->keyboardReport(keyboardReportId, _keyboard.modifier, _keyboard.keycode)) {
    complete_keyboard_report_delivery(keyboardPorts);
  }
}

void service_usb_feedback_reports() {
  switch (get_effective_output_mode()) {
    case OUTPUT_XINPUT:
      if (_xinput) {
        receive_xinput_report();
      }
      break;

    case OUTPUT_XINPUT2P:
    #ifdef FORCE_XINPUT2P_SINGLE_DRIVER
      if (_xinput) {
        receive_xinput_report();
      }
    #elif defined(ENABLE_XINPUT2P_WIRELESS_TRANSPORT)
      if (_xinputw) {
        const uint8_t outputPlayers = output_usb_player_count();
        for (uint8_t i = 0; i < XINPUT_WIRELESS_CONTROLLERS && i < outputPlayers; ++i) {
          receive_xinputw_report(i);
        }
      }
    #endif
      break;

    case OUTPUT_XID: {
      if (_xid && tud_mounted()) {
        const uint8_t ep_in = (_xid->stored_ep_in != 0) ? _xid->stored_ep_in : 0x81;
        const uint8_t ep_out = (_xid->stored_ep_out != 0) ? _xid->stored_ep_out : 0x02;
        xid_manual_init(0, _xid->stored_itfnum, ep_in, ep_out);
      }
      const int8_t xid_index = xid_get_index_by_type(0, XID_TYPE_WHEEL);
      if (xid_index >= 0) {
        USB_XboxWheel_OutReport_t xpad_rumble = {};
        if (xid_get_report(xid_index, &xpad_rumble, sizeof(xpad_rumble))) {
          uint8_t xid_left = xpad_rumble.lValue >> 8;
          uint8_t xid_right = xpad_rumble.rValue >> 8;
          if ((xid_left | xid_right) == 0 &&
              ((xpad_rumble.lValue & 0xFF) | (xpad_rumble.rValue & 0xFF))) {
            xid_left = xpad_rumble.lValue & 0xFF;
            xid_right = xpad_rumble.rValue & 0xFF;
          }
          rumble_callback(0, xid_left, xid_right);
        }
      }
      break;
    }

    default:
      break;
  }
}

bool output_mode_uses_usb_feedback_reports() {
  switch (get_effective_output_mode()) {
    case OUTPUT_XINPUT:
    case OUTPUT_XINPUT2P:
    case OUTPUT_XINPUTW:
    case OUTPUT_XID:
      return true;
    default:
      return false;
  }
}

// One USB player from one chosen port: the other physical controllers have no
// interface. Once the source's submission is complete (or idle), consume their
// irrelevant deliveries.
#ifdef ENABLE_OUTPUT_PS5
// PS5 chosen as PC Mode or MiSTer Mode reaches a host that never checks the
// signature, so it reports unsigned until a PS5 signing dongle is fitted. Only
// a host Auto classified as a PC, or fell back on after that host talked to
// the probe with no PlayStation sign, qualifies. A host Auto never saw (a
// no-connect fallback: maybe a PS5 in rest mode) or classified as a
// PlayStation never gets an unsigned report.
inline bool ps5_unsigned_report_allowed() {
  return is_auto_output_mode_selected() &&
         (autoDetectState == AUTO_STATE_WINDOWS ||
          auto_detect_fallback_saw_generic_host()) &&
         !ps_auth_dongle_ps5_signer_connected();
}
#endif

inline void consume_unsent_source_deliveries(uint8_t source) {
  if (controllerFrameNeedsDelivery(source)) return;
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    if (i != source) completeControllerFrameDelivery(i);
  }
}

void __not_in_flash_func(send_usb_single_report)(uint8_t port) {
  const outputMode_t effectiveOutputMode = get_effective_output_mode();
  const uint8_t sourcePort = effectiveOutputMode == OUTPUT_SWITCHPRO
      ? switchpro_output_source_port(port)
      : (effectiveOutputMode == OUTPUT_SWITCH2PRO
          ? switch2pro_output_source_port()
          : (effectiveOutputMode == OUTPUT_REMOTE ? remote_output_source_port() : port));
  const uint32_t nowMs = millis();
  bool ps4KeepaliveDue = false;
  if (effectiveOutputMode == OUTPUT_PS4) {
    if (!ps4_report_clock_started) {
      ps4_report_clock_started = true;
      ps4_last_report_ms = nowMs;
    } else {
      ps4KeepaliveDue =
        (uint32_t)(nowMs - ps4_last_report_ms) >= PS4_GAMEPAD_KEEPALIVE_MS;
    }
  } else {
    ps4_report_clock_started = false;
  }

  bool force_update = (effectiveOutputMode == OUTPUT_PS3) || ps4KeepaliveDue;
  #ifdef ENABLE_OUTPUT_PS5
  force_update = force_update || (effectiveOutputMode == OUTPUT_PS5);
  #endif
  #ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
  // GIP announce, descriptor, authentication, ACK, and keepalive traffic must
  // advance even while the physical controller frame is unchanged.
  force_update = force_update || (effectiveOutputMode == OUTPUT_XBOXONE);
  #endif
  const bool pointerMousePending = observePointerMouse(port);
  const bool gamepadPending =
      controllerFrameNeedsDelivery(sourcePort) || force_update;
  if (!gamepadPending && !pointerMousePending)
    return;

  if (use_device_hid_class) {
    const uint8_t hidInterface = output_usb_hid_interface_for_source_port(port);
    if (!usb_device_hid[hidInterface]) {
      return;
    }
    bool hidReady = false;
    {
      LatencyPhaseTraceScope readyTrace(LATENCY_TRACE_PHASE_USB_READY);
      hidReady = usb_device_hid[hidInterface]->ready();
    }
    if (!hidReady && !force_update) {
#if defined(ADAPT_FEATURE_N64_CAPTURE)
      if(effectiveOutputMode == OUTPUT_SWITCHPRO) n64CaptureBusy(sourcePort, hidInterface, controllerFrameConst(sourcePort).digital_buttons);
#endif
      LatencyPhaseTraceScope notReadyTrace(LATENCY_TRACE_PHASE_USB_NOT_READY);
      return;
    }

    // Alternate endpoint opportunities when both reports are pending. A
    // continuously changing gamepad must not starve its pointer
    // mouse, and the gamepad frame remains pending while the mouse goes first.
    if (pointerMousePending &&
        pointerMouseShouldPrecedeGamepad(port, gamepadPending)) {
      sendPointerMouseReport(port, usb_device_hid[hidInterface]);
      return;
    }

    uint8_t report_id = 0;
    uint16_t report_size = 0;
    uint8_t report_data[CFG_TUD_HID_EP_BUFSIZE];
    bool ps5_prepared_hash_report = false;
    {
      LatencyPhaseTraceScope buildTrace(LATENCY_TRACE_PHASE_USB_BUILD);
      UsbLatencyBuildScope usbLatencyBuild(port);
      switch (effectiveOutputMode) {
        case OUTPUT_HID:
        case OUTPUT_MISTER:
          report_size = sizeof(_hidgp);
          map_hid_output(port);
          memcpy(report_data, &_hidgp, report_size);
          break;
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
        case OUTPUT_REMOTE: {
          const RemoteOutputReport remote = remoteOutputReport(
              controllerFrameConst(sourcePort), menuOwnsControllerInput());
          report_id = REMOTE_OUTPUT_REPORT_ID;
          report_size = sizeof(remote);
          memcpy(report_data, &remote, report_size);
          break;
        }
        case OUTPUT_MISTER_JOGCON:
        case OUTPUT_RESERVED_JOGCON:
          report_size = sizeof(_jogcon);
          map_hidjogconmister_output(port);
          memcpy(report_data, &_jogcon, report_size);
          break;
        case OUTPUT_MISTER_PADDLE:
          report_size = sizeof(_paddle);
          map_hidpaddlemister_output(port);
          memcpy(report_data, &_paddle, report_size);
          break;
        case OUTPUT_MISTER_SPINNER:
          report_id = MISTER_SPINNER_REPORT_ID;
          report_size = sizeof(_spinner);
          map_hidspinnermister_output(port);
          memcpy(report_data, &_spinner, report_size);
          break;
        case OUTPUT_ROTARY_MOUSE:
          report_id = ROTARY_MOUSE_REPORT_ID;
          report_size = sizeof(_rotary_mouse);
          map_rotary_mouse_output(port);
          memcpy(report_data, &_rotary_mouse, report_size);
          break;
        case OUTPUT_MISTER_NEGCON:
          report_size = sizeof(_negcon);
          map_hidnegconmister_output(port);
          memcpy(report_data, &_negcon, report_size);
          break;
        case OUTPUT_MISTER_GUNCON:
          report_size = sizeof(_guncon);
          map_hidgunconmister_output(port);
          memcpy(report_data, &_guncon, report_size);
          break;
#endif
        case OUTPUT_PS3:
#if defined(ADAPT_FEATURE_NATIVE_PS3_OUTPUT)
          if (ps3NativeRuntimeActive()) {
            report_id = 1;
            report_size = sizeof(ps3_data);
            map_ps3_output(port);
            memcpy(report_data, &ps3_data, report_size);
            break;
          }
#endif
          report_size = sizeof(_hidgp);
          map_hid_output(port);
          memcpy(report_data, &_hidgp, report_size);
          break;
        case OUTPUT_PS4:
          report_size = sizeof(_ps4_report);
          map_ps4_output(port);
          if (ps4KeepaliveDue) {
            _ps4_report.counter = (_ps4_report.counter + 1u) & 0x3Fu;
            _ps4_report.axis_timing = (uint16_t)nowMs;
          }
          memcpy(report_data, &_ps4_report, report_size);
          break;
        #ifdef ENABLE_OUTPUT_PS5
        case OUTPUT_PS5: {
          if (!usb_device_hid[hidInterface]->ready()) {
            return;
          }
          map_ps5_output(port);
          uint16_t ps5_report_size = sizeof(_ps5_report);
          if (!ps_auth_dongle_prepare_ps5_output_report(
                reinterpret_cast<const uint8_t*>(&_ps5_report),
                sizeof(_ps5_report),
                report_data,
                &ps5_report_size,
                sizeof(report_data))) {
            if (!ps5_unsigned_report_allowed()) {
              return;
            }
            report_size = sizeof(_ps5_report);
            memcpy(report_data, &_ps5_report, report_size);
            break;
          }
          report_size = ps5_report_size;
          ps5_prepared_hash_report = true;
          break;
        }
        #endif
        case OUTPUT_SWITCH:
          report_size = sizeof(_pokken);
          map_switch_output(port);
          memcpy(report_data, &_pokken, report_size);
          break;
        case OUTPUT_SWITCHPRO:
          report_size = 64;
          map_switchpro_output(port, sourcePort);
          memcpy(report_data, switchpro[port]->switchCommon->generate_usb_report(), report_size);
          break;
        case OUTPUT_PANTHERLORD:
          report_size = sizeof(_pantherlord);
          map_pantherlord_output(port);
          memcpy(report_data, &_pantherlord, report_size);
          break;
        case OUTPUT_MDMINI:
          report_size = sizeof(_mdmini);
          map_mdmini_output(port);
          memcpy(report_data, &_mdmini, report_size);
          break;
        default:
          break;
      }
    }

    bool submitReady = hidReady;
    if ((controllerFrameNeedsDelivery(sourcePort) || force_update) && submitReady) {
      bool submitted = false;
#if defined(ADAPT_FEATURE_N64_CAPTURE)
      if(effectiveOutputMode == OUTPUT_SWITCHPRO) n64CaptureSubmitPrepare(sourcePort, hidInterface, report_id, report_data, report_size);
#endif
      {
        LatencyPhaseTraceScope submitTrace(LATENCY_TRACE_PHASE_USB_SUBMIT);
        submitted = usb_device_hid[hidInterface]->sendReport(report_id, report_data, report_size);
      }
#if defined(ADAPT_FEATURE_JAGUAR_TRACE)
      if (effectiveOutputMode == OUTPUT_MISTER_SPINNER) {
        jaguarTraceSubmitted(sourcePort, hidInterface, report_data, report_size, submitted);
      }
#endif
#if defined(ADAPT_FEATURE_N64_CAPTURE)
      if(effectiveOutputMode == OUTPUT_SWITCHPRO) n64CaptureSubmitResult(hidInterface, submitted);
#endif
      if (!submitted) {
        return;
      }
#if defined(ADAPT_FEATURE_SWITCH_REPORT_STREAM)
      if (effectiveOutputMode == OUTPUT_SWITCHPRO) {
        switch_report_stream[port].submitted(nowMs);
      }
#endif
      if (effectiveOutputMode == OUTPUT_PS4) {
        ps4_last_report_ms = nowMs;
        ps4_report_clock_started = true;
      }
      outputUsbDiagnosticsNoteSubmit(port, hidInterface);
      #ifdef ENABLE_OUTPUT_PS5
      if (ps5_prepared_hash_report) {
        ps_auth_dongle_complete_ps5_output_report();
      }
      #endif
      if (latencyTest.isEnabled()) {
        latencyTest.noteUsbSubmit(sourcePort, controllerFrameConst(sourcePort).digital_buttons);
      }
      notePointerGamepadReportSent(port, pointerMousePending);
      completeControllerFrameDelivery(sourcePort);
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
      if (effectiveOutputMode == OUTPUT_MISTER_SPINNER) {
        // Retain motion while the endpoint is busy or submission fails. Drain
        // only this report's signed chunk, then schedule any remaining motion.
        consumeRotaryMotion(sourcePort, _spinner.spinner_axis);
        if (peekRotaryMotion(sourcePort) != 0) {
          requestControllerFrameDelivery(sourcePort);
        }
      }
      if (effectiveOutputMode == OUTPUT_ROTARY_MOUSE) {
        completeRotaryMouseReport(sourcePort);
      }
#endif
    }

  } else {
    switch (effectiveOutputMode) {
      case OUTPUT_XINPUT:
        if (_xinput->ready()) {
          {
            LatencyPhaseTraceScope buildTrace(LATENCY_TRACE_PHASE_USB_BUILD);
            UsbLatencyBuildScope usbLatencyBuild(port);
            map_xinput_output(port);
          }
          receive_xinput_report();
          bool submitted = false;
          {
            LatencyPhaseTraceScope submitTrace(LATENCY_TRACE_PHASE_USB_SUBMIT);
            submitted = _xinput->sendReport(&_xinput_report);
          }
          if (!submitted) break;
          outputUsbDiagnosticsNoteSubmit(port, 0);
          if (latencyTest.isEnabled()) {
            latencyTest.noteUsbSubmit(port, controllerFrameConst(port).digital_buttons);
          }
          completeControllerFrameDelivery(port);
        }
        break;
      case OUTPUT_XINPUT2P:
      #ifdef FORCE_XINPUT2P_SINGLE_DRIVER
        if (port == 0 && _xinput && _xinput->ready()) {
          {
            LatencyPhaseTraceScope buildTrace(LATENCY_TRACE_PHASE_USB_BUILD);
            UsbLatencyBuildScope usbLatencyBuild(port);
            map_xinput_output(port);
          }
          receive_xinput_report();
          bool submitted = false;
          {
            LatencyPhaseTraceScope submitTrace(LATENCY_TRACE_PHASE_USB_SUBMIT);
            submitted = _xinput->sendReport(&_xinput_report);
          }
          if (submitted) {
            outputUsbDiagnosticsNoteSubmit(port, 0);
            if (latencyTest.isEnabled()) {
              latencyTest.noteUsbSubmit(port, controllerFrameConst(port).digital_buttons);
            }
            completeControllerFrameDelivery(port);
          }
        }
      #elif defined(ENABLE_XINPUT2P_WIRELESS_TRANSPORT)
        if (_xinputw && port < XINPUT_WIRELESS_CONTROLLERS) {
          const uint8_t targetSlot = xinputw_target_slot_for_source(port);
          if (targetSlot == XINPUTW_INVALID_PORT) {
            completeControllerFrameDelivery(port);
            break;
          }
          // Windows completes the receiver's optional controller-info train,
          // but Linux xpad creates the child from the 08/80 presence packet
          // and never sends that train. Permit normal state packets while the
          // receiver remains in UNKNOWN1/UNKNOWN2 so MiSTer can drive the
          // child; Windows still advances the same handshake asynchronously.
          const ControllerInfoState targetState =
              _xinputw->getControllerState(targetSlot);
          if (targetState == DISCONNECTED || !_xinputw->ready(targetSlot)) {
            break;
          }
          map_xinputw_output(port);
          if (targetSlot != port) {
            _xinputw_report[targetSlot] = _xinputw_report[port];
          }
          const bool submitted = _xinputw->sendReport(
              targetSlot, &_xinputw_report[targetSlot]);
          if (submitted) {
            outputUsbDiagnosticsNoteSubmit(port, targetSlot);
            if (latencyTest.isEnabled()) {
              latencyTest.noteUsbSubmit(port, controllerFrameConst(port).digital_buttons);
            }
            completeControllerFrameDelivery(port);
          }
        }
      #endif
        break;
      case OUTPUT_XINPUTW:
        if (_xinputw && port < XINPUT_WIRELESS_CONTROLLERS &&
            controllerFrameConst(port).connected &&
            _xinputw->getControllerState(port) == NONE &&
            _xinputw->ready(port)) {
          {
            LatencyPhaseTraceScope buildTrace(LATENCY_TRACE_PHASE_USB_BUILD);
            UsbLatencyBuildScope usbLatencyBuild(port);
            map_xinputw_output(port);
          }
          bool submitted = false;
          {
            LatencyPhaseTraceScope submitTrace(LATENCY_TRACE_PHASE_USB_SUBMIT);
            submitted = _xinputw->sendReport(port, &_xinputw_report[port]);
          }
          if (submitted) {
            outputUsbDiagnosticsNoteSubmit(port, port);
            if (latencyTest.isEnabled()) {
              latencyTest.noteUsbSubmit(port, controllerFrameConst(port).digital_buttons);
            }
            completeControllerFrameDelivery(port);
          }
        }
        break;
      case OUTPUT_XID: {
        if (_xid && tud_mounted()) {
          uint8_t ep_in = (_xid->stored_ep_in != 0) ? _xid->stored_ep_in : 0x81;
          uint8_t ep_out = (_xid->stored_ep_out != 0) ? _xid->stored_ep_out : 0x02;
          xid_manual_init(0, _xid->stored_itfnum, ep_in, ep_out);
        }
        int8_t xid_index = xid_get_index_by_type(0, XID_TYPE_WHEEL);
        if (xid_index >= 0 && xid_send_report_ready(xid_index)) {
          {
            LatencyPhaseTraceScope buildTrace(LATENCY_TRACE_PHASE_USB_BUILD);
            UsbLatencyBuildScope usbLatencyBuild(port);
            map_xid_output(port);
          }
          bool submitted = false;
          {
            LatencyPhaseTraceScope submitTrace(LATENCY_TRACE_PHASE_USB_SUBMIT);
            submitted = xid_send_report(xid_index, &xpad_data, sizeof(xpad_data));
          }
          usb_delivery_runtime_detail::completeIfSubmitted(submitted, [&]() {
            usbLatencyTraceNoteSubmit(port, 0);
            if (latencyTest.isEnabled()) {
              latencyTest.noteUsbSubmit(port, controllerFrameConst(port).digital_buttons);
            }
            completeControllerFrameDelivery(port);
          });
        }
        break;
      }
      case OUTPUT_SWITCH2PRO:
        if (port == 0) {
          bool submitted = false;
          {
            UsbLatencyBuildScope usbLatencyBuild(port);
            LatencyPhaseTraceScope submitTrace(LATENCY_TRACE_PHASE_USB_SUBMIT);
            submitted = send_switch2pro_report(sourcePort);
          }
          if (!submitted) break;
          outputUsbDiagnosticsNoteSubmit(port, 0);
          if (latencyTest.isEnabled()) {
            latencyTest.noteUsbSubmit(sourcePort, controllerFrameConst(sourcePort).digital_buttons);
          }
          completeControllerFrameDelivery(sourcePort);
        }
        break;
      #ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
      case OUTPUT_XBOXONE:
        if (_xboxone) {
          bool submitted = false;
          {
            UsbLatencyBuildScope usbLatencyBuild(port);
            submitted = _xboxone->sendReport(port);
          }
          if (!submitted) break;
          usbLatencyTraceNoteSubmit(port, port);
          if (latencyTest.isEnabled()) {
            latencyTest.noteUsbSubmit(port, controllerFrameConst(port).digital_buttons);
          }
          completeControllerFrameDelivery(port);
        }
        break;
      #endif
      default:
        break;
    }
  }
}

void __not_in_flash_func(send_usb_report)() {
#ifdef ENABLE_OUTPUT_JVS
  if (outputMode == OUTPUT_JVS) {
    jvsOutput.update();
    return;
  }
#endif

  if (can_run_usb_detection()) {
    return;
  }

  service_xinput2p_wireless_slot_routing();
  const outputMode_t effectiveOutputMode = get_effective_output_mode();

  if (effectiveOutputMode == OUTPUT_REMOTE) {
    static uint8_t lastRemoteSource = REMOTE_OUTPUT_NO_SOURCE;
    const uint8_t source = remote_output_source_port();
    if (source != lastRemoteSource) {
      requestControllerFrameDelivery(source);
      lastRemoteSource = source;
    }
    send_usb_single_report(0);
    consume_unsent_source_deliveries(source);
    return;
  }

  // Host feedback (rumble, LEDs, etc.) can arrive while the controller is idle.
  // Keep OUT endpoints armed even when there is no new input report to send.
  {
    LatencyPhaseTraceScope feedbackTrace(LATENCY_TRACE_PHASE_USB_FEEDBACK);
    if (output_mode_uses_usb_feedback_reports()) {
      service_usb_feedback_reports();
    }
    if (effectiveOutputMode == OUTPUT_SWITCH2PRO && _switch2pro) {
      _switch2pro->task();
    }
  }

  if (effectiveOutputMode == OUTPUT_KEYBOARD) {
    send_usb_keyboard_report();
    return;
  }

  if (effectiveOutputMode == OUTPUT_XINPUTW
  #ifdef ENABLE_XINPUT2P_WIRELESS_TRANSPORT
      || effectiveOutputMode == OUTPUT_XINPUT2P
  #endif
      ) {
    if (_xinputw) {
      for (uint8_t target = 0; target < XINPUT_WIRELESS_CONTROLLERS; ++target) {
        _xinputw->set_connected(target, xinput2p_target_connected(target));
      }
      _xinputw->driver_task();
    }
  }

  if (grouped_devices) {
    for (uint8_t i = 0; i < grouped_devices; ++i) {
      send_usb_grouped_report(i);
    }
  } else {
    const uint8_t outputPlayers = output_usb_player_count();
    for (uint8_t i = 0; i < outputPlayers; ++i) {
      send_usb_single_report(i);
    }
    if (effectiveOutputMode == OUTPUT_SWITCH2PRO) {
      consume_unsent_source_deliveries(switch2pro_output_source_port());
    }
  }
}

#endif  // !defined(ADAPT_USB_DELIVERY_HELPERS_ONLY)
