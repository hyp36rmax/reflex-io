# Dreamcast HKT-7500 Twin Stick

## Detection

The Dreamcast profile labels a controller `Twin-Stick` only when Maple reports
the product name `Twin-Stick` or `Twin Stick` and the exact capability mask
`0x0000FEFE`. Other dual-D-pad devices continue through their existing profile
detection, including the Mission Stick.

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
controller is the HKT-7500. In the OLED remap menu, select a Twin Stick source,
then use the menu navigation controls to cycle through destinations supported
by the selected output. Select confirms the destination; Save writes the
remaps. Other controller profiles keep their existing physical-button
destination selection.

When the Hold Start setting is enabled, holding Start alone for 1000 ms on a
detected Twin Stick sends a brief Home/Guide signal. A short press remains
Start. Standard Dreamcast controllers do not use this shortcut.

Mixed Dreamcast ports share the Dreamcast input/output remap pair. Automatic
Twin Stick initialization checks every connected Dreamcast frame. Direction
and Pause transformations remain gated by each frame's own controller type, so
a standard pad on another port does not receive Twin Stick inputs.

## Storage

D-pad 2 mode uses Dreamcast remap slot 13, which is unused by Dreamcast input.
The setting is encoded in that existing slot. Direction and Pause assignments
reuse slots 14–18. Remap table widths and flash record versions are unchanged;
existing records continue to load through the existing remap migration path.

## Validation status

- **PASS** — HKT-7500 identification and D-pad 2 routing in physical UAT.
- **PASS** — 8 Twin Stick regression tests and 11 OLED layout tests.
- **PASS** — Classic2USB build at `f4cff076`; this build predates the Twin Stick-
  only Start hold shortcut added in `6854deb`.
- **NOT RUN** — Physical validation of the Start hold shortcut in a firmware
  build containing `6854deb`.

Host-side mixed-port and remap tests do not replace Windows input-report
validation.

## Last validated build before the Start hold change

- Command: `PLATFORMIO_CORE_DIR=/tmp/reflex-platformio /tmp/reflex-pio312/bin/pio run -e classic2usb`
- Result: **PASS**
- UF2: `.pio/build/classic2usb/firmware.uf2` (3,682,816 bytes)
- SHA-256: `7e2afbae9a5aac1af8649888c95340f909e7c8398602fc717d3246aa217d9b15`
- Memory: 187,308 / 262,144 bytes RAM (71.5%); 1,829,204 / 2,068,480 bytes flash (88.4%)
- Compiler warnings: 8 identical `-Wstringop-overflow` warnings from the bundled
  RP2040 lwIP `dhcp.c` DHCP option writer; no project-source warnings.

Host checks cover direction/source routing, remap initialization and reset
paths, output-pair persistence calls, and port-scoped Twin Stick detection.
They do not simulate flash power loss or validate a physical adapter.
