#pragma once

// Common platform services. ADAPT_HAS_* describes fitted hardware;
// ADAPT_FEATURE_* describes the service compiled into this firmware target.
#if defined(ADAPT_HAS_OLED) && !defined(DISABLE_OLED_DISPLAY) && \
    !defined(ADAPT_HARDWARE_STUB)
  #define ADAPT_FEATURE_OLED_DISPLAY 1
#endif
#if defined(ADAPT_HAS_BUZZER) && !defined(DISABLE_BUZZER_HARDWARE) && \
    !defined(ADAPT_HARDWARE_STUB)
  #define ADAPT_FEATURE_BUZZER 1
#endif
#if defined(ADAPT_HAS_RGB_LED) && !defined(DISABLE_RGB_LED_PIO) && \
    !defined(ADAPT_HARDWARE_STUB)
  #define ADAPT_FEATURE_RGB_LED 1
#endif
#if defined(ADAPT_HAS_MODE_BUTTON) && \
    !defined(DISABLE_MODE_BUTTON_HARDWARE) && !defined(ADAPT_HARDWARE_STUB)
  #define ADAPT_FEATURE_MODE_BUTTON 1
#endif
#if defined(ADAPT_HAS_RESET_BUTTON) && !defined(ADAPT_HARDWARE_STUB)
  #define ADAPT_FEATURE_RESET_BUTTON 1
#endif
// First-boot setup runs on the OLED and needs both adapter buttons: one moves
// and one accepts, so it can be completed without a controller attached.
#if ADAPT_POLICY_FIRST_BOOT_SETUP && defined(ADAPT_FEATURE_OLED_DISPLAY) && \
    defined(ADAPT_FEATURE_MODE_BUTTON)
  #define ADAPT_FEATURE_FIRST_BOOT_SETUP 1
#endif
#if defined(ADAPT_HAS_DEBUG_UART) && defined(ENABLE_DEBUG_UART) && \
    !defined(ADAPT_HARDWARE_STUB)
  #define ADAPT_FEATURE_DEBUG_UART 1
#endif
#if defined(ADAPT_HAS_NFC_BRIDGE) && !defined(DISABLE_NFC_BRIDGE) && \
    !defined(ADAPT_HARDWARE_STUB)
  #define ADAPT_FEATURE_NFC_BRIDGE 1
  #ifndef ENABLE_NFC
    #define ENABLE_NFC 1
  #endif
#endif
#if defined(ADAPT_HAS_RAIL_MONITOR) && !defined(DISABLE_RAIL_MONITOR) && \
    !defined(ADAPT_HARDWARE_STUB)
  #define ADAPT_FEATURE_RAIL_MONITOR 1
#endif

#if defined(ADAPT_INPUT_RETRO) && !defined(ADAPT_HARDWARE_STUB)
  #define ENABLE_INPUT_DREAMCAST
  #define ENABLE_INPUT_N64
  #define ENABLE_INPUT_GAMECUBE
  #define ENABLE_INPUT_SATURN
  #define ENABLE_INPUT_MEGADRIVE
  #define ENABLE_INPUT_PSX
  #define ENABLE_INPUT_PSX_JOG
  #define ENABLE_INPUT_NES
  #define ENABLE_INPUT_SNES
  #define ENABLE_INPUT_VBOY
  #define ENABLE_INPUT_PCE
  #define ENABLE_INPUT_WII
  #define ENABLE_INPUT_NEOGEO
  #define ENABLE_INPUT_3DO
  #define ENABLE_INPUT_JAGUAR
  #define ENABLE_INPUT_DRIVING
  #define ENABLE_INPUT_SMS
  #define ENABLE_INPUT_JPC
  #define ENABLE_INPUT_INTV
  #define ENABLE_INPUT_COLECOVISION
  #define ENABLE_INPUT_CDI
  #if !defined(CLASSIC2USB_DISABLE_INPUT_AUTODETECT)
    #define ENABLE_INPUT_AUTODETECT
  #endif
  #if !defined(DISABLE_SERIAL_MEMCARD_API)
    #define ADAPT_FEATURE_SERIAL_MEMCARD_API 1
  #endif
  // MiSTer Main's session and card requests on the control CDC
  // (core/mister_serial_device.h).
  #if defined(ADAPT_FEATURE_SERIAL_MEMCARD_API) && \
      defined(ENABLE_RUNTIME_SERIAL_DEBUG_CDC) && \
      !defined(DISABLE_MISTER_SERIAL_REQUESTS)
    #define ADAPT_FEATURE_MISTER_SERIAL_REQUESTS 1
  #endif
  #if defined(ADAPT_FEATURE_OLED_DISPLAY) && \
      !defined(DISABLE_TTY2OLED_SERIAL)
    #define ENABLE_TTY2OLED_SERIAL 1
  #endif
  #if defined(ADAPT_FEATURE_MISTER_SERIAL_REQUESTS) && \
      defined(ADAPT_FEATURE_OLED_DISPLAY)
    #define ADAPT_FEATURE_MISTER_SERIAL_STATUS 1
  #endif
  #if defined(ENABLE_INPUT_AUTODETECT) && !defined(AUTODETECT_ENABLE_PASSIVE_ASSIST)
    #define AUTODETECT_ENABLE_PASSIVE_ASSIST 1
  #endif
