#pragma once

#include <stddef.h>
#include <stdint.h>

// Shared RP2040<->ESP32 SPI wire contract. The ESP32 owns Bluetooth HID
// pairing/decoding and publishes neutral-frame controller state using this
// fixed little-endian packet.
constexpr uint8_t ESP32_SPI_BRIDGE_MAX_PLAYERS = 4;
constexpr uint8_t ESP32_SPI_BRIDGE_MAX_BONDS = 8;
// The optional BLE HID output overlay is capability-detected through its RBOS
// magic and flag bits. Keep base version 4 so RP2040-only updates retain
// Bluetooth input compatibility with the previous ESP32 image.
constexpr uint8_t ESP32_SPI_BRIDGE_VERSION = 4;
constexpr uint32_t ESP32_SPI_BRIDGE_MAGIC = 0x32425452u;  // "RBT2"
constexpr uint32_t ESP32_SPI_CONTROL_MAGIC = 0x43425452u; // "RBTC"
constexpr uint32_t ESP32_SPI_PORTAL_REQUEST_MAGIC = 0x51505452u;  // "RTPQ"
constexpr uint32_t ESP32_SPI_PORTAL_RESPONSE_MAGIC = 0x53505452u; // "RTPS"
constexpr uint32_t ESP32_SPI_BT_OUTPUT_FRAME_MAGIC = 0x464F4252u; // "RBOF"
constexpr uint32_t ESP32_SPI_BT_OUTPUT_STATUS_MAGIC = 0x534F4252u; // "RBOS"
constexpr uint32_t ESP32_SPI_SONY_ASSOCIATION_MAGIC = 0x41534252u; // "RBSA"
constexpr uint32_t ESP32_SPI_BT_OUTPUT_DIAGNOSTICS_MAGIC =
    0x444F4252u; // "RBOD"
constexpr uint32_t ESP32_SPI_PS4_SIGN_REQUEST_MAGIC = 0x51533452u;  // "R4SQ"
constexpr uint32_t ESP32_SPI_PS4_SIGN_RESPONSE_MAGIC = 0x52533452u; // "R4SR"
constexpr uint8_t ESP32_SPI_BT_OUTPUT_DIAGNOSTICS_VERSION = 3;
// The signed updater deliberately uses distinct request/response magic from
// the earlier unsigned development protocol. This keeps the base v4 bridge
// shape compatible for controllers while making a mixed old/new RP2040-ESP32
// pair fail closed instead of letting an unsigned RP2040 updater treat the
// signature trailer as ordinary firmware bytes.
constexpr uint32_t ESP32_SPI_RP_UPDATE_REQUEST_MAGIC = 0x51535052u;  // "RPSQ"
constexpr uint32_t ESP32_SPI_RP_UPDATE_RESPONSE_MAGIC = 0x44535052u; // "RPSD"

// Marker embedded once in every MODERN2USB RP2040 application image. The ESP32
// scans an uploaded raw .bin for this record before it will arm an update.
constexpr uint32_t REFLEX_RP2040_FIRMWARE_MANIFEST_MAGIC = 0x32465752u; // "RWF2"
constexpr uint32_t REFLEX_RP2040_FIRMWARE_PRODUCT_MAGIC = 0x53553255u;  // "U2US"
constexpr uint8_t REFLEX_RP2040_FIRMWARE_MANIFEST_SCHEMA = 1;
constexpr uint8_t REFLEX_RP2040_FIRMWARE_PRODUCT_MODERN2USB = 1;
constexpr uint8_t REFLEX_RP2040_FIRMWARE_MCU_RP2040 = 1;
constexpr uint8_t REFLEX_RP2040_FIRMWARE_BOOT_LAYOUT_PICO_OTA_V1 = 1;
constexpr uint8_t REFLEX_RP2040_FIRMWARE_BOOT_LAYOUT_STAGE3_V2 = 2;
constexpr uint32_t REFLEX_RP2040_FIRMWARE_STAGE3_SIZE = 12u * 1024u;
constexpr uint32_t REFLEX_RP2040_UPDATE_MAX_IMAGE_SIZE = 640u * 1024u;
constexpr uint32_t REFLEX_RP2040_UPDATE_FILESYSTEM_SIZE = 672u * 1024u;
// Signed updater packages use Arduino-Pico's native trailer:
// RSA-2048 PKCS#1 v1.5/SHA-256 signature followed by its LE uint32 length.
constexpr uint32_t REFLEX_RP2040_UPDATE_SIGNATURE_SIZE = 256u;
constexpr uint32_t REFLEX_RP2040_UPDATE_SIGNATURE_LENGTH_SIZE =
    sizeof(uint32_t);
constexpr uint32_t REFLEX_RP2040_UPDATE_SIGNATURE_TRAILER_SIZE =
    REFLEX_RP2040_UPDATE_SIGNATURE_SIZE +
    REFLEX_RP2040_UPDATE_SIGNATURE_LENGTH_SIZE;
constexpr uint32_t REFLEX_RP2040_UPDATE_MAX_PACKAGE_SIZE =
    REFLEX_RP2040_UPDATE_MAX_IMAGE_SIZE +
    REFLEX_RP2040_UPDATE_SIGNATURE_TRAILER_SIZE;

