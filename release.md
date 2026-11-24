# Reflex Adapt Classic2USB 1.0.3

This update adds configurable global Hold Start/Guide timing. Twin Stick
support and the original Classic2USB OLED presentation are retained from
1.0.2.

## Changes

- Replace Classic2USB fixed 1,000 ms Start-to-Guide hold threshold with a
  configurable 500-3,000 ms global duration, in 500 ms steps.
- Default Hold Start duration is 2,000 ms.
- Adapt Web > System Settings > Menus and buttons > Hold Start Duration.
- Hardware: hold MODE > System Settings > Guide Hold, directly below Hotkeys.
- Existing Hotkeys On/Off still enables/disables Start shortcuts globally.
- General Menu Hold default remains 500 ms.
- Duration is saved with other global settings and survives reboot.
- HKT-7500 XInput/DInput mappings, remappable Pause, and standard OLED
  presentation are unchanged. Dedicated Twin Stick OLED art remains deferred.

## Validation

Source staged on `feature/classic2usb-v1.0.3-guide-hold`.
Firmware compilation, software regression run, WebHID read/write checks,
physical timing UAT, and hardware MODE-menu verification remain pending.
No UF2 or GitHub release published yet.
