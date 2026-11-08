# Reflex Adapt Firmware Third-Party Notices

Reflex Adapt firmware contains original project code together with third-party
source, ported code, protocol references, firmware library integrations and
lookup data. This notice summarizes attribution and provenance for those
materials.

This notice does not replace the license terms in source headers or copied
license files. When terms differ, the upstream source header or copied license
text controls. Paths below are relative to the Reflex Adapt source repository.
Copied upstream license files live under `third_party/licenses/` unless the
license already ships beside the vendored source.

## Top-Level License

Unless a file says otherwise, Reflex Adapt firmware source, including
Classic2USB, is released under GPL-3.0-or-later. This is the conservative
top-level license for the firmware because the source tree includes GPLv3-family
controller libraries and LGPL components. The GPL-3.0 text is in the source
repository's `LICENSE` file and is also included as `GPL-3.0.txt` on the
Classic2USB USB drive.

The Modern2USB ESP32 firmware is the exception: our own source for it is
proprietary and unpublished. The third-party parts of that image keep their
licenses and are listed below.

## Project Lineage

- Reflex Adapt Classic2USB builds on the RetroZord / Reflex firmware lineage by
  sonik-br: https://github.com/sonik-br/RetroZordAdapter
- Reflex Adapt hardware and firmware, including the original Reflex Adapt V1,
  by MiSTer Addons / Reflex Adapt.

## Component Notices