constexpr uint8_t ESP32_SPI_BRIDGE_FLAG_PAIRING_ENABLED = 1u << 0;
constexpr uint8_t ESP32_SPI_BRIDGE_FLAG_ANY_CONTROLLER = 1u << 1;
constexpr uint8_t ESP32_SPI_BRIDGE_FLAG_BT_HOST_READY = 1u << 2;
constexpr uint8_t ESP32_SPI_BRIDGE_FLAG_SCANNING = 1u << 3;
constexpr uint8_t ESP32_SPI_BRIDGE_FLAG_PORTAL_REQUEST = 1u << 4;
constexpr uint8_t ESP32_SPI_BRIDGE_FLAG_DS3_PAIRING_ARMED = 1u << 5;
// Portal maintenance and DS3 pairing are mutually exclusive. While the
// portal-active selected-bond bit is set, bit 5 instead reports an active
// firmware upload/verify/apply operation. This preserves the fixed v4 packet
// shape and remains harmless to older RP2040 firmware.
constexpr uint8_t ESP32_SPI_BRIDGE_FLAG_PORTAL_UPDATE_ACTIVE = 1u << 5;
constexpr uint8_t ESP32_SPI_BRIDGE_FLAG_BT_OUTPUT_ACTIVE = 1u << 6;
constexpr uint8_t ESP32_SPI_BRIDGE_FLAG_BT_OUTPUT_CONNECTED = 1u << 7;
// selected_bond_index only needs three bits for the eight-entry UI. New
// RP2040/ESP32 pairs use the high bits for portal acknowledgement without
// changing the v4 packet shape seen by earlier production firmware.
constexpr uint8_t ESP32_SPI_SELECTED_BOND_INDEX_MASK = 0x07;
// While the portal is inactive this means saved home-Wi-Fi credentials are
// present. While the portal is active it means selected_bond_addr carries a
// home-network IPv4 address in bytes 0..3 and the local Web-session access
// code in bytes 4..5 (rather than setup-AP credentials).
constexpr uint8_t ESP32_SPI_SELECTED_BOND_WIFI_CONFIGURED = 1u << 3;
constexpr uint8_t ESP32_SPI_SELECTED_BOND_RP_UPDATE_SUPPORTED = 1u << 4;
constexpr uint8_t ESP32_SPI_SELECTED_BOND_PORTAL_SUPPORTED = 1u << 5;
constexpr uint8_t ESP32_SPI_SELECTED_BOND_PORTAL_BLOCKED = 1u << 6;
constexpr uint8_t ESP32_SPI_SELECTED_BOND_PORTAL_ACTIVE = 1u << 7;

enum Esp32SpiControlCommand : uint8_t {
  ESP32_SPI_CONTROL_NONE = 0,
  ESP32_SPI_CONTROL_SET_PAIRING = 1,
  ESP32_SPI_CONTROL_FORGET_BONDS = 2,
  ESP32_SPI_CONTROL_SET_PLAYER_LIMIT = 3,
  ESP32_SPI_CONTROL_SET_FEEDBACK = 4,
  ESP32_SPI_CONTROL_SELECT_BOND = 5,
  ESP32_SPI_CONTROL_FORGET_BOND = 6,
  ESP32_SPI_CONTROL_RECONNECT_BOND = 7,
  ESP32_SPI_CONTROL_SET_PORTAL = 8,
  ESP32_SPI_CONTROL_ARM_DS3_PAIRING = 9,
  ESP32_SPI_CONTROL_SET_BT_OUTPUT = 10,
  ESP32_SPI_CONTROL_START_WIFI_SETUP = 11,
  ESP32_SPI_CONTROL_FORGET_WIFI = 12,
  ESP32_SPI_CONTROL_FORGET_BT_OUTPUT_BOND = 13,
  ESP32_SPI_CONTROL_SET_BT_OUTPUT_WINDOW = 14,
  // The adapter's user acknowledged the ESP32 firmware rollback notice that
  // the ESP32 publishes in the extended wire trailer. Earlier ESP32 images
  // ignore unknown commands.
  ESP32_SPI_CONTROL_ACK_FIRMWARE_NOTICE = 15,
};

// Link heartbeat (RP2040 -> ESP32). A CRC-valid control packet whose command
// is NONE is a no-op for every earlier ESP32 image, so a new RP2040 sends one
// in each transaction that carries no queued control, update or signer
// request. It proves round-trip link health: `value` echoes the sequence of
// the last CRC-valid ESP32 bridge packet the RP2040 accepted and `slot`
// carries the low byte of its accepted-packet counter. `reserved` holds the
// marker and `flags` the heartbeat state bits below.
constexpr uint8_t ESP32_SPI_HEARTBEAT_MARKER = 0x48;  // 'H'
constexpr uint8_t ESP32_SPI_HEARTBEAT_BRIDGE_FRESH = 1u << 0;
constexpr uint8_t ESP32_SPI_HEARTBEAT_RP2040_UPDATING = 1u << 1;
constexpr uint8_t ESP32_SPI_HEARTBEAT_RP2040_TRIAL = 1u << 2;
// A PS4 signer session continues across this packet. Sent only to an ESP32
// that advertised ESP32_SPI_PS4_SIGN_CAP_SESSION_HOLD.
constexpr uint8_t ESP32_SPI_HEARTBEAT_SIGNER_HOLD = 1u << 3;

// Control-packet value scope for FORGET_BT_OUTPUT_BOND. The packet CRC covers
// this bit, so Factory Reset's terminal acknowledgement proves the all-bonds
// request itself arrived rather than relying on an earlier best-effort burst.
constexpr uint8_t ESP32_SPI_BT_OUTPUT_FORGET_ALL_BONDS = 1u << 0;

enum Esp32SpiBtOutputPersona : uint8_t {
  ESP32_SPI_BT_OUTPUT_PERSONA_REFLEX = 0,
  ESP32_SPI_BT_OUTPUT_PERSONA_GENERIC_HID,
  ESP32_SPI_BT_OUTPUT_PERSONA_COUNT,
};

enum Esp32SpiSonyAssociationConsole : uint8_t {
  ESP32_SPI_SONY_ASSOCIATION_NONE = 0,
  ESP32_SPI_SONY_ASSOCIATION_PS3 = 1,
  ESP32_SPI_SONY_ASSOCIATION_PS4 = 2,
  ESP32_SPI_SONY_ASSOCIATION_PS5 = 3,
  ESP32_SPI_SONY_ASSOCIATION_COUNT,
};

constexpr uint8_t ESP32_SPI_SONY_ASSOCIATION_HAS_LINK_KEY = 1u << 0;

enum Esp32SpiPersistenceOperation : uint8_t {
  ESP32_SPI_PERSISTENCE_OPERATION_NONE = 0,
  ESP32_SPI_PERSISTENCE_OPERATION_SONY_ASSOCIATION = 1,
  ESP32_SPI_PERSISTENCE_OPERATION_BT_OUTPUT_FORGET = 2,
  ESP32_SPI_PERSISTENCE_OPERATION_WIFI_FORGET = 3,
};

enum Esp32SpiPersistenceStatus : uint8_t {
  ESP32_SPI_PERSISTENCE_STATUS_NONE = 0,
  ESP32_SPI_PERSISTENCE_STATUS_PENDING = 1,
  ESP32_SPI_PERSISTENCE_STATUS_SUCCESS = 2,
  ESP32_SPI_PERSISTENCE_STATUS_FAILED = 3,
};

