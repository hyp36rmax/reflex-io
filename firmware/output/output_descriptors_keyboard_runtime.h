#pragma once

// Plain boot-keyboard descriptor for the MAME-oriented arcade keyboard output.
// Management/WebHID stays in DInput mode; keyboard mode should enumerate as a
// boring keyboard so PC hosts and MAME see key events immediately.

constexpr uint8_t KEYBOARD_REPORT_ID = 1;

const uint8_t keyboard_report_desc[] = {
  TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)),
  // MiSTer's Stadia force-feedback driver binds this VID/PID and requires
  // output report 5 even though a keyboard has no motor to drive.
  0x06, 0x00, 0xFF,  // Vendor Defined 0xFF00
  0x09, 0x01,
  0xA1, 0x01,        // Application
  0x05, 0x0F,        // Physical Interface Device
  0x85, 0x05,        // Report ID (5)
  0x09, 0x97,        // DC Enable Actuators
  0x15, 0x00,
  0x26, 0xFE, 0xFF,  // Logical Maximum (65534)
  0x75, 0x10,
  0x95, 0x02,
  0x91, 0x02,        // Output (Data,Var,Abs): strong/weak 16-bit values
  0xC0               // End Collection
};
