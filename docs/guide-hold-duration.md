# Guide Hold Duration — Classic2USB

## Purpose

One global Hold Start to Home/Guide threshold for Classic2USB, independent of
controller input mode and USB output. Original 500 ms Menu Hold default is
unchanged. Existing Hotkeys On/Off continues to enable or disable Guide.

## Adapt Web

System Settings > Menus and buttons: Hold Start Duration appears directly
below Hotkeys On/Off.

- 500, 1000, 1500, 2000, 2500, or 3000 ms
- Factory default: 2000 ms
- Hidden when connected firmware does not advertise support
- Writes only edited settings, reads back current value after save
- Reset to Defaults restores 2000 ms

## Hardware OLED

Hold MODE to open System Settings. Scroll to `Guide Hold`, immediately after
`Hotkeys`, press MODE to open six choices, select 0.5s–3.0s and save.
The existing six-choice OLED list is backed by `SettingId::GuideHoldTime`;
reserved value 0 is never offered. The same stored value is shown in Adapt Web.
No dedicated Twin Stick artwork is included.

## Implementation

- `SettingId::GuideHoldTime` is appended after existing IDs; no existing ID
  changes. Value is global and stored in previously unused
  `capture_hotkey` bits 1-3; Capture On/Off stays in bit 0.
- Existing 22-byte `GlobalSettingsRecord`, flash record layouts, and USB
  device descriptors remain unchanged.
- WebHID settings report uses former reserved byte 45 with marker `0x80`;
  write setting 50 accepts values 1-6.
- Older saved records with unused bits at zero resolve to 2000 ms.
- Classic2USB uses selected duration; other Reflex products retain original
  1000 ms Hold Start behavior.

## Validation

Version 1.0.3 source staged on `feature/classic2usb-v1.0.3-guide-hold`,
based on Classic2USB 1.0.2 Twin Stick implementation. Firmware build, source
regressions, WebHID read/write tests, and hardware OLED and timing UAT are
pending. No release has been published. Test Hotkeys On and Off, all six durations, brief Start presses,
Start+direction combinations, reboot persistence, settings reset, and old
firmware compatibility.