constexpr uint8_t ESP32_SPI_PERSISTENCE_STATUS_MASK = 0x0Fu;
constexpr uint8_t ESP32_SPI_PERSISTENCE_OPERATION_SHIFT = 4;
constexpr uint8_t ESP32_SPI_PERSISTENCE_OPERATION_MASK = 0xF0u;
constexpr uint8_t ESP32_SPI_PERSISTENCE_TOKEN_MAX = 7;

inline constexpr uint8_t esp32SpiPersistenceOperationStatus(
    uint8_t operation, uint8_t status) {
  return static_cast<uint8_t>(
      ((operation << ESP32_SPI_PERSISTENCE_OPERATION_SHIFT) &
       ESP32_SPI_PERSISTENCE_OPERATION_MASK) |
      (status & ESP32_SPI_PERSISTENCE_STATUS_MASK));
}

inline constexpr uint8_t esp32SpiPersistenceOperation(uint8_t value) {
  return static_cast<uint8_t>(
      (value & ESP32_SPI_PERSISTENCE_OPERATION_MASK) >>
      ESP32_SPI_PERSISTENCE_OPERATION_SHIFT);
}

inline constexpr uint8_t esp32SpiPersistenceStatus(uint8_t value) {
  return value & ESP32_SPI_PERSISTENCE_STATUS_MASK;
}

// Sequence zero is reserved for legacy/non-acknowledged controls. When one
// processor restarts independently, the other can still be advertising its
// last terminal result. Skip that live operation/sequence pair so the first
// new request cannot consume a stale full-duplex response as its own ACK.
inline constexpr uint8_t esp32SpiNextPersistenceSequence(
    uint8_t current, uint8_t maximum, uint8_t operation,
    uint8_t remoteOperation, uint8_t remoteSequence) {
  if (maximum == 0) return 0;
  uint8_t candidate = current;
  for (uint16_t attempt = 0; attempt < maximum; ++attempt) {
    candidate = candidate >= maximum
        ? 1 : static_cast<uint8_t>(candidate + 1u);
    if (operation != remoteOperation || candidate != remoteSequence) {
      return candidate;
    }
  }
  return 0;
}

static_assert(
    esp32SpiNextPersistenceSequence(
        0, ESP32_SPI_PERSISTENCE_TOKEN_MAX,
        ESP32_SPI_PERSISTENCE_OPERATION_BT_OUTPUT_FORGET,
        ESP32_SPI_PERSISTENCE_OPERATION_BT_OUTPUT_FORGET, 1) == 2,
    "Persistence sequence must skip a retained terminal ACK after reboot");
static_assert(
    esp32SpiNextPersistenceSequence(
        7, ESP32_SPI_PERSISTENCE_TOKEN_MAX,
        ESP32_SPI_PERSISTENCE_OPERATION_WIFI_FORGET,
        ESP32_SPI_PERSISTENCE_OPERATION_BT_OUTPUT_FORGET, 1) == 1,
    "Independent persistence operations may reuse a compact wire token");

constexpr uint8_t ESP32_SPI_BT_OUTPUT_CONTROL_ENABLED = 1u << 0;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_CONTROL_PERSONA_SHIFT = 1;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_CONTROL_PERSONA_MASK = 0x0Eu;

inline constexpr uint8_t esp32SpiBtOutputControlValue(
    bool enabled, uint8_t persona) {
  if (!enabled) {
    // SPI v4 ESP32 images interpret any nonzero value as enabled, so persona
    // bits must never be present in a disable command.
    return 0;
  }
  const uint8_t sanitizedPersona =
      persona < ESP32_SPI_BT_OUTPUT_PERSONA_COUNT
          ? persona
          : static_cast<uint8_t>(ESP32_SPI_BT_OUTPUT_PERSONA_REFLEX);
  return static_cast<uint8_t>(
      ESP32_SPI_BT_OUTPUT_CONTROL_ENABLED |
      ((sanitizedPersona << ESP32_SPI_BT_OUTPUT_CONTROL_PERSONA_SHIFT) &
       ESP32_SPI_BT_OUTPUT_CONTROL_PERSONA_MASK));
}
static_assert(
    esp32SpiBtOutputControlValue(
        false, ESP32_SPI_BT_OUTPUT_PERSONA_GENERIC_HID) == 0,
    "Bluetooth output disable must remain compatible with older SPI v4 ESP32");
static_assert(
    esp32SpiBtOutputControlValue(
        true, ESP32_SPI_BT_OUTPUT_PERSONA_GENERIC_HID) == 3,
    "Bluetooth output persona encoding changed");

enum Esp32SpiPortalOperation : uint8_t {
  ESP32_SPI_PORTAL_NONE = 0,
  ESP32_SPI_PORTAL_SET_REPORT = 1,
  ESP32_SPI_PORTAL_GET_REPORT = 2,
};

enum Esp32SpiPortalStatus : uint8_t {
  ESP32_SPI_PORTAL_STATUS_OK = 0,
  ESP32_SPI_PORTAL_STATUS_BAD_REQUEST = 1,
  ESP32_SPI_PORTAL_STATUS_UNSUPPORTED = 2,
  ESP32_SPI_PORTAL_STATUS_INTERNAL_ERROR = 3,
};

enum Esp32SpiRpUpdateOperation : uint8_t {
  ESP32_SPI_RP_UPDATE_OP_NONE = 0,
  ESP32_SPI_RP_UPDATE_OP_READ = 1,
  ESP32_SPI_RP_UPDATE_OP_COMPLETE = 2,
  ESP32_SPI_RP_UPDATE_OP_FAILED = 3,
  ESP32_SPI_RP_UPDATE_OP_CANCEL = 4,
};

enum Esp32SpiRpUpdateStatus : uint8_t {
  ESP32_SPI_RP_UPDATE_STATUS_EMPTY = 0,
  ESP32_SPI_RP_UPDATE_STATUS_READY = 1,
  ESP32_SPI_RP_UPDATE_STATUS_DATA = 2,
  ESP32_SPI_RP_UPDATE_STATUS_COMPLETE = 3,
  ESP32_SPI_RP_UPDATE_STATUS_FAILED = 4,
  ESP32_SPI_RP_UPDATE_STATUS_BUSY = 5,
};

