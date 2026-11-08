#pragma once

#include "../../output/output_mode.h"

// The opt-in, read-only USB drive: the offline manual, the MiSTer controller
// maps, Adapt Manager and their install scripts, built into a FAT12 image by
// tools/generate_usb_drive_image.py. Call it after the gamepad interfaces are
// added, so their interface numbers, which MiSTer uses in its controller
// ids, never depend on the drive. out_usb.h includes this inside extern "C".
extern "C++" void usb_drive_configure(outputMode_t effectiveMode);
// Diagnostic-only frozen media publication; other builds provide a no-op.
extern "C++" void usb_drive_publish_psx_capture();
extern "C++" void usb_drive_publish_genesis_capture();
