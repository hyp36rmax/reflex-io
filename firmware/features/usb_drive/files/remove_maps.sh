#!/bin/sh
# Reflex Adapt Classic2USB: set aside its MiSTer controller maps.
#
# Moves every map MiSTer keeps for the Classic2USB (USB ID 16d0:1460) out of
# /media/fat/config/inputs into a dated folder there, so MiSTer goes back to
# its default mapping for the adapter. That covers maps you made in MiSTer's
# Define joystick buttons and the ones install_maps.sh added. Maps for other
# controllers stay. Nothing is deleted: move the files back to undo, or run
# install_maps.sh to reinstall the Classic2USB maps.
#
# The adapter's USB drive does not need to be on. Run it over SSH
# (sh /media/usb0/Scripts/Reflex_Adapt/remove_maps.sh) or run it from
# Reflex_Adapt in the Scripts menu. Reload the core afterwards.

DIR=/media/fat/config/inputs

# MiSTer names a Classic2USB map after the adapter's USB serial, which is the
# input mode (ReflexN64, ReflexSat, ...), with spaces as underscores.
NAMES="ReflexClassic2USB Adapt_Classic2USB
ReflexN64 ReflexGC ReflexNES ReflexSNES ReflexVboy ReflexMD ReflexSat ReflexSat3D
ReflexWii ReflexPCE ReflexPSDS1 ReflexPSDS2 ReflexNeoGeo Reflex3DO ReflexJag
ReflexDreamcast ReflexDriving ReflexSMS ReflexJPC ReflexIntv ReflexColeco ReflexCDi
ReflexPSGun ReflexPSWheel ReflexPSJogCon MiSTer-A1_Paddle MiSTer-A1_Spinner
ReflexMouse
ReflexRemote"

if [ ! -d "$DIR" ]; then
  echo "No $DIR, so there are no maps to remove."
  exit 0
fi

dest="$DIR/classic2usb_removed_$(date +%Y%m%d_%H%M%S)"
moved=0
move() {
  for file in "$@"; do
    [ -f "$file" ] || continue
    mkdir -p "$dest" || exit 1
    mv -f "$file" "$dest/" || exit 1
    moved=$((moved + 1))
  done
}

for name in $NAMES; do
  # Core and menu maps, modifier (_m), unique (_<hash>) and joystick-to-
  # keyboard (_jk) maps, keyboard maps and install_maps.sh's .bak copies.
  move "$DIR"/*16d0_1460_"$name"_* "$DIR"/kbd_16d0_1460_"$name".map*
done
# MiSTer Main before May 2026 left the serial out of the name.
move "$DIR"/*input_16d0_1460_v3.map* "$DIR"/*input_16d0_1460_m_v3.map* \
     "$DIR"/*input_16d0_1460_jk.map* "$DIR"/kbd_16d0_1460.map*
sync

if [ "$moved" -eq 0 ]; then
  echo "No Classic2USB maps in $DIR."
  exit 0
fi
echo "Moved $moved Classic2USB map files to"
echo "  $dest"
echo "Reload the core to use MiSTer's default mapping. To undo, move them back."