enum Esp32SpiRpUpdateError : uint8_t {
  ESP32_SPI_RP_UPDATE_ERROR_NONE = 0,
  ESP32_SPI_RP_UPDATE_ERROR_BAD_REQUEST = 1,
  ESP32_SPI_RP_UPDATE_ERROR_NO_IMAGE = 2,
  ESP32_SPI_RP_UPDATE_ERROR_READ = 3,
  ESP32_SPI_RP_UPDATE_ERROR_SIZE = 4,
  ESP32_SPI_RP_UPDATE_ERROR_PRODUCT = 5,
  ESP32_SPI_RP_UPDATE_ERROR_CHECKSUM = 6,
  ESP32_SPI_RP_UPDATE_ERROR_STAGE3_MISMATCH = 7,
  ESP32_SPI_RP_UPDATE_ERROR_STORAGE = 8,
  ESP32_SPI_RP_UPDATE_ERROR_TIMEOUT = 9,
  ESP32_SPI_RP_UPDATE_ERROR_SIGNATURE = 10,
  // RP2040-local refusals before anything is staged. The candidate's health
  // trial needs a working ESP32 bridge, so an install while the bridge is down
  // would always roll back; one during an ESP32 health trial could make the
  // ESP32 roll back a good image while the RP2040 restarts.
  ESP32_SPI_RP_UPDATE_ERROR_BRIDGE_DOWN = 11,
  ESP32_SPI_RP_UPDATE_ERROR_PEER_TRIAL = 12,
  // The candidate is older than the update floor (core/
  // modern2usb_update_policy.h). An ESP32 without this code reports a generic
  // refusal.
  ESP32_SPI_RP_UPDATE_ERROR_VERSION_FLOOR = 13,
};
// MODERN2USB keeps TinyUSB device output on RP2040 core0 and PIO-USB host input
// on core1. A local RSA-PSS operation blocks core0 for roughly 1.5 seconds,
// starving the PS4 interrupt endpoint. These packets offload only the private
// nonce signature to the onboard ESP32; key material is held in RAM and is
// retransmitted after either processor restarts.
enum Esp32SpiPs4SignOperation : uint8_t {
  ESP32_SPI_PS4_SIGN_OP_NONE = 0,
  ESP32_SPI_PS4_SIGN_OP_KEY = 1,
  ESP32_SPI_PS4_SIGN_OP_NONCE = 2,
  ESP32_SPI_PS4_SIGN_OP_POLL = 3,
  ESP32_SPI_PS4_SIGN_OP_CANCEL = 4,
};

enum Esp32SpiPs4SignStatus : uint8_t {
  ESP32_SPI_PS4_SIGN_STATUS_NONE = 0,
  ESP32_SPI_PS4_SIGN_STATUS_ACK = 1,
  ESP32_SPI_PS4_SIGN_STATUS_BUSY = 2,
  ESP32_SPI_PS4_SIGN_STATUS_READY = 3,
  ESP32_SPI_PS4_SIGN_STATUS_ERROR = 4,
};

constexpr size_t ESP32_SPI_PS4_SIGN_CHUNK_SIZE = 128;
constexpr uint8_t ESP32_SPI_PS4_SIGN_KEY_CHUNKS = 6;
constexpr uint8_t ESP32_SPI_PS4_SIGN_NONCE_CHUNKS = 2;
constexpr uint8_t ESP32_SPI_PS4_SIGN_SIGNATURE_CHUNKS = 2;
// Capability bits in data[0] of a zero-length signer response (CRC-covered;
// earlier RP2040 images ignore data when length is zero). SESSION_HOLD: the
// ESP32 keeps the session across heartbeats flagged SIGNER_HOLD, so the
// RP2040 may interleave normal bridge transactions while the RSA operation
// runs instead of starving Bluetooth input.
constexpr uint8_t ESP32_SPI_PS4_SIGN_CAP_SESSION_HOLD = 1u << 0;

struct __attribute__((packed, aligned(1))) Esp32SpiPs4SignRequest {
  uint32_t magic;
  uint8_t version;
  uint8_t sequence;
  uint8_t operation;
  uint8_t chunk;
  uint8_t length;
  uint8_t flags;
  uint8_t data[ESP32_SPI_PS4_SIGN_CHUNK_SIZE];
  uint16_t crc16;
};

struct __attribute__((packed, aligned(1))) Esp32SpiPs4SignResponse {
  uint32_t magic;
  uint8_t version;
  uint8_t sequence;
  uint8_t operation;
  uint8_t chunk;
  uint8_t status;
  uint8_t length;
  uint32_t duration_us;
  uint8_t data[ESP32_SPI_PS4_SIGN_CHUNK_SIZE];
  uint16_t crc16;
};

constexpr uint8_t ESP32_SPI_RP_UPDATE_FLAG_MANIFEST_VALID = 1u << 0;
constexpr uint8_t ESP32_SPI_RP_UPDATE_FLAG_CHECKSUM_READY = 1u << 1;
constexpr uint8_t ESP32_SPI_RP_UPDATE_FLAG_STAGE3_MATCH_REQUIRED = 1u << 2;
constexpr uint8_t ESP32_SPI_RP_UPDATE_FLAG_SIGNATURE_PRESENT = 1u << 3;

enum Esp32SpiControllerType : uint8_t {
  ESP32_SPI_CONTROLLER_UNKNOWN = 0,
  ESP32_SPI_CONTROLLER_DUALSHOCK3,
  ESP32_SPI_CONTROLLER_DUALSHOCK4,
  ESP32_SPI_CONTROLLER_DUALSENSE,
  ESP32_SPI_CONTROLLER_SWITCH,
  ESP32_SPI_CONTROLLER_XBOX,
  ESP32_SPI_CONTROLLER_GENERIC,
  ESP32_SPI_CONTROLLER_SWITCH2,
  ESP32_SPI_CONTROLLER_WII,
  ESP32_SPI_CONTROLLER_WII_U_PRO,
  ESP32_SPI_CONTROLLER_SWITCH_N64,
  ESP32_SPI_CONTROLLER_SWITCH_JOYCON,
  ESP32_SPI_CONTROLLER_SWITCH_NES,
  ESP32_SPI_CONTROLLER_SWITCH_SNES,
  ESP32_SPI_CONTROLLER_SWITCH_GENESIS,
};

