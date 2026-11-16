#include "../product_config.h"

#include "../core/device_runtime_state.h"
#include "runtime/output_boot_bridge.h"
#include "runtime/input_runtime_output_bridge.h"
#include "output_capabilities.h"

extern const uint16_t RZORD1_VID = ADAPT_PRODUCT_USB_VID;
extern const uint16_t RZORD1_PID = ADAPT_PRODUCT_USB_PID;
// Keep the legacy Reflex PID only for MiSTer PSX special modes so existing
// MiSTer main quirk handling continues to match those descriptors.
extern const uint16_t RZORD1_MISTER_PID = 0x127E;
extern const uint16_t RZORD1_VERSION = 0x0100;
extern const char* const RZORD1_VENDOR = "MiSTerAddons";
extern const char* const RZORD1_PPRODUCT = "Adapt " PRODUCT_SHORT_NAME;
extern const uint8_t bcd_input_revision = 1;

namespace {

constexpr uint16_t kReflexSharedMisterPid = 0x1460;

}  // namespace

const char* get_reflex_input_product_name() {
  if (output_effective_mode_is(OUTPUT_MISTER_GUNCON)) return "ReflexPSGun";
  if (output_effective_mode_is(OUTPUT_MISTER_NEGCON)) return "ReflexPSWheel";
  if (output_effective_mode_is(OUTPUT_MISTER_PADDLE)) return "MiSTer-A1 Paddle";
  if (output_effective_mode_is(OUTPUT_MISTER_SPINNER)) return "MiSTer-A1 Spinner";
  if (output_effective_mode_is(OUTPUT_ROTARY_MOUSE)) return "ReflexMouse";
  if (output_effective_mode_is(OUTPUT_REMOTE)) return "ReflexRemote";
  if (output_effective_mode_is(OUTPUT_MISTER_JOGCON)) return "ReflexPSJogCon";

  switch (deviceMode) {
#ifdef ENABLE_INPUT_N64
    case RZORD_N64: return "ReflexN64";
#endif
#ifdef ENABLE_INPUT_GAMECUBE
    case RZORD_GAMECUBE: return "ReflexGC";
#endif
#ifdef ENABLE_INPUT_GBA
    case RZORD_GBA: return "ReflexGBA";
#endif
#ifdef ENABLE_INPUT_NES
    case RZORD_NES: return "ReflexNES";
#endif
#ifdef ENABLE_INPUT_SNES
    case RZORD_SNES: return "ReflexSNES";
#endif
#ifdef ENABLE_INPUT_VBOY
    case RZORD_VBOY: return "ReflexVboy";
#endif
#ifdef ENABLE_INPUT_MEGADRIVE
    case RZORD_MEGADRIVE: return "ReflexMD";
#endif
#ifdef ENABLE_INPUT_SATURN
    case RZORD_SATURN:
      return bcd_device_version.platform == BCD_PLAT_SATURN &&
             bcd_device_version.platform_sub == BCD_SATURN_3D_PLATFORM_SUB
        ? "ReflexSat3D" : "ReflexSat";
#endif
#ifdef ENABLE_INPUT_WII
    case RZORD_WII: return "ReflexWii";
#endif
#ifdef ENABLE_INPUT_PCE
    case RZORD_PCE: return "ReflexPCE";
#endif
#ifdef ENABLE_INPUT_PSX
    case RZORD_PSX:
      if (bcd_device_version.platform == BCD_PLAT_PSX &&
          bcd_device_version.platform_sub == BCD_NEGCON_PAD_PLATFORM_SUB) {
        return "ReflexPSNeGcon";
      }
      return bcd_device_version.platform == BCD_PLAT_PSX &&
             bcd_device_version.platform_sub == BCD_DUALSHOCK2_PLATFORM_SUB
        ? "ReflexPSDS2" : "ReflexPSDS1";
#endif
#ifdef ENABLE_INPUT_PSX_DANCE
    case RZORD_PSX_DANCE: return "ReflexPSDance";
#endif
#ifdef ENABLE_INPUT_NEOGEO
    case RZORD_NEOGEO: return "ReflexNeoGeo";
#endif
#ifdef ENABLE_INPUT_3DO
    case RZORD_3DO: return "Reflex3DO";
#endif
#ifdef ENABLE_INPUT_JAGUAR
    case RZORD_JAGUAR: return "ReflexJag";
#endif
#ifdef ENABLE_INPUT_DREAMCAST
    case RZORD_DREAMCAST: return "ReflexDreamcast";
#endif
#ifdef ENABLE_INPUT_INTV
    case RZORD_INTV: return "ReflexIntv";
#endif
#ifdef ENABLE_INPUT_COLECOVISION
    case RZORD_COLECOVISION: return "ReflexColeco";
#endif
#ifdef ENABLE_INPUT_CDI
    case RZORD_CDI: return "ReflexCDi";
#endif
#ifdef ENABLE_INPUT_PADDLE
    case RZORD_PADDLE: return "ReflexPaddle";
#endif
#ifdef ENABLE_INPUT_DRIVING
    case RZORD_DRIVING: return "ReflexDriving";
#endif
#ifdef ENABLE_INPUT_GAMEPORT
    case RZORD_GAMEPORT: return "ReflexGameport";
#endif
#ifdef ENABLE_INPUT_MEMCARD
    case RZORD_MEMCARD: return "ReflexMemCard";
#endif
#ifdef ENABLE_INPUT_SMS
    case RZORD_SMS: return "ReflexSMS";
#endif
#ifdef ENABLE_INPUT_JPC
    case RZORD_JPC: return "ReflexJPC";
#endif
#ifdef ENABLE_INPUT_JVS
    case RZORD_JVS: return "ReflexJVS";
#endif
#ifdef ENABLE_INPUT_USB
    case RZORD_USB: return "ReflexUSB";
#endif
    default: return RZORD1_PPRODUCT;
  }
}

