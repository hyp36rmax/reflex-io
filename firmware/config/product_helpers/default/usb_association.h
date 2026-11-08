#pragma once

static inline void productCaptureSonyUsbAssociationSetReport(
    uint8_t, hid_report_type_t, const uint8_t*, uint16_t,
    outputMode_t, bool) {}

static inline bool productSonyPs3LocalAddressRequired() {
  return false;
}

static inline bool productCopySonyPs3LocalAddress(uint8_t[6]) {
  return false;
}

static inline bool productCopySonyPs3HostAddress(uint8_t[6]) {
  return false;
}
