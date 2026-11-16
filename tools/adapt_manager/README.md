# Reflex Adapt Manager

Reflex Adapt Manager is the single MiSTer management entry point for current
and legacy Reflex Adapt hardware. Its source, catalog, release integration, and
MiSTer Downloader database live in this repository.

MiSTer Downloader only installs or updates the manager. The manager detects
attached Adapt hardware and owns firmware selection, rollback,
product-specific settings, MiSTer support configuration, and map installation.
There is no separate Classic2USB updater or Legacy updater.

## Supported Products

- Classic2USB, JVSIO2USB and future RP2040 Adapt targets use hash-verified and
  structurally validated UF2 downloads. The Manager rejects non-RP2040 images,
  and images that overlap the preserved settings journal, EEPROM or
  auth/mapping flash.
- Each RP2040 image must be built for the product it is installed as: it must
  carry exactly one hardware target, and that target must be the product's
  own. A Classic2USB image is never written to a JVSIO2USB, or the reverse.
  The Manager knows each product's INFO name and target itself; a catalog only
  lists releases, and a product the Manager does not know is never installed.
- Adapt V1 / Legacy uses the 32u4 Caterina backend.
- Adapt V1 / Legacy requires an explicit choice between **MPG Firmware** and
  **Legacy Firmware**. Their shared bootloader identity cannot safely determine
  the hardware family.

| Product | Firmware source | MiSTer maps | Management |
|---------|-----------------|-------------|------------|
| Classic2USB | Verified Reflex Adapt GitHub release descriptors and UF2 assets | Ten official core-specific maps | Firmware, rollback, recovery, and serial settings |
| JVSIO2USB | Verified Reflex Adapt GitHub release descriptors and UF2 assets | None | Firmware, rollback, and recovery; settings are on the adapter's own menus |
| Adapt V1 / Legacy | Exact-tag, size, and SHA256-verified assets from `Reflex-Adapt-Legacy` | Complete 346-file Legacy set | MPG or Legacy firmware selection plus MiSTer USB support |

Classic2USB and JVSIO2USB firmware becomes installable when its GitHub release
includes the generated `adapt-manager-release.json` descriptor
(`generate_release_descriptor.py --product`). Both products publish to the same
release feed, so JVSIO2USB releases are tagged `jvsio2usb-vX.Y.Z` while
Classic2USB keeps `vX.Y.Z`. Until a release is published, the manager reports
that no release is available. Adapt V1 / Legacy firmware is already described by
the embedded catalog.

A JVSIO2USB exposes its management serial interface in MiSTer output mode;
console output modes such as XInput do not. BOOTSEL cannot identify an RP2040 board, so recovery firmware
first asks which product the board is. When the last verified installation at
the same USB path was a different product, the Manager asks again before
writing.

Firmware binaries are not copied from `Reflex-Adapt-Legacy`. The generated
catalog references sanctioned Git tags in that repository and records each
artifact's exact size and SHA256.

## MiSTer Installation

Add the contents of `reflex-adapt-manager.ini` to a MiSTer `*downloader.ini`
file, then run MiSTer Downloader. Launch `Scripts/Reflex_Adapt/reflex_adapt_manager.sh`
from MiSTer's Scripts menu.