const char* get_reflex_input_usb_serial_descriptor() {
  // MiSTer includes the USB serial string in custom .map filenames. Classic2USB
  // therefore keeps its exact input-mode identity, while fixed-input products
  // use a stable product-family serial so their maps cannot replace the
  // Classic2USB PSX map despite sharing its GameControllerDB GUID.
#if ADAPT_POLICY_DYNAMIC_MISTER_INPUT_IDENTITY
  if (deviceMode != RZORD_AUTODETECT && deviceMode != RZORD_NONE) {
    return get_reflex_input_product_name();
  }
#endif
#ifdef PRODUCT_MISTER_MAP_SERIAL
  return PRODUCT_MISTER_MAP_SERIAL;
#else
  return get_reflex_input_product_name();
#endif
}

bool uses_reflex_psx_mister_mapping_identity() {
#if ADAPT_POLICY_ALWAYS_REFLEX_PSX_MISTER_IDENTITY
  return true;
#elif ADAPT_POLICY_REFLEX_PSX_MISTER_IDENTITY_WHEN_PSX && defined(ENABLE_INPUT_PSX)
  return deviceMode == RZORD_PSX;
#else
  return false;
#endif
}

uint16_t get_reflex_mister_hid_pid() {
  return uses_reflex_psx_mister_mapping_identity()
    ? kReflexSharedMisterPid
    : RZORD1_PID;
}

uint16_t get_reflex_mister_hid_device_version() {
#if ADAPT_POLICY_ALWAYS_REFLEX_PSX_MISTER_IDENTITY
  // Every product with one fixed MiSTer identity shares the DualShock 2 rows,
  // which read L2/R2 from axes 4/5 (owner, 2026-11-12).
  return encode_bcd_device_version(bcd_device_version.revision, BCD_PLAT_PSX,
                                   BCD_DUALSHOCK2_PLATFORM_SUB);
#else
#if ADAPT_POLICY_DYNAMIC_MISTER_INPUT_IDENTITY
  if (bcd_device_version.platform == BCD_PLAT_PSX) {
    return current_bcd_device_version();
  }
#endif
  return uses_reflex_psx_mister_mapping_identity()
    ? encode_bcd_device_version(bcd_device_version.revision, BCD_PLAT_PSX)
    : current_bcd_device_version();
#endif
}