#endif

// Opt-in, read-only USB drive (features/usb_drive). TinyUSB's MSC class is
// compiled only into products that declare the drive (platformio.ini).
#if defined(ADAPT_OUTPUT_USB_DEVICE) && defined(ADAPT_HAS_USB_DRIVE) && \
    !defined(DISABLE_USB_DRIVE)
  #define ADAPT_FEATURE_USB_DRIVE 1
#endif

// Tester-only PSX tracing needs the existing read-only drive and retro input.
#if defined(ADAPT_ENABLE_PSX_IDENTIFICATION_CAPTURE)
  #if !defined(ADAPT_FEATURE_USB_DRIVE) || !defined(ENABLE_INPUT_PSX) || \
      !defined(ENABLE_INPUT_AUTODETECT)
    #error "PSX identification capture requires PSX, Auto input and USB drive"
  #endif
  #define ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE 1
#endif

#if defined(ADAPT_ENABLE_PS2_IR_CAPTURE)
  #if !defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
    #error "PS2 IR capture requires the PSX identification capture transport"
  #endif
  #define ADAPT_FEATURE_PS2_IR_CAPTURE 1
#endif

#if defined(ADAPT_ENABLE_GENESIS_IDENTIFICATION_CAPTURE)
  #if !defined(ADAPT_FEATURE_USB_DRIVE) || !defined(ENABLE_INPUT_MEGADRIVE) || \
      !defined(ENABLE_INPUT_AUTODETECT)
    #error "Genesis capture requires Mega Drive, Auto input and USB drive"
  #endif
  #if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
    #error "Identification captures require separate images and MSC overlays"
  #endif
  #define ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE 1
#endif

#if defined(ADAPT_ENABLE_JAGUAR_TRACE)
  #if !defined(ENABLE_INPUT_JAGUAR) || !defined(ADAPT_BRIDGE_CLASSIC_TO_USB) || \
      !defined(ENABLE_RUNTIME_SERIAL_DEBUG_CDC)
    #error "Jaguar trace requires classic Jaguar input, USB device output and CDC"
  #endif
  #define ADAPT_FEATURE_JAGUAR_TRACE 1
#endif

#if defined(ADAPT_ENABLE_JOGCON_CAPTURE)
  #if !defined(ADAPT_FEATURE_USB_DRIVE) || !defined(ENABLE_INPUT_PSX)
    #error "JogCon capture requires PSX input and USB drive"
  #endif
  #if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE) || defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
    #error "Capture overlays require separate images"
  #endif
  #define ADAPT_FEATURE_JOGCON_CAPTURE 1
#elif defined(ADAPT_JOGCON_CAPTURE_MOTOR_OFF)
  #error "JogCon motor-off requires capture"
#endif

#if defined(ADAPT_ENABLE_N64_CAPTURE)
  #if !defined(ADAPT_FEATURE_USB_DRIVE) || !defined(ENABLE_INPUT_N64)
    #error "N64 capture requires N64 input and USB drive"
  #endif
  #if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE) || defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE) || defined(ADAPT_FEATURE_JOGCON_CAPTURE)
    #error "Capture overlays require separate images"
  #endif
  #define ADAPT_FEATURE_N64_CAPTURE 1
#endif

#if defined(ADAPT_ENABLE_HOST_CAPTURE)
  #if !defined(ADAPT_FEATURE_USB_DRIVE) || !defined(ADAPT_BRIDGE_CLASSIC_TO_USB) || \
      !defined(FORCE_CLASSIC_AUTO_OUTPUT)
    #error "Host capture is a Classic2USB USB-drive image that forces Auto output"
  #endif
  #if defined(ADAPT_ENABLE_N64_CAPTURE) || defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE) || \
      defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE) || defined(ADAPT_FEATURE_JOGCON_CAPTURE)
    #error "Capture overlays require separate images"
  #endif
  #define ADAPT_FEATURE_HOST_CAPTURE 1
#endif

// Switch Pro output re-sends the finalized frame every 8 ms, like a real Pro
// Controller, so one report the console ignores cannot leave a stale state.
// ADAPT_DISABLE_SWITCH_REPORT_STREAM restores change-only delivery for A/B.
#if defined(ADAPT_OUTPUT_USB_DEVICE) && !defined(ADAPT_DISABLE_SWITCH_REPORT_STREAM)
  #define ADAPT_FEATURE_SWITCH_REPORT_STREAM 1
