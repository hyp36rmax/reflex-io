Reflex Adapt Classic2USB - USB drive
====================================

This read-only drive comes from the adapter itself.

  Classic2USB Manual.html   The Classic2USB guide from docs.misteraddons.com,
                            without pictures. Open it in any browser. The
                            online guide is newer when the two differ.
  Changelog.txt             What changed in each firmware version.
  config/inputs/           MiSTer controller maps for the input modes that
                            MiSTer cannot map on its own (N64, PlayStation,
                            Saturn, Jaguar, Virtual Boy, ...). The folder
                            matches the MiSTer SD card.
  install_manager.sh        Installs the scripts below into the MiSTer
                            Scripts menu.
  Scripts/Reflex_Adapt/     The MiSTer scripts, in the SD card's own layout:
    install_maps.sh         Copies those maps to /media/fat/config/inputs on
                            a MiSTer, keeping a .bak of any map you changed.
    remove_maps.sh          Moves every Classic2USB map (yours and the ones
                            above) out of /media/fat/config/inputs into a
                            dated folder there, so MiSTer uses its defaults.
    reflex_adapt_manager.sh Self-contained MiSTer Adapt Manager: settings,
                            maps and verified published firmware updates.
  reflex-adapt-manager.ini   Optional MiSTer Downloader entry for Manager
                            updates. Add it to your *downloader.ini file.
  GPL-3.0.txt               License for the Manager source embedded in its
                            script (GPL-3.0-or-later).

Installing Adapt Manager on a MiSTer
-----------------------------------
Run   sh /media/usb0/install_manager.sh   over SSH (use this drive's actual
usb number), or copy install_manager.sh to /media/fat/Scripts and run it
from the Scripts menu while this drive is connected. It copies Manager to
/media/fat/Scripts/Reflex_Adapt with the map scripts, backs up any different
existing script, moves an older Manager out of /media/fat/Scripts, and
installs nothing else. Launch reflex_adapt_manager from Reflex_Adapt in
MiSTer's Scripts menu.

Install Manager on the SD card before updating firmware: the adapter's
drive disappears during BOOTSEL/reboot. Manager needs MiSTer's Python 3;
this is not a Windows application. Windows can copy the files to the SD
card instead. It includes an offline catalog and maps, but downloading a
new firmware needs Internet unless already cached. Classic2USB firmware
updates need a published release descriptor and UF2; this drive does not
invent releases or flash firmware automatically. Console output modes
(including PS3) have no drive or management serial; use DInput or a MiSTer
mode for Manager.

The bundled script contains its Python source, catalog and map bundle.
The offline manual records its documentation source and generation date.

Installing the maps on a MiSTer
-------------------------------
Either run   sh /media/usb0/Scripts/Reflex_Adapt/install_maps.sh   over SSH
(the number after "usb" depends on what else is plugged in), or run it from
Reflex_Adapt in the Scripts menu after installing the scripts as above, while
this drive is connected. You can also copy the config folder to the root of
the SD card.

Clearing your Classic2USB maps
------------------------------
remove_maps.sh sets aside every map MiSTer keeps for the Classic2USB (USB
ID 16d0:1460), including ones you made with Define joystick buttons. Run it
the same way as install_maps.sh; it does not need this drive. Nothing is
deleted: the maps go to config/inputs/classic2usb_removed_<date>, and moving
them back undoes it. Run install_maps.sh afterwards to start again from the
Classic2USB maps. Reload the core either way.

Turning the drive off
---------------------
The drive is on by default. Switch it off in System Settings > USB Drive;
saving a change reconnects the adapter. Turn it off if your own USB storage
on a MiSTer moves: some setups number USB drives by plug order, and an extra
drive can change which number your own USB storage gets.
