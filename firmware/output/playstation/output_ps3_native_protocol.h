#pragma once

#include <stdint.h>
#include <string.h>
#include "output_descriptors_ps3_runtime.h"

// Wired diagnostic protocol. This identity is not a Bluetooth radio address,
// and host association is volatile: no settings or pairing storage is written.
struct Ps3NativeUsbState {
  uint8_t local_address[6] = {};
  uint8_t host_address[6] = {};
  uint8_t leds = 0;
  uint8_t sensor_selector = 0;

  void reset(const uint8_t board_id[8]) {
    memcpy(local_address, board_id + 2, sizeof(local_address));
    local_address[0] = (local_address[0] | 0x02u) & 0xfeu;
    clearSession();
  }

  void clearSession() {
    memset(host_address, 0, sizeof(host_address));
    leds = sensor_selector = 0;
  }

  static uint16_t copy(uint8_t* out, uint16_t capacity,
                       const void* source, uint16_t size) {
    const uint16_t length = capacity < size ? capacity : size;
    if (length != 0) memcpy(out, source, length);
    return length;
  }

  uint16_t getReport(uint8_t id, hid_report_type_t type, uint8_t* out,
                     uint16_t capacity, const usbout_ps3_report_t& input) const {
    if (type == HID_REPORT_TYPE_INPUT && id == 1) {
      return copy(out, capacity, &input, sizeof(input));
    }
    return 0;
  }

  // DS3 control features do not follow TinyUSB's automatic Report-ID prefix.
  // These are complete 64-byte control replies, including their native header.
  uint16_t getFeature(uint8_t id, uint8_t* out, uint16_t capacity) const {
    uint8_t wire[64] = {};
    const uint8_t* payload = nullptr;
    switch (id) {
      case 1: payload = report_01; break;
      case 0xef: payload = report_ef; break;
      case 0xf2:
        wire[0] = 0xf2;
        payload = report_f2;
        break;
      case 0xf5:
        wire[0] = 1;
        payload = report_f5;
        break;
      case 0xf7: wire[0] = 1; payload = report_f7; break;
      case 0xf8: payload = report_f8; break;
      default: return 0;
    }
    static_assert(sizeof(report_01) == 63 && sizeof(report_ef) == 63 &&
                  sizeof(report_f2) == 63 && sizeof(report_f5) == 63 &&
                  sizeof(report_f7) == 63 && sizeof(report_f8) == 63,
                  "DS3 feature templates exclude one native header byte");
    memcpy(wire + 1, payload, 63);
    if (id == 0xf2) memcpy(wire + 4, local_address, sizeof(local_address));
    if (id == 0xf5) memcpy(wire + 2, host_address, sizeof(host_address));
    if (id == 0xef || id == 0xf8) wire[7] = sensor_selector;
    return copy(out, capacity, wire, sizeof(wire));
  }

  bool setHostAddress(uint8_t id, hid_report_type_t type,
                       const uint8_t* data, uint16_t size) {
    if (type != HID_REPORT_TYPE_FEATURE || id != 0xf5 || size < 8 || size > 64 ||
        data[0] != 1 || data[1] != 0) return false;
    memcpy(host_address, data + 2, sizeof(host_address));
    return true;
  }

  bool setSensorSelector(const uint8_t* data, uint16_t size) {
    if (size != 48 || data[4] != 3 || data[5] != 1) return false;
    sensor_selector = data[6];
    return true;
  }

  static uint32_t rumbleDurationMs(uint8_t duration) {
    // Wired DS3 vibration captures fit about 18.7 ms/unit. Use an 18.75 ms
    // compatibility scale; this is measured behavior, not a Sony timing spec.
    // Zero already gates motor power; FF runs until the next host update.
    return duration == 0 || duration == 0xff
        ? 0 : (static_cast<uint32_t>(duration) * 75u + 3u) / 4u;
  }

  bool output(uint8_t id, const uint8_t* data, uint16_t size,
               uint8_t& large, uint8_t& small,
               uint8_t* largeDuration = nullptr, uint8_t* smallDuration = nullptr) {
    if (id == 0 && (size == 49 || size == 36) && data[0] == 1) {
      ++data;
      --size;
      id = 1;
    }
    if (id != 1) return false;
    uint8_t shift = 0;
    if (size == 47 || size == 34) {
      // TinyUSB strips the payload's leading pad byte when it equals the
      // control report ID. The remaining first bytes are the small-motor
      // duration, small-motor power and large-motor duration.
      if (data[1] > 1) return false;
      shift = 1;
    } else if (size != 48 && size != 35) {
      return false;
    }
    const uint8_t small_duration = data[1 - shift];
    const uint8_t large_duration = data[3 - shift];
    large = large_duration != 0 ? data[4 - shift] : 0;
    small = small_duration != 0 && data[2 - shift] != 0 ? 255 : 0;
    if (largeDuration) *largeDuration = large_duration;
    if (smallDuration) *smallDuration = small_duration;
    leds = data[9 - shift] & 0x1eu;
    return true;
  }
};
