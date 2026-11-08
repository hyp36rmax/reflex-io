#include "modern2usb_analog_policy.h"

const char* modern2usbAnalogFamilyName(
    Modern2UsbAnalogControllerFamily family) {
  switch (family) {
    case MODERN2USB_ANALOG_FAMILY_DUALSHOCK3: return "DS3";
    case MODERN2USB_ANALOG_FAMILY_DUALSHOCK4: return "DS4";
    case MODERN2USB_ANALOG_FAMILY_DUALSENSE: return "DualSense";
    case MODERN2USB_ANALOG_FAMILY_SWITCH: return "Switch";
    case MODERN2USB_ANALOG_FAMILY_SWITCH2: return "Switch2";
    case MODERN2USB_ANALOG_FAMILY_XBOX: return "Xbox";
    case MODERN2USB_ANALOG_FAMILY_WII_U_PRO: return "WiiU";
    case MODERN2USB_ANALOG_FAMILY_WII: return "Wii";
    case MODERN2USB_ANALOG_FAMILY_8BITDO: return "8BitDo";
    case MODERN2USB_ANALOG_FAMILY_GENERIC: return "Generic";
    case MODERN2USB_ANALOG_FAMILY_UNKNOWN:
    default:
      return "Unknown";
  }
}

uint32_t modern2usbAnalogIntegerSqrt(uint64_t value) {
  uint64_t result = 0;
  uint64_t bit = uint64_t{1} << 62;
  while (bit > value) bit >>= 2;
  while (bit != 0) {
    if (value >= result + bit) {
      value -= result + bit;
      result = (result >> 1) + bit;
    } else {
      result >>= 1;
    }
    bit >>= 2;
  }
  return static_cast<uint32_t>(result);
}

uint32_t modern2usbAnalogNoisePpm(
    uint16_t radialNoisePeak, uint16_t axisSpan) {
  if (axisSpan == 0) axisSpan = MODERN2USB_ANALOG_FULL_SCALE;
  return static_cast<uint32_t>(
      (static_cast<uint64_t>(radialNoisePeak) * 1000000u +
       axisSpan / 2u) /
      axisSpan);
}
