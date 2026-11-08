#!/bin/sh
# Install the drive's MiSTer scripts into Scripts/Reflex_Adapt on the SD card:
# Adapt Manager and the map scripts.
set -eu

SCRIPTS="${REFLEX_ADAPT_SCRIPTS_DIR:-/media/fat/Scripts}"
DEST="$SCRIPTS/Reflex_Adapt"
NAME=reflex_adapt_manager.sh
MARKER="Classic2USB Manual.html"
here=$(cd "$(dirname "$0")" && pwd)
source_dir=""
for dir in "$here" /media/usb*; do
  if [ -f "$dir/$MARKER" ] && [ -f "$dir/Scripts/Reflex_Adapt/$NAME" ]; then
    source_dir="$dir/Scripts/Reflex_Adapt"
    break
  fi
done
if [ -z "$source_dir" ]; then
  echo "Classic2USB drive not found."
  echo "Set Output to DInput or a MiSTer mode and System Settings > USB Drive to On."
  echo "Save, reconnect, then run this again."
  exit 1
fi

mkdir -p "$DEST"
temp_file=""
cleanup() { [ -z "$temp_file" ] || rm -f "$temp_file"; }
trap cleanup EXIT
trap 'exit 1' INT TERM

changed=0
for source_file in "$source_dir"/*.sh; do
  name=$(basename "$source_file")
  target="$DEST/$name"
  if [ -f "$target" ] && cmp -s "$source_file" "$target"; then
    continue
  fi
  # Stage on the SD card so interruption cannot leave a partial installed script.
  temp_file=$(mktemp "$DEST/.$name.XXXXXX")
  cp "$source_file" "$temp_file"
  chmod 755 "$temp_file"
  if [ -e "$target" ]; then
    backup=$(mktemp "$DEST/$name.bak.XXXXXX")
    cp "$target" "$backup"
    echo "Previous $name backed up to $backup"
  fi
  mv -f "$temp_file" "$target"
  temp_file=""
  changed=$((changed + 1))
  echo "Installed $target"
done

# Manager used to sit directly in Scripts, and a copy left there would go stale.
old="$SCRIPTS/$NAME"
if [ -f "$old" ]; then
  backup=$(mktemp "$DEST/$NAME.old.XXXXXX")
  mv -f "$old" "$backup"
  changed=$((changed + 1))
  echo "Moved the old Manager out of Scripts to $backup"
fi

sync
if [ "$changed" -eq 0 ]; then
  echo "The Reflex Adapt scripts are already current."
fi
echo "Launch reflex_adapt_manager from Reflex_Adapt in MiSTer's Scripts menu."
echo "No firmware, maps or Downloader configuration were changed."