Classic2USB development firmware also embeds the self-contained Manager on
its read-only USB drive. Run `sh /media/usb0/install_manager.sh` (use the
drive's actual mount) to copy it and the map scripts into
`/media/fat/Scripts/Reflex_Adapt`; a different installed script is backed up
first, the replacement is staged before rename, and a Manager left directly in
`Scripts` moves aside. Downloader deletes that old copy itself. You
can instead copy the installer into the Scripts menu while the drive is
connected, or copy the `Scripts/Reflex_Adapt` folder directly onto the SD card
from a PC. Install to the SD card before flashing because the adapter drive
disappears during BOOTSEL/reboot. Installation changes no firmware, maps or
Downloader configuration. Manager requires Python 3 on MiSTer and emits a
clear prerequisite error if it is missing. The embedded version is 1.1.0,
which adds JVSIO2USB; 1.0.0 was the first public release, alongside
Classic2USB 1.0.0. The optional Downloader entry follows the public version, so
a new Manager has to be published before the drive's copy is the newest
(`RELEASE_CHECKLIST.md`). Release descriptors keep `manager_min_version` 1.0.0,
because Manager 1.0.0 falls back to its cached catalog when any descriptor
fails validation.

`tools/generate_usb_drive_image.py` renders Manager directly from the template,
Python, catalog and map bundle, using the same renderer as the standalone
package. It does not depend on the generated release-assets script being fresh.

At startup, the manager disables the superseded `misteraddons/reflexadapt`
Downloader database entry when its section name and URL exactly match the
retired database. `misteraddons/reflex-adapt-legacy` remains the maintained
Adapt V1 / Legacy feed: the manager leaves it active and restores a section
that Manager 0.4.0 commented out, unless the same section is already active.
Other sections and modified URLs are left untouched, and changed files receive
a `.reflex-adapt-manager.bak` backup.

The manager scans attached USB hardware and checks the published firmware
catalog before displaying its menu. Use the arrow keys and Enter to navigate;
only actions supported by the attached Adapt hardware are shown.

Classic2USB exposes its management serial interface in MiSTer DInput and the
specialized MiSTer GunCon, neGcon, and JogCon output modes. Firmware-version
checks, automatic BOOTSEL entry, and settings management work in those modes.
Console output modes do not expose CDC and must be changed to DInput before
using the manager. A Classic2USB already in BOOTSEL remains available for
firmware recovery. BOOTSEL cannot identify the product, so recovery requires
the user to select and explicitly confirm the exact mount and USB path.
Runtime-triggered updates correlate the serial device, BOOTSEL block device,
and rebooted runtime through their USB topology.

Automatic updates and settings require an INFO response identifying Classic2USB;
USB IDs and controller-shaped names are shared and cannot prove the product.
Manager rechecks INFO on the selected serial port before requesting BOOTSEL and
requires the expected product, version and tag when verifying its return.
Every settings operation also validates INFO on the connection used for that
operation, so an old menu scan cannot authorize writes to another product.
Older/unidentified adapters require manual BOOTSEL and explicit target confirmation.

ASCII commands accept complete newline-terminated replies. INFO, STATUS and
STATE append PING and drain the ordered PONG before closing or reusing the
connection. This also handles a status extension whose final text joins PONG
without an intervening newline. Operations with explicit final OK replies use
those completion tokens.

The manager provides firmware installation, previous stable releases,
rollback, an offline download and catalog cache, install receipts, device discovery, and
Classic2USB serial settings. Its compact, checksummed map bundle contains the
official Classic2USB maps and the complete Adapt V1 / Legacy map set. Installing
missing maps preserves existing files. Restoring official maps is a separate
confirmed action that backs up changed files before replacement.

For Adapt V1 / Legacy it also configures MiSTer's 1 ms USB polling, composite
HID quirk, and `no_merge_vidpid` settings. Original MiSTer configuration files
receive a `.reflex-adapt-manager.bak` backup.

RP2040 receipts start as pending after the copy and become verified only after
the same USB device reboots and reports the selected version. If the output mode
does not expose management serial, history records the result as unverified
instead of claiming success. Adapt V1 flash is read back and verified while its
EEPROM is preserved during firmware installation. The manager warns
before a downgrade because older firmware may require an EEPROM reset. Classic2USB
uses the selected adapter's current INFO version; recovery can use a verified
receipt for the same USB path, identified as historical evidence. Unknown
versions and receipts from another USB path do not establish the installed version.

Classic2USB settings include raw input modes 20/21 for Intellivision and
ColecoVision, the global OK / Back layout, pointer and profile-aware rotary
controls, and semantic PlayStation Poll Rate / Command Gap editors. The latter
and the explicit stored Generic/JogCon rotary profile editors appear only when
firmware advertises their capability, so the Manager never treats the shared
packed PSX byte as an ordinary shoulder setting.

## Core Art

The **Core Art setup / updates** menu installs the embedded `pix4rflx.sh` helper
and builds artwork from existing `/media/fat/i2c2oled/` PIX files. The download
action updates Venice1200's collection and rebuilds Main's artwork cache.
Existing custom PRI artwork takes priority. The menu is available during
BOOTSEL recovery and while preparing an SD card with the adapter disconnected.

Main delivery uses the cache with the compatible `classic2usb_mister_serial`
candidate. Standalone delivery offers a one-shot display and startup controls.
Enabling standalone startup adds a managed block to `linux/user-startup.sh`
and starts the helper. Disabling it removes that block and stops matching
installed helper processes. Existing startup commands and helper configuration
are preserved, and changed files receive backups.

For automation, use `--core-art setup`, `update`, `show`, `enable` or `disable`.
These operations also work with `--offline`; only `update` downloads artwork.
The self-contained Manager and the firmware's USB drive carry the helper.
The public source export maps its canonical development source into
`tools/adapt_manager/pix4rflx.sh`.

## Maintainer Commands

```powershell
python tools/adapt_manager/generate_catalog.py
python tools/adapt_manager/generate_map_bundle.py
python tools/adapt_manager/render_manager.py
python tools/adapt_manager/generate_downloader_db.py
python tools/test_adapt_manager.py
```

Catalog and map-bundle generation require a sibling `Reflex-Adapt-Legacy`
checkout by default. Use `--legacy-repo` to select another checkout. MiSTer
Downloader installs only the manager script; map installation remains an
explicit manager action so Downloader cannot overwrite user mappings. The
Classic2USB map actions are Install missing, Restore official (backs up changed
files) and Remove all (moves every Classic2USB map, custom ones included, into
`config/inputs/classic2usb_removed_<date>`; same file set as the USB drive's
`remove_maps.sh`).

## System Organization

```text
reflex-adapt-manager.ini      common Downloader entry
          |
          v
MiSTer Downloader
  /media/fat/*downloader.ini
  misteraddons/reflex-adapt-manager
          |
          v
  /media/fat/Scripts/Reflex_Adapt/reflex_adapt_manager.sh
          |
          +-- embedded product catalog
          +-- embedded checksummed map bundle
          +-- RP2040 UF2 installer
          +-- Adapt V1 AVR109 installer
          +-- settings and MiSTer support configuration
          |
          +-- firmware cache, receipts, logs, and backups
              /media/fat/config/reflex-adapt-manager/
          |
          +-- explicitly installed maps
              /media/fat/config/inputs/
```

The repository sources are organized as follows:

| Path | Responsibility |
|------|----------------|
| `tools/adapt_manager/reflex_adapt_manager.py` | Manager behavior and product dispatch |
| `tools/adapt_manager/catalog.json` | Embedded product and firmware fallback catalog |
| `tools/adapt_manager/mister_maps.bin` | Generated compressed map bundle |
| `tools/adapt_manager/reflex-adapt-manager.ini` | Single MiSTer Downloader entry |
| `tools/release_assets/adapt-manager/` | Generated self-contained MiSTer script |
| `tools/release_assets/classic2usb/mister/config/inputs/` | Ten Classic2USB map sources and manual package copies |
| `tools/release_targets.json` | Classic2USB release-package composition |
| `Reflex-Adapt-Legacy/mister/config/inputs/` | Source archive for the 346 Legacy maps |

The generated manager shell embeds its Python implementation, fallback
catalog, and compressed map bundle. It remains one MiSTer script even though it
supports multiple firmware backends and product families.

## Adding Adapt Products

MODERN2USB, JVSIO2USB, and later products should extend this manager instead of
shipping additional updater scripts or Downloader databases. Each product adds:

1. A stable product ID, USB detection identity, display name, and installer
   backend in the catalog and manager dispatch.
2. A release descriptor containing versioned firmware URLs, sizes, and SHA256
   hashes.
3. A product-specific settings schema only when the device exposes settings.
4. A product namespace in the map bundle only when the product requires MiSTer
   maps.

RP2040 products can share the existing UF2 backend. Product-specific detection,
settings, and recovery behavior remain isolated behind their product IDs. This
keeps one user-facing manager while allowing each device to expose only its
relevant actions.
