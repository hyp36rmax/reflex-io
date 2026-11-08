#pragma once
#include "../output_descriptors_identity.h"

// MiSTer routes relative Generic Desktop Dial to its dedicated spinner input.
// Motion is consumed only after successful USB report submission.
static constexpr uint8_t MISTER_SPINNER_REPORT_ID = 1;

static const uint8_t spinnermister_report_descriptor[] = {
  0x05, 0x01,        // Generic Desktop
  0x09, 0x04,        // Joystick
  0xA1, 0x01,        // Application
  0x85, MISTER_SPINNER_REPORT_ID, // Number input alongside output/feature reports
  0x05, 0x09,        // Buttons
  0x19, 0x01,
  0x29, 0x10,
  0x15, 0x00,
  0x25, 0x01,
  0x75, 0x01,
  0x95, 0x10,
  0x81, 0x02,        // 16 button bits
  0x05, 0x01,
  0x09, 0x37,        // Dial (Linux REL_DIAL, MiSTer spinner)
  0x15, 0x81,        // -127
  0x25, 0x7F,        // +127
  0x75, 0x08,
  0x95, 0x01,
  0x81, 0x06,        // Relative signed movement
  0x09, 0x39,        // Hat switch
  0x15, 0x00,
  0x25, 0x07,
  0x35, 0x00,
  0x46, 0x3B, 0x01,
  0x65, 0x14,
  0x81, 0x42,        // Eight directions; 8 is neutral
  0x65, 0x00,
  // MiSTer's Stadia force-feedback driver binds this VID/PID and requires
  // output report 5 even though a relative spinner has no motor to drive.
  0x05, 0x0F,        // Physical Interface Device
  0x85, 0x05,        // Report ID (5)
  0x09, 0x97,        // DC Enable Actuators
  0x15, 0x00,
  0x26, 0xFE, 0xFF, // Logical Maximum (65534)
  0x75, 0x10,
  0x95, 0x02,
  0x91, 0x02,        // Output (Data,Var,Abs): strong/weak 16-bit values
  // WebHID Configuration Reports (0xE0-0xE6)
  0x06, 0x00, 0xFF,               // Usage Page (Vendor Defined 0xFF00)
  0x15, 0x00,                     // Logical Minimum (0)
  0x26, 0xFF, 0x00,               // Logical Maximum (255) for byte reports
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
  0xc0,                             // END_COLLECTION
};

typedef struct TU_ATTR_PACKED {
  uint16_t buttons;
  int8_t spinner_axis;
  uint8_t direction;
} usbout_spinnermister_report_t;

static_assert(sizeof(usbout_spinnermister_report_t) == 4,
              "Spinner report must match its HID descriptor");
