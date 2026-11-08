#!/bin/sh
# Reflex Adapt Classic2USB: install its MiSTer controller maps.
#
# Copies config/inputs/*.map from the adapter's USB drive to the MiSTer SD
# card (/media/fat/config/inputs). A map you already have with different
# contents is kept as <name>.bak first. Run it from the drive over SSH:
#   sh /media/usb0/Scripts/Reflex_Adapt/install_maps.sh
# or run it from Reflex_Adapt in the Scripts menu, once install_manager.sh
# has put it there, while the adapter's USB drive is on and connected.

DEST=/media/fat/config/inputs
MARKER="Classic2USB Manual.html"

src=""
here=$(cd "$(dirname "$0")" 2>/dev/null && pwd)
for dir in "$here/../.." /media/usb*; do
  if [ -f "$dir/$MARKER" ] && [ -d "$dir/config/inputs" ]; then
    src="$dir/config/inputs"
    break
  fi
done
if [ -z "$src" ]; then
  echo "Classic2USB drive not found."
  echo "On the adapter, set System Settings > USB Drive to On and save,"
  echo "connect it to the MiSTer, then run this again."
  exit 1
fi

mkdir -p "$DEST" || exit 1
copied=0
current=0
for map in "$src"/*.map; do
  [ -f "$map" ] || continue
  name=$(basename "$map")
  if [ -f "$DEST/$name" ]; then
    if cmp -s "$map" "$DEST/$name"; then
      current=$((current + 1))
      continue
    fi
    cp -f "$DEST/$name" "$DEST/$name.bak" || exit 1
  fi
  cp -f "$map" "$DEST/$name" || exit 1
  copied=$((copied + 1))
done
sync
echo "Classic2USB maps from $src"
echo "Installed $copied, already current $current. Reload the core to use them."
