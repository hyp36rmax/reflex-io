#pragma once

#include "../usb/output_usb_product_association.h"

// Internal AUTO/WebHID HID callback bridge. This header is included from
// out_usb.h after descriptors and AUTO-detect runtime helpers are available.

static bool handle_output_autodetect_hid_get_report(
  uint8_t report_id,
  hid_report_type_t report_type,
  uint8_t* buffer,
  uint16_t reqlen,
  uint16_t* response_len
) {
  const bool auto_detect_ps3_feature =
    can_run_usb_detection() &&
    report_type == HID_REPORT_TYPE_FEATURE &&
    (report_id == 0xEF || report_id == 0xF2 || report_id == 0xF5 || report_id == 0xF7 || report_id == 0xF8);

  if (get_effective_output_mode() != OUTPUT_SWITCHPRO &&
      !auto_detect_ps3_feature &&
      report_type == HID_REPORT_TYPE_FEATURE &&
      report_id >= 0xE0 &&
      report_id <= 0xEF) {
    *response_len = webhid_get_report(report_id, buffer, reqlen);
    return true;
  }

  if (can_run_usb_detection() && outputMode != OUTPUT_PS4) {
    usb_detect_handle_hid_get(report_id, report_type, buffer, reqlen);
    if (report_type == HID_REPORT_TYPE_FEATURE) {
      switch (report_id) {
        case 0x00: {
          const uint16_t len = min(reqlen, (uint16_t) sizeof(ps3_magic_bytes));
          memcpy(buffer, ps3_magic_bytes, len);
          *response_len = len;
          return true;
        }
        case 0x01: {
          const uint16_t len = min(reqlen, (uint16_t) sizeof(report_01));
          memcpy(buffer, report_01, len);
          *response_len = len;
          return true;
        }
        case 0x03: {
          const uint16_t len = min(
            reqlen, (uint16_t)sizeof(ps4_gamepad_definition_report));
          memcpy(buffer, ps4_gamepad_definition_report, len);
          *response_len = len;
          return true;
        }
        case 0xEF: {
          const uint16_t len = min(reqlen, (uint16_t) sizeof(report_ef));
          memcpy(buffer, report_ef, len);
          *response_len = len;
          return true;
        }
        case 0xF2: {
          const uint16_t len = min(reqlen, (uint16_t) sizeof(report_f2));
          const bool localAddressRequired =
              productSonyPs3LocalAddressRequired();
          uint8_t localAddress[6] = {};
          if (localAddressRequired) {
          if (len < 9) {
            // A malformed/short GET_REPORT has no room for the six-byte
            // identity. Return a bounded zero length; the -1 sentinel below
            // is valid only when TinyUSB prepended a normal nonzero report ID.
            *response_len = 0;
            return true;
          }
          if (!productCopySonyPs3LocalAddress(localAddress)) {
            // The address returned by F2 is the identity a PS3 records in F5.
            // Explicitly stall until the ESP32 publishes its real Classic
            // address; never expose report_f2's legacy 01:02:03:04:05:06.
            // TinyUSB has already counted the nonzero report ID. Wrap its
            // one-byte transfer length back to zero so EP0 is stalled.
            *response_len = (uint16_t)-1;
            return true;
          }
          }
          memcpy(buffer, report_f2, len);
          if (localAddressRequired) {
            memcpy(buffer + 3, localAddress, sizeof(localAddress));
          }
          *response_len = len;
          return true;
        }
        case 0xF5: {
          uint8_t hostAddress[6] = {};
          if (reqlen >= 8 &&
              productCopySonyPs3HostAddress(hostAddress)) {
            buffer[0] = 0x01;
            buffer[1] = 0x00;
            memcpy(buffer + 2, hostAddress, sizeof(hostAddress));
            *response_len = 8;
            return true;
          }
          const uint16_t len = min(reqlen, (uint16_t) sizeof(report_f5));
          memcpy(buffer, report_f5, len);
          *response_len = len;
          return true;
        }
        case 0xF7: {
          const uint16_t len = min(reqlen, (uint16_t) sizeof(report_f7));
          memcpy(buffer, report_f7, len);
          *response_len = len;
          return true;
        }
        case 0xF8: {
          const uint16_t len = min(reqlen, (uint16_t) sizeof(report_f8));
          memcpy(buffer, report_f8, len);
          *response_len = len;
          return true;
        }
        default:
          break;
      }
    }

    // Match Sonik's exact AUTO probe behavior: returning -1 here wraps the
    // current TinyUSB xfer length back to 0 for nonzero report IDs and stalls
    // the request instead of sending a bogus 1-byte feature response.
    // TinyUSB only prepends that report-ID byte for nonzero IDs. For report
    // ID 0 the -1 sentinel would stay 0xFFFF and read past the 64-byte EP0
    // buffer; return 0 instead, which TinyUSB's xferlen > 0 assert stalls.
    *response_len = (report_id != 0) ? (uint16_t)-1 : 0;
    return true;
  }

  return false;
}

static bool handle_output_autodetect_hid_set_report(
  uint8_t report_id,
  hid_report_type_t report_type,
  uint8_t const* buffer,
  uint16_t bufsize
) {
  const bool auto_detect_ps3_feature =
    can_run_usb_detection() &&
    report_type == HID_REPORT_TYPE_FEATURE &&
    (report_id == 0xEF || report_id == 0xF5);

  if (get_effective_output_mode() != OUTPUT_SWITCHPRO &&
      !auto_detect_ps3_feature &&
      report_type == HID_REPORT_TYPE_FEATURE &&
      report_id >= 0xE0 &&
      report_id <= 0xEF) {
    webhid_set_report(report_id, buffer, bufsize);
    return true;
  }

  if (can_run_usb_detection()) {
    usb_detect_handle_hid_set(report_id, report_type, buffer, bufsize);
    return true;
  }

  return false;
}