constexpr uint8_t ESP32_SPI_FEEDBACK_RUMBLE_VALID = 1u << 0;
constexpr uint8_t ESP32_SPI_FEEDBACK_LED_VALID = 1u << 1;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_STATUS_ADVERTISING = 1u << 0;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_STATUS_ENCRYPTED = 1u << 1;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_STATUS_BONDED = 1u << 2;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_STATUS_NOTIFY_ENABLED = 1u << 3;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_STATUS_READY = 1u << 4;

struct __attribute__((packed, aligned(1))) Esp32SpiBridgeTouchPoint {
  // Sony-compatible contact packing: inactive/id, X low, X high/Y low, Y high.
  // Coordinates are normalized to a 0..4095 transport range.
  uint8_t data[4];
};

struct __attribute__((packed, aligned(1))) Esp32SpiBridgePadPacket {
  uint8_t connected;
  uint8_t config;
  uint8_t controller_type;
  uint8_t battery_percent;
  uint32_t digital_buttons;
  int16_t lx;
  int16_t ly;
  int16_t rx;
  int16_t ry;
  uint8_t analog_l2;
  uint8_t analog_r2;
  // Bluetooth device address for transport identity. MODERN2USB uses this to
  // collapse a Sony controller that is simultaneously connected over USB and
  // Bluetooth into one logical player.
  uint8_t identity_addr[6];
  uint8_t reserved2[2];
  Esp32SpiBridgeTouchPoint touch[2];
};

struct __attribute__((packed, aligned(1))) Esp32SpiBridgePacket {
  uint32_t magic;
  uint8_t version;
  uint8_t sequence;
  uint8_t player_count;
  uint8_t flags;
  uint8_t bond_count;
  uint8_t selected_bond_index;
  uint8_t selected_bond_addr[6];
  Esp32SpiBridgePadPacket pads[ESP32_SPI_BRIDGE_MAX_PLAYERS];
  uint16_t crc16;
};

struct __attribute__((packed, aligned(1))) Esp32SpiControlPacket {
  uint32_t magic;
  uint8_t version;
  uint8_t command;
  uint8_t value;
  uint8_t slot;
  uint8_t rumble_left;
  uint8_t rumble_right;
  uint8_t led_r;
  uint8_t led_g;
  uint8_t led_b;
  uint8_t player_leds;
  uint8_t flags;
  uint8_t reserved;
  uint16_t crc16;
};

// Player-1 neutral frame sent from RP2040 to the ESP32 while Bluetooth output
// owns the radio. It lives in the unused tail of every full-duplex transfer.
struct __attribute__((packed, aligned(1))) Esp32SpiBtOutputFrame {
  uint32_t magic;
  uint8_t version;
  uint8_t sequence;
  uint8_t connected;
  uint8_t reserved;
  uint32_t digital_buttons;
  int16_t lx;
  int16_t ly;
  int16_t rx;
  int16_t ry;
  uint8_t analog_l2;
  uint8_t analog_r2;
  uint16_t crc16;
};

// A console-side Sony SET_FEATURE association captured while Adapt is wired
// to a PlayStation host. This optional overlay is staging only: it records the
// host address/key needed by future native PS3/PS4/PS5 Bluetooth personas but
// does not advertise or claim one. Repeated packets are idempotent on ESP32.
struct __attribute__((packed, aligned(1))) Esp32SpiSonyAssociationPacket {
  uint32_t magic;
  uint8_t version;
  uint8_t sequence;
  uint8_t console;
  uint8_t flags;
  uint8_t host_address[6];
  uint8_t link_key[16];
  uint16_t crc16;
};

// CRC coverage comes from the enclosing bridge packet. While Bluetooth output
// is inactive this occupies pad[3].reserved2; RBOS carries the output-Forget
// result while the peripheral role owns the pad payload.
struct __attribute__((packed, aligned(1))) Esp32SpiPersistenceResult {
  uint8_t sequence;
  uint8_t operation_status;
};

// BLE HID peripheral state returned in the bridge pad payload while output is
// active. Bluetooth input packets are suppressed in that mutually-exclusive
// mode, so the overlay cannot consume a controller slot.
struct __attribute__((packed, aligned(1))) Esp32SpiBtOutputStatus {
  uint32_t magic;
  uint8_t version;
  uint8_t sequence;
  uint8_t flags;
  uint8_t rumble_left;
  uint8_t rumble_right;
  uint8_t peer_addr[6];
  uint8_t reserved;
  uint16_t crc16;
};
constexpr uint8_t ESP32_SPI_BT_OUTPUT_STATUS_CAP_RADIO_V1 = 1u << 0;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_STATUS_CAP_PERSISTENCE_RESULT =
    1u << 1;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_TOKEN_SHIFT = 2;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_TOKEN_MASK = 0x1Cu;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_STATUS_SHIFT = 5;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_STATUS_MASK = 0x60u;
// Output mode has only one remaining RBOS bit. Sony association cannot run in
// that role, so bit 7 distinguishes the two operations that can: output-bond
// Forget and Wi-Fi Forget during Factory Reset.
constexpr uint8_t ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_WIFI = 1u << 7;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_FORGET_TOKEN_MAX =
    ESP32_SPI_PERSISTENCE_TOKEN_MAX;

inline constexpr uint8_t esp32SpiBtOutputPersistenceReserved(
    uint8_t base, uint8_t operation, uint8_t token, uint8_t status) {
  return static_cast<uint8_t>(
      (base & ~(ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_TOKEN_MASK |
                ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_STATUS_MASK |
                ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_WIFI)) |
      ESP32_SPI_BT_OUTPUT_STATUS_CAP_PERSISTENCE_RESULT |
      (operation == ESP32_SPI_PERSISTENCE_OPERATION_WIFI_FORGET
           ? ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_WIFI : 0) |
      ((token << ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_TOKEN_SHIFT) &
       ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_TOKEN_MASK) |
      ((status << ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_STATUS_SHIFT) &
       ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_STATUS_MASK));
}

inline constexpr uint8_t esp32SpiBtOutputPersistenceToken(uint8_t value) {
  return static_cast<uint8_t>(
      (value & ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_TOKEN_MASK) >>
      ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_TOKEN_SHIFT);
}

