# Dreamcast HKT-7500 Twin Stick

## Detection

The Dreamcast profile labels a controller `Twin-Stick` only when Maple reports
the exact product name `Twin-Stick` and capability mask `0x0000FEFE`. Other
dual-D-pad devices continue through their existing profile detection, including
the Mission Stick.

## Routing and defaults

D-pad 1 remains the Dreamcast native D-pad and uses the existing D-pad mode
setting. D-pad 2 keeps its four directions in the Dreamcast `EXTRA` bits until
the final output mapping stage. Its mode is independently configurable as
D-pad, Left Stick, Right Stick, or Buttons. Digital stick routing uses full
scale; an opposing pair on one axis centers that axis.

| Input | XInput | DInput / generic HID |
| --- | --- | --- |
| D-pad 1 | D-pad | POV hat |
| D-pad 2 Up | LB | Button 15 |
| D-pad 2 Down | RB | Button 16 |
| D-pad 2 Left | LS | Button 17 |
| D-pad 2 Right | RS | Button 18 |
| Pause (D) | Back | Button 19 |
| Start | Start | Existing Start |

The existing A/B/X/Y and trigger/thumb assignments are unchanged. Twin Stick
direction and Pause assignments are available in the OLED remap menu and
Adapt.html. Quick Config exposes D-pad 1 and D-pad 2 routing when the detected
controller is the HKT-7500.

## Storage

D-pad 2 mode uses Dreamcast remap slot 13, which is unused by Dreamcast input.
The setting is encoded in that existing slot. Direction and Pause assignments
reuse slots 14–18. Remap table widths and flash record versions are unchanged;
existing records continue to load through the existing remap migration path.

## Validation status

- Host helper checks: mode encoding, full-scale axis values, opposing directions,
  and hat direction handling passed.
- Existing OLED layout test: passed.
- Adapt.html inline JavaScript syntax check: passed.
- `pio run -e classic2usb`: blocked in the current workspace because PlatformIO
  is unavailable. No UF2 was produced or inspected.
- Physical HKT-7500 behavior has not been verified on hardware.