| Component | Local Use | License / Notice Location |
|-----------|-----------|---------------------------|
| [RetroZord and sonik-br controller libraries](https://github.com/sonik-br/RetroZordAdapter) | Firmware lineage and classic controller libraries, including JoybusLib, SnesLib, SaturnLib, PceLib, JaguarLib, and ThreedoLib | `third_party/licenses/*Lib-LICENSE`; source headers where present |
| [GP2040-CE / OpenStickCommunity](https://github.com/OpenStickCommunity/GP2040-CE) | PS4 key-bundle conventions, XInput/auth guidance, and Xbox One/XGIP donor implementation details | `third_party/licenses/GP2040-CE-LICENSE`; SPDX headers in copied/ported files |
| [SDL GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB) | Pinned Linux/USB SDL-backend mappings compiled as a best-effort Modern2USB raw-HID suggestion seed | zlib license; `third_party/licenses/SDL_GameControllerDB-LICENSE.txt` |
| [TinyUSB](https://github.com/hathach/tinyusb) / [Adafruit TinyUSB Arduino](https://github.com/adafruit/Adafruit_TinyUSB_Arduino) | USB device/host stack and Arduino integration | `third_party/Adafruit_TinyUSB_Arduino/LICENSE` |
| [BlueRetro by darthcloud](https://github.com/darthcloud/BlueRetro) | Modern2USB ESP32 Classic Bluetooth and BLE controller host, decoding, bonding, and feedback at pinned commit `e1a9831a875f5313a923160a1379a7ebbfaa2b11` | Apache-2.0; `firmware/targets/modern2usb/esp32/blueretro/LICENSE`. `firmware/targets/modern2usb/esp32/blueretro/UPSTREAM.md` lists the files we changed, and each carries a change notice on its first line. BlueRetro's own third-party files keep their notices: Zephyr (Apache-2.0), Espressif (Apache-2.0), Daniel Frejek (MIT), Hector Martin (GPL-2.0-only OR GPL-3.0-only OR MIT) and Thanos Siopoudis (Apache-2.0) |
| [liblfds](https://liblfds.org/) 7.1.1, from [darthcloud/liblfds7.1.1](https://github.com/darthcloud/liblfds7.1.1) at commit `da3494fef10df4681e267d8b2b8cce2c90d5a9fa` | Modern2USB ESP32 image: the bounded single-producer, single-consumer queue behind BlueRetro's `queue_bss`, unmodified | Upstream ships no license file; `third_party/licenses/liblfds-license-statement.md` records its published terms |
| [ESP-IDF by Espressif](https://github.com/espressif/esp-idf) 5.5.0, built through pioarduino 55.03.30-2 | Modern2USB ESP32 image: bootloader, system, drivers, HTTP server, NVS and OTA. The Bluetooth controller, Wi-Fi, PHY and coexistence libraries (`libbtdm_app`, `libcoexist`, `libcore`, `libespnow`, `libnet80211`, `libphy`, `libpp`, `librtc`) link as binaries, and upstream publishes no source for them | Apache-2.0; `third_party/licenses/ESP-IDF-LICENSE`, the same text as the `LICENSE` beside each binary library |
| [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) 3.6.3, through ESP-IDF | Modern2USB ESP32 image: `libmbedcrypto` for update signature checks, PS4 signing and Wi-Fi security | Apache-2.0 OR GPL-2.0-or-later; `third_party/licenses/mbedTLS-LICENSE` |
| [lwIP](https://savannah.nongnu.org/projects/lwip/) 2.2.0, through ESP-IDF | Modern2USB ESP32 image: TCP/IP for the home-network portal | BSD-3-Clause; `third_party/licenses/lwIP-LICENSE` |
| [wpa_supplicant by Jouni Malinen and contributors](https://w1.fi/wpa_supplicant/), through ESP-IDF | Modern2USB ESP32 image: Wi-Fi station and access-point security | BSD license; `third_party/licenses/wpa_supplicant-README` and `third_party/licenses/wpa_supplicant-COPYING` |
| [FreeRTOS Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) V10.5.1, through ESP-IDF | Modern2USB ESP32 image: scheduler | MIT; `third_party/licenses/FreeRTOS-Kernel-LICENSE.md` |
| [http_parser](https://github.com/nodejs/http-parser), through ESP-IDF | Modern2USB ESP32 image: HTTP request parsing for the portal; based on NGINX | MIT-style; `third_party/licenses/http_parser-LICENSE.txt` |
| [SPIFFS by Peter Andersson](https://github.com/pellepl/spiffs), through ESP-IDF | Modern2USB ESP32 image: BlueRetro configuration storage | MIT; `third_party/licenses/SPIFFS-LICENSE` |
| [TLSF by Matthew Conte](https://github.com/mattconte/tlsf), through ESP-IDF | Modern2USB ESP32 image: heap allocator | BSD-3-Clause; SPDX header in ESP-IDF's `components/heap/tlsf/tlsf.c` |
| Xtensa port and HAL by Cadence Design Systems, through ESP-IDF | Modern2USB ESP32 image: interrupt vectors and `libxt_hal` | MIT; license text in ESP-IDF's `components/xtensa/include/xtensa_context.h` |
| [newlib](https://sourceware.org/newlib/), in the Espressif Xtensa toolchain 14.2.0 | Modern2USB ESP32 image: C library (`libc`, `libm`) | Collection of permissive licenses; `third_party/licenses/newlib-COPYING.NEWLIB` |
| [GCC runtime libraries](https://gcc.gnu.org/), in the same toolchain | Modern2USB ESP32 image: `libgcc` and `libstdc++` | GPL-3.0 with the GCC Runtime Library Exception; `third_party/licenses/GCC-COPYING.RUNTIME` |
| [Pico-PIO-USB by sekigon-gonnoc](https://github.com/sekigon-gonnoc/Pico-PIO-USB) | RP2040 PIO USB support | `third_party/Pico_PIO_USB/LICENSE` |
| [Arduino-Pico by Earle F. Philhower III and contributors](https://github.com/earlephilhower/arduino-pico) | RP2040 Arduino core and bundled build libraries | `third_party/licenses/arduino-pico-LICENSE` |
| [jvsio by Takashi Toyoshima](https://github.com/toyoshim/jvsio) | JVS packet framing, device discovery, host polling, and response parsing in the integrated and standalone JVSIO2USB implementations | BSD-3-Clause; `third_party/licenses/toyoshim-jvsio-BSD-3-Clause-LICENSE`; copyright headers retained in `JVSIO_c.c` and `JVSIO_c.h` |
| [BearSSL by Thomas Pornin](https://bearssl.org/) | RSA-PSS-SHA256 primitives used for PS4 authentication | `third_party/licenses/BearSSL-LICENSE.txt` |
| [libxsm3 by InvoxiPlayGames](https://github.com/InvoxiPlayGames/libxsm3), with [usbdsec by oct0xor](https://github.com/oct0xor/usbdsec) | Xbox 360 security/authentication support | License retained with vendored source |
| [NintendoExtensionCtrl by David Madison](https://github.com/dmadison/NintendoExtensionCtrl) | Wii extension controller protocol library | `third_party/licenses/NintendoExtensionCtrl-LICENSE` |
| [SSD1306Ascii by Bill Greiman](https://github.com/greiman/SSD1306Ascii) | OLED text rendering | `third_party/licenses/SSD1306Ascii-LICENSE.md` and local source headers |
| [U8g2 by olikraus](https://github.com/olikraus/u8g2) | OLED graphics/font support | License retained with vendored source |
| [PsxNewLib by SukkoPera](https://github.com/SukkoPera/PsxNewLib) | PlayStation controller protocol library | `third_party/licenses/PsxNewLib-LICENSE`; source headers |
| [BlueRetro 3DO protocol research](https://hackaday.io/project/170365-blueretro/log/190948-3do-interface), [Opera PBUS](https://github.com/libretro/opera-libretro/blob/master/libopera/opera_pbus.c), [PC Engine Controller Info](https://github.com/pce-devel/PCE_Controller_Info), [KallistiOS mouse driver](https://github.com/KallistiOS/KallistiOS/blob/master/kernel/arch/dreamcast/hardware/maple/mouse.c), and Philips *Technical Info: Pointing Devices* (August 1990) | Classic2USB 3DO, PC Engine, Dreamcast and CD-i mouse protocol references; independently implemented decoding, no source imported in this integration | Protocol research and source boundaries: `docs/internal/classic2usb/mouse-input-output.md`, `docs/internal/classic2usb/cdi.md` |
| [Doridian Joybus-PIO](https://github.com/Doridian/Joybus-PIO) | Joybus PIO transport reference wrapped by JoybusLib | `third_party/licenses/Doridian-Joybus-PIO-LICENSE` |
| [Dreamcast controller USB Pico](https://github.com/sega-dreamcast/dreamcast-controller-usb-pico) | Dreamcast Maple protocol research and source-derived implementation details | `third_party/licenses/DreamcastControllerUsbPico-LICENSE.md`; local MapleLib notices where applicable |
| [Santroller](https://github.com/santroller/santroller) | Host/console detection guidance and XGIP protocol research | `third_party/licenses/Santroller-LICENSE`; local notices where applicable |
| [bootsector PS3PadMicro / KADE](https://github.com/bootsector/PS3PadMicro) | PS3 feature-report byte provenance | `third_party/licenses/bootsector-PS3PadMicro-LICENSE.txt`; local notices where applicable |
| [GIMX-firmwares by Mathieu Laurendeau](https://github.com/matlo/GIMX-firmwares/tree/master/EMUPS3) | Native DS3 HID descriptor and feature templates match EMUPS3; wired control protocol reference | GPLv3; `third_party/licenses/GIMX-firmwares-GPL-3.0-LICENSE`; local descriptor/template notices |
| [Windows Switch 2 Pro Controller by amccomb](https://github.com/Altureus/Windows-Switch2-Pro-Controller) | Modern2USB Switch 2 Pro Controller input: `057E:2069` vendor-interface wake sequence, HID report layout, button bits, and initial stick calibration reference | MIT; `third_party/licenses/Windows-Switch2-Pro-Controller-LICENSE` |
| [ndeadly switch2_controller_research](https://github.com/ndeadly/switch2_controller_research) | Switch 2 Pro output (`057E:2069`): the captured 80-byte configuration descriptor and 97-byte HID report descriptor, the captured stick parameter (calibration) blocks and motion-scale constants, and the command, report and memory-layout documentation. Taken from the repository's documentation and its USB capture `captures/usb/rumble-procon-gccon.pcapng.gz`; the capture file itself is not included | The upstream repository declares no license (checked 2026-10-23); no license text is copied |
| [SDL](https://github.com/libsdl-org/SDL) `src/joystick/hidapi/SDL_hidapi_switch2.c` | Switch 2 Pro output: rumble amplitude scale (full scale 29000), stick direction and report 0x05 field offsets, used as a reference; no SDL source is vendored | zlib license (upstream) |
| [PicoSwitch2 by notsosaelin](https://github.com/notsosaelin/PicoSwitch2) | Switch 2 Pro output: console-tested behaviour reference (the firmware version tuple that avoids the update prompt, the stored wired challenge answer, bare acknowledgement and command 0x18 replies); no code copied | Apache-2.0 (upstream); nothing copied, so no license copy is bundled |
| [MiSTer_i2c2oled](https://github.com/venice1200/MiSTer_i2c2oled) | Compatible `.pix` artwork format and MiSTer core-name workflow reference; no upstream source or artwork is vendored | External GPL-3.0 project; install artwork separately |
| [MiSTer_i2c2oled_Pictures](https://github.com/venice1200/MiSTer_i2c2oled_Pictures) | External PIX source downloaded by the Reflex Core Art helper; no artwork is vendored or repackaged | Upstream artwork attribution remains with Venice1200 and contributors; no repository-wide license declared, install from upstream separately |
| [niemasd/GameDB-GB](https://github.com/niemasd/GameDB-GB) and [niemasd/GameDB-GBC](https://github.com/niemasd/GameDB-GBC) | Game Boy / Game Boy Color title lookup data used by Adapt.html for Transfer Pak cartridge names | `third_party/licenses/niemasd-GameDB-GPL-3.0-LICENSE` |
| [bucanero/dreamcast-saves](https://github.com/bucanero/dreamcast-saves) | Dreamcast VMU Backup CD folder/title lookup data used by Adapt.html for save identification | `third_party/licenses/bucanero-dreamcast-saves-GPL-3.0-LICENSE` |

## License File Copies

Original upstream license files that were not already present beside vendored
source are copied under `third_party/licenses/`; its `README.md` maps each file
to its upstream. Source-adjacent license copies are retained for:

- `third_party/Adafruit_TinyUSB_Arduino/LICENSE`
- `third_party/Pico_PIO_USB/LICENSE`
- `firmware/targets/modern2usb/esp32/blueretro/LICENSE`
- libxsm3 source-adjacent license
- U8g2 source-adjacent license

The copied license bundle covers Santroller, bootsector PS3PadMicro,
GP2040-CE, GIMX-firmwares, PsxNewLib, NintendoExtensionCtrl, sonik-br
controller libraries, Doridian Joybus-PIO, DreamcastControllerUsbPico,
SSD1306Ascii, BearSSL, Arduino-Pico, Toyoshima jvsio, SDL GameControllerDB,
the amccomb Windows Switch 2 Pro Controller (MIT), and the Modern2USB ESP32
image's ESP-IDF, Mbed TLS, lwIP, wpa_supplicant, FreeRTOS Kernel, http_parser,
SPIFFS, newlib and GCC runtime components. GameDB-GB, GameDB-GBC, and
bucanero/dreamcast-saves use the GPL-3.0 license copies noted above.
No license copy is bundled for the ndeadly research (none is declared
upstream), SDL (no SDL source is vendored) or PicoSwitch2 (nothing is copied).
liblfds publishes no license file, so its terms are recorded in
`third_party/licenses/liblfds-license-statement.md`. TLSF and the Cadence Xtensa
port state their terms in source headers rather than a license file.
Our own Switch 2 Pro research is under
`third_party/licenses/Reflex-Switch2-Pro-Research-LICENSE`, which does not
cover the ndeadly material.

## License Scope

This source tree includes GPLv3 and LGPL-family components. See the copied
upstream license files for the applicable terms.