inline constexpr uint8_t esp32SpiBtOutputPersistenceStatus(uint8_t value) {
  return static_cast<uint8_t>(
      (value & ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_STATUS_MASK) >>
      ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_STATUS_SHIFT);
}

inline constexpr uint8_t esp32SpiBtOutputPersistenceOperation(uint8_t value) {
  return (value & ESP32_SPI_BT_OUTPUT_STATUS_PERSISTENCE_WIFI) != 0
      ? ESP32_SPI_PERSISTENCE_OPERATION_WIFI_FORGET
      : ESP32_SPI_PERSISTENCE_OPERATION_BT_OUTPUT_FORGET;
}

// Optional Bluetooth-output link diagnostics. This extension follows the
// unchanged 18-byte RBOS record inside the otherwise-unused output-mode pad
// payload. Old firmware ignores it; new firmware validates its own
// magic/version/CRC before using it.
struct __attribute__((packed, aligned(1))) Esp32SpiBtOutputDiagnostics {
  uint32_t magic;
  uint8_t version;
  uint8_t sequence;
  uint8_t flags;
  uint8_t last_disconnect_status;
  uint8_t last_disconnect_reason;
  uint8_t last_frame_sequence;
  uint8_t last_queued_sequence;
  uint8_t last_sent_sequence;
  uint16_t initial_interval;
  uint16_t initial_latency;
  uint16_t initial_supervision_timeout;
  uint16_t current_interval;
  uint16_t current_latency;
  uint16_t current_supervision_timeout;
  uint32_t connection_count;
  uint32_t disconnect_count;
  uint32_t active_connection_ms;
  uint32_t last_connection_ms;
  uint32_t frame_updates;
  uint32_t notify_attempts;
  uint32_t notify_enqueued;
  uint32_t notify_sent;
  uint32_t notify_backpressure;
  uint32_t notify_coalesced;
  uint32_t last_frame_age_ms;
  uint32_t last_sent_age_ms;
  uint32_t pending_age_ms;
  uint32_t hci_acl_submitted;
  uint32_t hci_acl_completed;
  uint32_t hci_completion_events;
  uint32_t hci_acl_outstanding;
  uint32_t hci_acl_max_outstanding;
  uint32_t last_hci_completion_age_ms;
  uint8_t hci_acl_capacity;
  uint8_t notification_window_requested;
  uint8_t notification_window_effective;
  uint8_t notification_in_flight;
  uint16_t crc16;
};

constexpr uint8_t ESP32_SPI_BT_OUTPUT_DIAG_PARAMS_VALID = 1u << 0;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_DIAG_NOTIFY_PENDING = 1u << 1;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_DIAG_BACKPRESSURE = 1u << 2;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_DIAG_PARAM_REQUEST_SENT = 1u << 3;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_DIAG_PARAM_RESPONSE_RECEIVED = 1u << 4;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_DIAG_PARAM_RESPONSE_ACCEPTED = 1u << 5;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_DIAG_NOTIFY_SUBMITTING = 1u << 6;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_DIAG_NOTIFY_IN_FLIGHT = 1u << 7;
constexpr size_t ESP32_SPI_BT_OUTPUT_DIAGNOSTICS_OFFSET =
    sizeof(Esp32SpiBtOutputStatus);

// Optional radio-quality tail. RBOS advertises this exact V1 layout through
// its reserved capability byte. It occupies the final twelve otherwise-unused
// bytes of the output-mode pad payload, so SPI v4 and RBOD v3 stay compatible
// across one-sided RP2040/ESP32 updates. The enclosing bridge CRC covers it.
struct __attribute__((packed, aligned(1))) Esp32SpiBtOutputRadioDiagnostics {
  uint8_t sample_sequence;
  uint8_t flags;
  uint8_t rssi_status;
  uint8_t channel_map_status;
  int8_t rssi_dbm;
  uint8_t channel_map[5];
  uint16_t query_duration_ms;
};

constexpr uint8_t ESP32_SPI_BT_OUTPUT_RADIO_RSSI_VALID = 1u << 0;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_RADIO_CHANNEL_MAP_VALID = 1u << 1;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_RADIO_RSSI_PENDING = 1u << 2;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_RADIO_CHANNEL_MAP_PENDING = 1u << 3;
constexpr uint8_t ESP32_SPI_BT_OUTPUT_RADIO_SAMPLE_VALID = 1u << 4;
constexpr size_t ESP32_SPI_BT_OUTPUT_RADIO_OFFSET =
    ESP32_SPI_BT_OUTPUT_DIAGNOSTICS_OFFSET +
    sizeof(Esp32SpiBtOutputDiagnostics);

constexpr size_t ESP32_SPI_PORTAL_REPORT_SIZE = 63;

struct __attribute__((packed, aligned(1))) Esp32SpiPortalRequest {
  uint32_t magic;
  uint8_t version;
  uint8_t sequence;
  uint8_t operation;
  uint8_t report_id;
  uint8_t length;
  uint8_t reserved;
  uint8_t data[ESP32_SPI_PORTAL_REPORT_SIZE];
  uint16_t crc16;
};

struct __attribute__((packed, aligned(1))) Esp32SpiPortalResponse {
  uint32_t magic;
  uint8_t version;
  uint8_t sequence;
  uint8_t status;
  uint8_t report_id;
  uint8_t length;
  uint8_t reserved;
  uint8_t data[ESP32_SPI_PORTAL_REPORT_SIZE];
  uint16_t crc16;
};

struct __attribute__((packed, aligned(1))) ReflexRp2040FirmwareManifest {
  uint32_t magic;
  uint32_t product_magic;
  uint8_t schema;
  uint8_t product;
  uint8_t mcu;
  uint8_t boot_layout;
  uint8_t version_major;
  uint8_t version_minor;
  uint8_t version_patch;
  uint8_t reserved;
  uint32_t filesystem_size;
  uint32_t maximum_image_size;
};

// RP2040 -> ESP32 request. During an update this replaces the normal control
// packet at byte zero; update and controller/portal traffic are exclusive.
struct __attribute__((packed, aligned(1))) Esp32SpiRpUpdateRequest {
  uint32_t magic;
  uint8_t version;
  uint8_t sequence;
  uint8_t operation;
  uint8_t value;
  uint16_t session_id;
  uint32_t offset;
  uint16_t length;
  uint16_t crc16;
};

