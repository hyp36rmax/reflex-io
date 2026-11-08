# Third-Party License Copies

This directory stores upstream license texts for third-party source,
protocol-reference code, and documentation that Reflex Adapt 2 either vendors,
ports from, or explicitly uses as implementation provenance.

These files do not replace source-adjacent upstream license files. See
`../../NOTICE.md` (identical to the packaged `../../docs/NOTICE.md`) for
third-party attribution and provenance.

| File | Upstream | Local Use |
|------|----------|-----------|
| `arduino-pico-LICENSE` | `earlephilhower/arduino-pico` | Arduino-Pico core and bundled libraries used by the PlatformIO build |
| `BearSSL-LICENSE.txt` | BearSSL | RSA-PSS-SHA256 library used by PS4 auth through Arduino-Pico |
| `bootsector-PS3PadMicro-LICENSE.txt` | `bootsector/PS3PadMicro` | PS3 feature-report magic bytes reference |
| `bucanero-dreamcast-saves-GPL-3.0-LICENSE` | `bucanero/dreamcast-saves` | Dreamcast VMU Backup CD folder/title lookup data used by Adapt.html |
| `Doridian-Joybus-PIO-LICENSE` | `Doridian/Joybus-PIO` | Joybus PIO transport reference wrapped by `JoybusLib` |
| `DreamcastControllerUsbPico-LICENSE.md` | `sega-dreamcast/dreamcast-controller-usb-pico` | Dreamcast Maple PIO/source reference |
| `ESP-IDF-LICENSE` | `espressif/esp-idf` 5.5.0 | MODERN2USB ESP32 image: framework, bootloader and the binary radio libraries, which carry the same text |
| `FreeRTOS-Kernel-LICENSE.md` | `FreeRTOS/FreeRTOS-Kernel` V10.5.1, through ESP-IDF | MODERN2USB ESP32 image: scheduler |
| `GCC-COPYING.RUNTIME` | GCC 14.2.0, in the Espressif Xtensa toolchain | MODERN2USB ESP32 image: `libgcc` and `libstdc++` under the GCC Runtime Library Exception |
| `GIMX-firmwares-GPL-3.0-LICENSE` | `matlo/GIMX-firmwares` (EMUPS3) | Native DS3 HID descriptor and feature templates; the same GPL-3.0 text is embedded as `GPL-3.0.txt` on the Classic2USB USB drive and the host and N64 capture drives |
| `GP2040-CE-LICENSE` | `OpenStickCommunity/GP2040-CE` | PS4 key-bundle conventions and Xbox One/XGIP donor code |
| `http_parser-LICENSE.txt` | `nodejs/http-parser`, through ESP-IDF | MODERN2USB ESP32 image: portal HTTP request parsing |
| `JaguarLib-LICENSE` | `sonik-br/JaguarLib` | Jaguar input library lineage |
| `liblfds-license-statement.md` | liblfds 7.1.1 (not an upstream file: upstream has none) | MODERN2USB ESP32 image: BlueRetro `queue_bss` queue; records where its terms are published |
| `lwIP-LICENSE` | lwIP 2.2.0, through ESP-IDF | MODERN2USB ESP32 image: portal TCP/IP |
| `mbedTLS-LICENSE` | `Mbed-TLS/mbedtls` 3.6.3, through ESP-IDF | MODERN2USB ESP32 image: update signature checks, PS4 signing and Wi-Fi security |
| `newlib-COPYING.NEWLIB` | newlib, in the Espressif Xtensa toolchain; copy from ESP-IDF | MODERN2USB ESP32 image: C library |
| `niemasd-GameDB-GPL-3.0-LICENSE` | `niemasd/GameDB-GB` and `niemasd/GameDB-GBC` | Game Boy / Game Boy Color title lookup data used by Adapt.html |
| `NintendoExtensionCtrl-LICENSE` | `dmadison/NintendoExtensionCtrl` | Wii extension controller library |
| `PceLib-LICENSE` | `sonik-br/PceLib` | PC Engine input library lineage |
| `PsxNewLib-LICENSE` | `SukkoPera/PsxNewLib` | PlayStation controller protocol library |
| `SDL_GameControllerDB-LICENSE.txt` | `mdqinc/SDL_GameControllerDB` | Pinned Linux/USB SDL-backend data compiled into the MODERN2USB best-effort raw-HID suggestion seed |
| `Santroller-LICENSE` | `Santroller/Santroller` | Host/console detection documentation and Xbox One research provenance |
| `Reflex-Switch2-Pro-Research-LICENSE` | MiSTer Addons (first party) | Our own Switch 2 Pro output research; it does not cover the `ndeadly` material below |
| `SaturnLib-LICENSE` | `sonik-br/SaturnLib` | Saturn/Genesis input library lineage |
| `SnesLib-LICENSE` | `sonik-br/SnesLib` | SNES/NES/Virtual Boy input library lineage |
| `SPIFFS-LICENSE` | `pellepl/spiffs`, through ESP-IDF | MODERN2USB ESP32 image: BlueRetro configuration storage |
| `SSD1306Ascii-LICENSE.md` | `greiman/SSD1306Ascii` | OLED text rendering library |
| `ThreedoLib-LICENSE` | `sonik-br/ThreedoLib` | 3DO input library lineage |
| `toyoshim-jvsio-BSD-3-Clause-LICENSE` | `toyoshim/jvsio` | JVS packet framing, discovery, host polling, and response parsing |
| `wpa_supplicant-COPYING`, `wpa_supplicant-README` | wpa_supplicant, through ESP-IDF | MODERN2USB ESP32 image: Wi-Fi security; `COPYING` points to the BSD terms in `README` |
| `Windows-Switch2-Pro-Controller-LICENSE` | `Altureus/Windows-Switch2-Pro-Controller` (MIT, amccomb) | MODERN2USB Switch 2 Pro Controller input: vendor-interface wake sequence, HID report layout, button bits and initial stick calibration reference |

## Credited without a copy here

| Upstream | Local Use | Why no file |
|----------|-----------|-------------|
| `ndeadly/switch2_controller_research` | Switch 2 Pro output: captured configuration and HID report descriptors, stick parameter (calibration) blocks and motion-scale constants; command and memory-layout documentation | The repository declares no license (checked 2026-10-23) |
| `libsdl-org/SDL` (`SDL_hidapi_switch2.c`, zlib) | Switch 2 Pro output: rumble amplitude scale, stick direction and report 0x05 offsets as a reference | No SDL source is vendored |
| `notsosaelin/PicoSwitch2` (Apache-2.0) | Switch 2 Pro output: console-tested behaviour reference | No code or data is copied |
| `mattconte/tlsf` (BSD-3-Clause) | MODERN2USB ESP32 image: heap allocator, through ESP-IDF | SPDX header only in ESP-IDF's `components/heap/tlsf/tlsf.c` |
| Cadence Xtensa port and HAL (MIT) | MODERN2USB ESP32 image: interrupt vectors and `libxt_hal`, through ESP-IDF | License text sits in ESP-IDF source headers, such as `components/xtensa/include/xtensa_context.h` |
