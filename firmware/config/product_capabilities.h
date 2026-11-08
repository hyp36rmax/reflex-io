#pragma once

// Product behavior is derived from ingress/egress roles. Runtime modules must
// depend on these capabilities instead of naming a retail product directly.
#if defined(ADAPT_PRIMARY_INGRESS_RETRO) && defined(ADAPT_PRIMARY_EGRESS_USB_HOST)
  #define ADAPT_BRIDGE_CLASSIC_TO_USB 1
#endif
#if defined(ADAPT_PRIMARY_INGRESS_USB_DEVICE) && defined(ADAPT_PRIMARY_EGRESS_USB_HOST)
  #define ADAPT_BRIDGE_USB_TO_USB 1
#endif
#if defined(ADAPT_PRIMARY_INGRESS_ESP32) && defined(ADAPT_PRIMARY_EGRESS_USB_HOST)
  #define ADAPT_BRIDGE_ESP32_TO_USB 1
#endif
#if defined(ADAPT_PRIMARY_INGRESS_JVS) && defined(ADAPT_PRIMARY_EGRESS_USB_HOST)
  #define ADAPT_BRIDGE_JVS_TO_USB 1
#endif
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  // Settings and remap saves survive power loss through a two-sector journal
  // (core/settings_journal.h).
  #define ADAPT_CAP_SETTINGS_JOURNAL 1
#endif
#if defined(ADAPT_PRIMARY_INGRESS_DIRECT_GPIO) && defined(ADAPT_PRIMARY_EGRESS_USB_HOST)
  #define ADAPT_BRIDGE_DIRECT_GPIO_TO_USB 1
#endif
#if defined(ADAPT_PRIMARY_INGRESS_USB_DEVICE) && defined(ADAPT_PRIMARY_EGRESS_CLASSIC_CONSOLE)
  #define ADAPT_BRIDGE_USB_TO_CLASSIC 1
#endif
#if defined(ADAPT_PRIMARY_INGRESS_RETRO) && defined(ADAPT_PRIMARY_EGRESS_CLASSIC_CONSOLE)
  #define ADAPT_BRIDGE_CLASSIC_TO_CLASSIC 1
#endif
#if defined(ADAPT_PRIMARY_INGRESS_USB_DEVICE) && defined(ADAPT_PRIMARY_EGRESS_DB15)
  #define ADAPT_BRIDGE_USB_TO_DB15 1
#endif
#if defined(ADAPT_PRIMARY_INGRESS_USB_DEVICE) && defined(ADAPT_PRIMARY_EGRESS_JVS_BOARD)
  #define ADAPT_BRIDGE_USB_TO_JVS 1
#endif

#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) || defined(ADAPT_BRIDGE_USB_TO_USB) || \
    defined(ADAPT_BRIDGE_ESP32_TO_USB)
  #define ADAPT_CAP_DUAL_INPUT_MERGE 1
#endif
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) || defined(ADAPT_BRIDGE_USB_TO_USB) || \
    defined(ADAPT_BRIDGE_JVS_TO_USB)
  #define ADAPT_CAP_DIRECTIONAL_HOTKEYS 1
#endif
// Classic controllers remap their D-pad directions too (core/remap_table.h).
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  #define ADAPT_CAP_DPAD_REMAP 1
#endif
#if defined(ADAPT_BRIDGE_USB_TO_USB)
  #define ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES 1
  #define ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT 1
  #define ADAPT_CAP_OUTPUT_TRANSPORT_SELECTION 1
  #define ADAPT_CAP_LEARNED_HID_MAPPINGS 1
  #define ADAPT_CAP_USB_INPUT_ANALOG_PROFILES 1
  #define ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE 1
#endif
#if defined(ADAPT_BRIDGE_USB_TO_DB15)
  // USB2DB15 drives normalized arcade pins directly, so use the pinned Linux
  // SDL identity, name, and button-order mapping layers as its canonical USB
  // controller database. MODERN2USB's rollback app region cannot currently
  // carry the full catalog; learned HID mappings remain a separate capability.
  #define ADAPT_CAP_SDL_CONTROLLER_IDENTITIES 1
  #define ADAPT_CAP_SDL_CONTROLLER_MAPPINGS 1
  // One USB port means a taught controller is unplugged before the adapter
  // reaches a PC, so each finished gamecontrollerdb line is cached in flash.
  #define ADAPT_CAP_MAPPING_EXPORT_STORE 1
  // A DB15 bridge drives live parallel lines for one seated player, so it owns
  // a per-game control mapping session: its own menu pages and selector, remap
  // rows that bind each controller source to an arcade control, a turbo phase
  // model driven from each source press, and game artwork on the idle screen.
  // Shared modules gate that behavior on this capability instead of naming the
  // retail product.
  #define ADAPT_CAP_ARCADE_MAPPING_SESSION 1
#endif
#if defined(ADAPT_BRIDGE_ESP32_TO_USB)
  #define ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT 1
  #define ADAPT_CAP_OUTPUT_TRANSPORT_SELECTION 1
  #define ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE 1
#endif
#if defined(ADAPT_BRIDGE_JVS_TO_USB)
  #define ADAPT_CAP_JVS_INPUT_STATUS 1
#endif
#if defined(ADAPT_BRIDGE_DIRECT_GPIO_TO_USB)
  #define ADAPT_CAP_DIRECT_GPIO_INPUT 1
#endif
#if defined(ADAPT_PRIMARY_EGRESS_USB_HOST)
  #define ADAPT_CAP_HOST_OUTPUT_PREFERENCES 1
  #define ADAPT_CAP_USB_OUTPUT_AUTODETECT 1
#endif
// Native PS5 output is a donor-assisted protocol. A USB-output product must
// have the physically fitted authentication sidecar before firmware may even
// compile or advertise that personality. Products without it retain PS4
// compatibility and sanitize persisted/raw PS5 selections back to PS4.
#if defined(ADAPT_PRIMARY_EGRESS_USB_HOST) && defined(ADAPT_HAS_USB_AUTH_SIDECAR)
  #define ADAPT_CAP_NATIVE_PS5_OUTPUT 1
#endif
#if defined(ADAPT_PRIMARY_INGRESS_RETRO)
  #define ADAPT_CAP_RETRO_MODE_SETTINGS 1
#endif
#if defined(ADAPT_PRIMARY_INGRESS_JVS) || defined(ADAPT_PRIMARY_EGRESS_JVS_BOARD)
  #define ADAPT_CAP_JVS_UI 1
#endif
#if defined(ADAPT_HARDWARE_STUB)
  #define ADAPT_HARDWARE_DEFINED 0
  #define ADAPT_RELEASE_ELIGIBLE 0
#else
  #define ADAPT_HARDWARE_DEFINED 1
  #define ADAPT_RELEASE_ELIGIBLE 1
#endif