constexpr size_t ESP32_SPI_RP_UPDATE_DATA_SIZE = 91;

static_assert(sizeof(Esp32SpiPs4SignRequest) == 140,
              "Unexpected PS4 signer request size");
static_assert(sizeof(Esp32SpiPs4SignResponse) == 144,
              "Unexpected PS4 signer response size");
// ESP32 -> RP2040 response. It overlays the four 34-byte pad records while
// player_count is zero, preserving the fixed v4 156-byte full-duplex wire.
struct __attribute__((packed, aligned(1))) Esp32SpiRpUpdateResponse {
  uint32_t magic;
  uint8_t version;
  uint8_t sequence;
  uint8_t status;
  uint8_t flags;
  uint16_t session_id;
  uint32_t total_size;
  uint32_t offset;
  uint16_t data_length;
  uint8_t md5[16];
  uint8_t product;
  uint8_t mcu;
  uint8_t boot_layout;
  uint8_t version_major;
  uint8_t version_minor;
  uint8_t version_patch;
  uint8_t error;
  uint8_t data[ESP32_SPI_RP_UPDATE_DATA_SIZE];
  uint16_t crc16;
};

static_assert(sizeof(Esp32SpiBridgeTouchPoint) == 4,
              "Unexpected ESP32 SPI touch point size");
static_assert(sizeof(Esp32SpiBridgePadPacket) == 34,
              "Unexpected ESP32 SPI pad packet size");
static_assert(sizeof(Esp32SpiBridgePacket) == 154,
              "Unexpected ESP32 SPI packet size");
static_assert(sizeof(Esp32SpiControlPacket) == 18,
              "Unexpected ESP32 SPI control packet size");
static_assert(sizeof(Esp32SpiBtOutputFrame) == 24,
              "Unexpected ESP32 Bluetooth output frame size");
static_assert(sizeof(Esp32SpiSonyAssociationPacket) == 32,
              "Unexpected Sony association packet size");
static_assert(sizeof(Esp32SpiPersistenceResult) == 2,
              "Unexpected persistence result size");
static_assert(sizeof(Esp32SpiBtOutputStatus) == 18,
              "Unexpected ESP32 Bluetooth output status size");
static_assert(sizeof(Esp32SpiBtOutputDiagnostics) == 106,
              "Unexpected ESP32 Bluetooth output diagnostics size");
static_assert(sizeof(Esp32SpiBtOutputRadioDiagnostics) == 12,
              "Unexpected ESP32 Bluetooth radio diagnostics size");
static_assert(sizeof(Esp32SpiPortalRequest) == 75,
              "Unexpected ESP32 portal request size");
static_assert(sizeof(Esp32SpiPortalResponse) == 75,
              "Unexpected ESP32 portal response size");
static_assert(sizeof(ReflexRp2040FirmwareManifest) == 24,
              "Unexpected RP2040 firmware manifest size");
static_assert(sizeof(Esp32SpiRpUpdateRequest) == 18,
              "Unexpected RP2040 update request size");
static_assert(sizeof(Esp32SpiRpUpdateResponse) == 136,
              "Unexpected RP2040 update response size");
static_assert(sizeof(Esp32SpiPortalRequest) <=
                  sizeof(((Esp32SpiBridgePacket*)nullptr)->pads),
              "ESP32 portal request must fit in the bridge pad payload");
static_assert(sizeof(Esp32SpiRpUpdateResponse) ==
                  sizeof(((Esp32SpiBridgePacket*)nullptr)->pads),
              "RP2040 update response must exactly fill the pad payload");
// ESP32 SPI-slave DMA requires the declared receive length to be a multiple
// of four bytes. Pad only the wire transaction; packet offsets and CRC cover
// the packed protocol payload.
constexpr size_t ESP32_SPI_BRIDGE_WIRE_SIZE =
    (sizeof(Esp32SpiBridgePacket) + 3u) & ~size_t(3u);
static_assert(ESP32_SPI_BRIDGE_WIRE_SIZE == 156,
              "Unexpected ESP32 SPI DMA wire size");
static_assert(sizeof(Esp32SpiPs4SignRequest) <=
                  ESP32_SPI_BRIDGE_WIRE_SIZE,
              "PS4 signer request must fit in the SPI control direction");
static_assert(sizeof(Esp32SpiPs4SignResponse) <=
                  ESP32_SPI_BRIDGE_WIRE_SIZE,
              "PS4 signer response must fit in the SPI bridge direction");
// The two DMA-alignment bytes are outside the CRC-covered v4 packet. They
// carry a backward-compatible parser capability plus live Sony-touch health;
// old ESP32 images leave both bytes at zero and therefore fail closed.
constexpr size_t ESP32_SPI_WIRE_TRAILER_OFFSET =
    sizeof(Esp32SpiBridgePacket);
constexpr uint8_t ESP32_SPI_WIRE_TRAILER_MAGIC = 0xA5;
constexpr uint8_t ESP32_SPI_WIRE_CAP_TOUCH_SANITIZED = 1u << 0;
constexpr uint8_t ESP32_SPI_WIRE_TOUCH_LAYOUT_SEEN = 1u << 1;
constexpr uint8_t ESP32_SPI_WIRE_TOUCH_ACTIVE = 1u << 2;
constexpr uint8_t ESP32_SPI_WIRE_TOUCH_REJECTED = 1u << 3;
constexpr uint8_t ESP32_SPI_WIRE_TOUCH_LENGTH_ERROR = 1u << 4;
// Live station state is separate from the saved-Wi-Fi bit in
// selected_bond_index. Older firmware ignores this DMA-trailer bit.
constexpr uint8_t ESP32_SPI_WIRE_WIFI_CONNECTED = 1u << 5;
// Confirmed HCI input suspension: background BR/EDR page and passive LE scans
// are stopped for provisioning or while an idle LAN portal owns the radio.
constexpr uint8_t ESP32_SPI_WIRE_BT_INPUT_SUSPENDED = 1u << 6;
// New BlueRetro owner publishes tokenized durable persistence results. Legacy
// portal bridges leave this bit clear and retain best-effort erase controls.
constexpr uint8_t ESP32_SPI_WIRE_CAP_DURABLE_PERSISTENCE = 1u << 7;
static_assert(ESP32_SPI_WIRE_TRAILER_OFFSET + 2u <=
                  ESP32_SPI_BRIDGE_WIRE_SIZE,
              "ESP32 SPI capability trailer must fit DMA padding");