#endif

#if defined(ADAPT_INPUT_DIRECT_GPIO) && !defined(ADAPT_HARDWARE_STUB)
  #define ENABLE_INPUT_CUSTOM
#endif

#if defined(ADAPT_INPUT_USB_HOST) && !defined(ADAPT_HARDWARE_STUB)
  #define ENABLE_INPUT_USB
  #define ENABLE_USB_XINPUT_HOST
#endif

// The unknown-DInput mapping wizard is an optional OLED UI module. Headless
// USB-host products retain deterministic descriptor/database defaults and do
// not carry interactive mapping state.
#if defined(ENABLE_INPUT_USB) && defined(ADAPT_FEATURE_OLED_DISPLAY)
  #define ADAPT_FEATURE_DINPUT_MAPPING_WIZARD 1
#endif

#ifdef ADAPT_INPUT_JVS
  #if !defined(ADAPT_HARDWARE_STUB)
    #define ENABLE_INPUT_JVS
  #endif
#endif

#if defined(ADAPT_BRIDGE_JVS_TO_USB)
  #if defined(ENABLE_INPUT_USB) || defined(ENABLE_USB_BT_HCI)
    #error "Production JVS ingress keeps its auxiliary USB port auth-only"
  #endif
#endif

#if !defined(DISABLE_USB_AUTH_SIDECAR) && defined(ADAPT_HAS_USB_AUTH_SIDECAR)
  #define ADAPT_FEATURE_USB_AUTH_SIDECAR 1
  #define ENABLE_USB_AUTH_SIDECAR 1
#endif

// Debug images only: a serial TRIAL runs an output unsaved and restarts into
// the saved one (features/output_trial/). No shipping env sets the flag, and
// the [env] build filter leaves the folder out of every other build.
#if defined(ENABLE_OUTPUT_TRIAL)
  #if !defined(ADAPT_OUTPUT_USB_DEVICE)
    #error "The output trial needs a USB-device output"
  #endif
  #if defined(ADAPT_ENABLE_HOST_CAPTURE) || defined(ADAPT_ENABLE_N64_CAPTURE) || \
      defined(ADAPT_ENABLE_JOGCON_CAPTURE) || defined(FORCE_CLASSIC_AUTO_OUTPUT)
    #error "Capture images force their own output; keep the output trial separate"
  #endif
  #define ADAPT_FEATURE_OUTPUT_TRIAL 1
#endif

#if defined(ADAPT_PRIMARY_EGRESS_CLASSIC_CONSOLE) && !defined(ADAPT_HARDWARE_STUB)
  #define ENABLE_OUTPUT_CONSOLE 1
#endif
#if defined(ADAPT_PRIMARY_EGRESS_DB15) && !defined(ADAPT_HARDWARE_STUB)
  #define ENABLE_OUTPUT_DB15 1
#endif
#ifdef ADAPT_OUTPUT_USB_DEVICE
  #define ENABLE_OUTPUT_USB_MODES 1
  #ifndef ENABLE_EXPERIMENTAL_XINPUT2P_OUTPUT
    #define ENABLE_EXPERIMENTAL_XINPUT2P_OUTPUT 1
  #endif
  // Multi-player XInput is the Xbox 360 wireless receiver on every USB-device
  // image (2026-10-19); the wired XInputMulti transport was removed.
  #ifndef ENABLE_XINPUT2P_WIRELESS_TRANSPORT
    #define ENABLE_XINPUT2P_WIRELESS_TRANSPORT 1
  #endif
#endif
// Native wired PS3 is the selected product's runtime output policy. The old
// diagnostic flag remains accepted for external diagnostic configurations.
#if defined(ADAPT_OUTPUT_USB_DEVICE) && !defined(ADAPT_HARDWARE_STUB) && \
    (ADAPT_POLICY_NATIVE_PS3_OUTPUT || defined(ADAPT_ENABLE_PS3_NATIVE_DIAGNOSTIC))
  #define ADAPT_FEATURE_NATIVE_PS3_OUTPUT 1
#endif
#if defined(ADAPT_OUTPUT_USB_DEVICE) && defined(ADAPT_CAP_NATIVE_PS5_OUTPUT) && \
    defined(ADAPT_FEATURE_USB_AUTH_SIDECAR)
  #ifndef ENABLE_OUTPUT_PS5
    #define ENABLE_OUTPUT_PS5 1
  #endif
#elif defined(ENABLE_OUTPUT_PS5)
  #error "Native PS5 output requires a fitted and enabled USB authentication sidecar"
#endif
#if defined(ADAPT_BRIDGE_USB_TO_JVS) && !defined(ADAPT_HARDWARE_STUB)
  #define ENABLE_OUTPUT_JVS 1
#endif
