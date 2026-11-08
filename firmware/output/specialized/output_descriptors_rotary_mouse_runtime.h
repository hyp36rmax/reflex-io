#pragma once
#include "../output_descriptors_identity.h"

// A rotary controller as a relative USB mouse. Rotation moves one axis or
// the wheel (the per-mode Mouse Axis setting); queued motion is consumed only
// after a report is accepted, as for MiSTer Spinner.
//
// The mouse is its own top-level collection so hosts treat it as a pointer.
// Browsers withhold every report inside a mouse collection from WebHID, so
// the configuration reports live in a second, vendor collection. A device
// with two top-level collections must number every report.
static constexpr uint8_t ROTARY_MOUSE_REPORT_ID = 1;

static const uint8_t rotarymouse_report_descriptor[] = {
  0x05, 0x01,        // Generic Desktop
  0x09, 0x02,        // Mouse
  0xA1, 0x01,        // Application
  0x85, ROTARY_MOUSE_REPORT_ID,
  0x09, 0x01,        //   Pointer
  0xA1, 0x00,        //   Physical
  0x05, 0x09,        //     Buttons 1-5
  0x19, 0x01,
  0x29, 0x05,
  0x15, 0x00,
  0x25, 0x01,
  0x75, 0x01,
  0x95, 0x05,
  0x81, 0x02,
  0x75, 0x03,        //     Padding
  0x95, 0x01,
  0x81, 0x03,
  0x05, 0x01,
  0x09, 0x30,        //     X
  0x09, 0x31,        //     Y
  0x09, 0x38,        //     Wheel
  0x15, 0x81,        //     -127
  0x25, 0x7F,        //     +127
  0x75, 0x08,
  0x95, 0x03,
  0x81, 0x06,        //     Relative signed movement
  0xC0,              //   End Collection
  0xC0,              // End Collection
  0x06, 0x00, 0xFF,  // Vendor Defined 0xFF00
  0x09, 0x01,
  0xA1, 0x01,        // Application
  // MiSTer's Stadia force-feedback driver binds this VID/PID and requires
  // output report 5 even though a mouse has no motor to drive.
  0x05, 0x0F,        // Physical Interface Device
  0x85, 0x05,        // Report ID (5)
  0x09, 0x97,        // DC Enable Actuators
  0x15, 0x00,
  0x26, 0xFE, 0xFF,  // Logical Maximum (65534)
  0x75, 0x10,
  0x95, 0x02,
  0x91, 0x02,        // Output (Data,Var,Abs): strong/weak 16-bit values
  // WebHID Configuration Reports (0xE0-0xE7)
  0x06, 0x00, 0xFF,               // Usage Page (Vendor Defined 0xFF00)
  0x15, 0x00,                     // Logical Minimum (0)
  0x26, 0xFF, 0x00,               // Logical Maximum (255)
  0xA1, 0x02,                     // Collection (Logical)
  REFLEX_IDENTITY_FEATURE_REPORT_DESC(),
  0x85, 0xE0,                     // Report ID (0xE0) - Device Info
  0x75, 0x08,                     // Report Size (8)
  0x95, 0x3F,                     // Report Count (63)
  0x09, 0x01,                     // Usage (0x01)
  0xB1, 0x02,                     // Feature (Data,Var,Abs)
  0xC0,                           // End Collection
  0xA1, 0x02,                     // Collection (Logical)
  0x85, 0xE3,                     // Report ID (0xE3) - Commands
  0x75, 0x08,                     // Report Size (8)
  0x95, 0x3F,                     // Report Count (63)
  0x09, 0x01,                     // Usage (0x01)
  0xB1, 0x02,                     // Feature (Data,Var,Abs)
  0xC0,                           // End Collection
  0xA1, 0x02,                     // Collection (Logical)
  0x85, 0xE4,                     // Report ID (0xE4) - Key Status
  0x75, 0x08,                     // Report Size (8)
  0x95, 0x3F,                     // Report Count (63)
  0x09, 0x01,                     // Usage (0x01)
  0xB1, 0x02,                     // Feature (Data,Var,Abs)
  0xC0,                           // End Collection
  0xA1, 0x02,                     // Collection (Logical)
  0x85, 0xE5,                     // Report ID (0xE5) - Key Write
  0x75, 0x08,                     // Report Size (8)
  0x95, 0x3F,                     // Report Count (63)
  0x09, 0x01,                     // Usage (0x01)
  0xB1, 0x02,                     // Feature (Data,Var,Abs)
  0xC0,                           // End Collection
  0xA1, 0x02,                     // Collection (Logical)
  0x85, 0xE6,                     // Report ID (0xE6) - Key Clear
  0x75, 0x08,                     // Report Size (8)
  0x95, 0x3F,                     // Report Count (63)
  0x09, 0x01,                     // Usage (0x01)
  0xB1, 0x02,                     // Feature (Data,Var,Abs)
  0xC0,                           // End Collection
  0xA1, 0x02,                     // Collection (Logical)
  0x85, 0xE7,                     // Report ID (0xE7) - Input State
  0x75, 0x08,                     // Report Size (8)
  0x95, 0x3F,                     // Report Count (63)
  0x09, 0x01,                     // Usage (0x01)
  0xB1, 0x02,                     // Feature (Data,Var,Abs)
  0xC0,                           // End Collection
  0xC0,                           // End Collection
};

// Buttons 1-5 are Left, Right, Middle, Back and Forward. A positive wheel
// scrolls up.
typedef struct TU_ATTR_PACKED {
  uint8_t buttons;
  int8_t x;
  int8_t y;
  int8_t wheel;
} usbout_rotarymouse_report_t;

static_assert(sizeof(usbout_rotarymouse_report_t) == 4,
              "Rotary mouse report must match its HID descriptor");