// Extended trailer magic. An ESP32 sends 0xB0 | status in trailer byte 0
// only in the transaction right after it received a valid heartbeat, which
// only an RP2040 that decodes this form sends; every other packet keeps the
// 0xA5 magic. Byte 1 keeps its meaning in both forms.
constexpr uint8_t ESP32_SPI_WIRE_TRAILER_EXT_BASE = 0xB0;
constexpr uint8_t ESP32_SPI_WIRE_TRAILER_EXT_MASK = 0xF0;
// An automatic ESP32 rollback is recorded and not yet acknowledged.
constexpr uint8_t ESP32_SPI_WIRE_EXT_ROLLBACK_NOTICE = 1u << 0;
// The running ESP32 image is a candidate awaiting its health decision.
constexpr uint8_t ESP32_SPI_WIRE_EXT_UPDATE_TRIAL = 1u << 1;
// Recent heartbeats proved that the RP2040 accepts this ESP32's packets.
constexpr uint8_t ESP32_SPI_WIRE_EXT_LINK_PROVEN = 1u << 2;

inline constexpr bool esp32SpiWireTrailerMagicValid(uint8_t magic) {
  return magic == ESP32_SPI_WIRE_TRAILER_MAGIC ||
      (magic & ESP32_SPI_WIRE_TRAILER_EXT_MASK) ==
          ESP32_SPI_WIRE_TRAILER_EXT_BASE;
}

inline constexpr bool esp32SpiWireTrailerExtended(uint8_t magic) {
  return (magic & ESP32_SPI_WIRE_TRAILER_EXT_MASK) ==
      ESP32_SPI_WIRE_TRAILER_EXT_BASE;
}

inline constexpr uint8_t esp32SpiWireTrailerExtMagic(uint8_t status) {
  return static_cast<uint8_t>(
      ESP32_SPI_WIRE_TRAILER_EXT_BASE |
      (status & static_cast<uint8_t>(~ESP32_SPI_WIRE_TRAILER_EXT_MASK)));
}
static_assert(!esp32SpiWireTrailerExtended(ESP32_SPI_WIRE_TRAILER_MAGIC),
              "Legacy trailer magic must not decode as extended");
static_assert(esp32SpiWireTrailerMagicValid(esp32SpiWireTrailerExtMagic(0x0F)),
              "Extended trailer magic must stay valid");

// Fill the heartbeat fields of a control packet. The caller sets magic,
// version and CRC through its normal control finalizer.
inline void esp32SpiFillHeartbeat(
    Esp32SpiControlPacket& packet, uint8_t echoedSequence,
    uint8_t acceptedCount, uint8_t flags) {
  packet = {};
  packet.command = ESP32_SPI_CONTROL_NONE;
  packet.value = echoedSequence;
  packet.slot = acceptedCount;
  packet.flags = flags;
  packet.reserved = ESP32_SPI_HEARTBEAT_MARKER;
}

// True for an already CRC-validated control packet that is a heartbeat.
inline bool esp32SpiControlIsHeartbeat(const Esp32SpiControlPacket& packet) {
  return packet.command == ESP32_SPI_CONTROL_NONE &&
      packet.reserved == ESP32_SPI_HEARTBEAT_MARKER;
}
static_assert(sizeof(Esp32SpiRpUpdateRequest) <=
                  ESP32_SPI_BRIDGE_WIRE_SIZE,
              "RP2040 update request must fit the SPI control direction");

// The normal control packet remains at byte zero. Portal responses use the
// otherwise-unused tail of the same full-duplex SPI transaction.
constexpr size_t ESP32_SPI_PORTAL_RESPONSE_OFFSET =
    (sizeof(Esp32SpiControlPacket) + 3u) & ~size_t(3u);
static_assert(ESP32_SPI_PORTAL_RESPONSE_OFFSET +
                  sizeof(Esp32SpiPortalResponse) <=
              ESP32_SPI_BRIDGE_WIRE_SIZE,
              "ESP32 portal response must fit in the SPI control direction");

constexpr size_t ESP32_SPI_BT_OUTPUT_FRAME_OFFSET = 96;
static_assert(ESP32_SPI_PORTAL_RESPONSE_OFFSET +
                  sizeof(Esp32SpiPortalResponse) <=
              ESP32_SPI_BT_OUTPUT_FRAME_OFFSET,
              "Portal response must not overlap Bluetooth output frame");
static_assert(ESP32_SPI_BT_OUTPUT_FRAME_OFFSET +
                  sizeof(Esp32SpiBtOutputFrame) <=
              ESP32_SPI_BRIDGE_WIRE_SIZE,
              "ESP32 Bluetooth output frame must fit in SPI control direction");
constexpr size_t ESP32_SPI_SONY_ASSOCIATION_OFFSET =
    ESP32_SPI_BT_OUTPUT_FRAME_OFFSET + sizeof(Esp32SpiBtOutputFrame);
static_assert(ESP32_SPI_SONY_ASSOCIATION_OFFSET +
                  sizeof(Esp32SpiSonyAssociationPacket) <=
              ESP32_SPI_BRIDGE_WIRE_SIZE,
              "Sony association packet must fit in SPI control direction");
static_assert(sizeof(Esp32SpiBtOutputStatus) <=
                  sizeof(((Esp32SpiBridgePacket*)nullptr)->pads),
              "ESP32 Bluetooth output status must fit in bridge pad payload");
static_assert(ESP32_SPI_BT_OUTPUT_DIAGNOSTICS_OFFSET +
                  sizeof(Esp32SpiBtOutputDiagnostics) <=
              sizeof(((Esp32SpiBridgePacket*)nullptr)->pads),
              "ESP32 Bluetooth diagnostics must fit in bridge pad payload");
static_assert(ESP32_SPI_BT_OUTPUT_RADIO_OFFSET +
                  sizeof(Esp32SpiBtOutputRadioDiagnostics) ==
              sizeof(((Esp32SpiBridgePacket*)nullptr)->pads),
              "ESP32 Bluetooth radio diagnostics must fill the pad tail");
