#!/usr/bin/env python3
"""Generate the Classic2USB USB drive: a read-only FAT12 image as a C array.

The drive holds the offline manual, the changelog, the MiSTer controller maps
in the SD card's own layout (config/inputs), Adapt Manager and install
scripts. The image is
written to firmware/features/usb_drive/generated/usb_drive_image.inc, which
is not tracked; tools/pio_hooks.py runs this before every build and the file
is rewritten only when its contents change.
"""
import argparse
import struct
import sys
from pathlib import Path

from adapt_manager.render_manager import SOURCE_DIR as MANAGER_DIR, render_text

ROOT = Path(__file__).resolve().parents[1]
FILES_DIR = ROOT / "firmware/features/usb_drive/files"
MAPS_DIR = ROOT / "tools/release_assets/classic2usb/mister/config/inputs"
OUTPUT = ROOT / "firmware/features/usb_drive/generated/usb_drive_image.inc"

SECTOR = 512
ROOT_ENTRIES = 64
FAT_COUNT = 2
VOLUME_LABEL = b"CLASSIC2USB"
VOLUME_ID = 0x0C2B2026
# A fixed timestamp keeps the image reproducible: 2026-10-03 12:00:00.
FAT_DATE = ((2026 - 1980) << 9) | (10 << 5) | 3
FAT_TIME = 12 << 11
ATTR_DIRECTORY = 0x10
ATTR_ARCHIVE = 0x20
ATTR_VOLUME = 0x08
ATTR_LFN = 0x0F
SHORT_CHARS = set(b"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!#$%&'()-@^_`{}~")


def drive_files():
    """(path on the drive, contents) in the order they are laid out."""
    files = [
        ("README.txt", (FILES_DIR / "README.txt").read_bytes()),
        ("Classic2USB Manual.html", (FILES_DIR / "Classic2USB Manual.html").read_bytes()),
        ("Changelog.txt", (ROOT / "CHANGELOG.md").read_bytes()),
        ("install_manager.sh", (FILES_DIR / "install_manager.sh").read_bytes()),
        ("Scripts/Reflex_Adapt/install_maps.sh", (FILES_DIR / "install_maps.sh").read_bytes()),
        ("Scripts/Reflex_Adapt/remove_maps.sh", (FILES_DIR / "remove_maps.sh").read_bytes()),
        ("Scripts/Reflex_Adapt/reflex_adapt_manager.sh", render_text(
            MANAGER_DIR / "reflex_adapt_manager.sh.in",
            MANAGER_DIR / "reflex_adapt_manager.py",
            MANAGER_DIR / "catalog.json",
        ).encode("utf-8")),
        ("reflex-adapt-manager.ini", (MANAGER_DIR / "reflex-adapt-manager.ini").read_text(
            encoding="utf-8").encode("utf-8")),
        ("GPL-3.0.txt", (ROOT / "third_party/licenses/GIMX-firmwares-GPL-3.0-LICENSE").read_text(
            encoding="utf-8").encode("utf-8")),
    ]
    maps = sorted(MAPS_DIR.glob("*.map"))
    if not maps:
        raise SystemExit(f"no controller maps in {MAPS_DIR}")
    files += [(f"config/inputs/{path.name}", path.read_bytes()) for path in maps]
    return files


def clean_short(text: str) -> bytes:
    return bytes(c if c in SHORT_CHARS else ord("_")
                 for c in text.upper().encode("ascii", "replace") if c != ord(" "))


def short_name(long_name: str, taken: set) -> bytes:
    """An 8.3 alias, unique in its directory, in the Windows ~N style. Every
    entry also carries its long name, so the alias is never what users see."""
    stem, dot, ext = long_name.rpartition(".")
    if not dot or not stem:
        stem, ext = long_name, ""
    base, extension = clean_short(stem) or b"_", clean_short(ext)[:3]
    for number in range(1, 100):
        tail = f"~{number}".encode()
        candidate = (base[: 8 - len(tail)] + tail).ljust(8) + extension.ljust(3)
        if candidate not in taken:
            taken.add(candidate)
            return candidate
    raise SystemExit(f"cannot make a short name for {long_name}")


def lfn_checksum(name: bytes) -> int:
    total = 0
    for byte in name:
        total = (((total & 1) << 7) | (total >> 1)) + byte
        total &= 0xFF
    return total


def lfn_entries(long_name: str, short: bytes) -> list:
    """Long-name entries in on-disk order (last part first, flagged 0x40)."""
    raw = long_name.encode("utf-16-le")
    chars = [raw[i:i + 2] for i in range(0, len(raw), 2)]
    if len(chars) % 13:
        chars.append(b"\x00\x00")
    while len(chars) % 13:
        chars.append(b"\xff\xff")
    checksum = lfn_checksum(short)
    pieces = [chars[i:i + 13] for i in range(0, len(chars), 13)]
    entries = []
    for index, piece in enumerate(pieces, start=1):
        order = index | (0x40 if index == len(pieces) else 0)
        entry = (bytes([order]) + b"".join(piece[0:5]) + bytes([ATTR_LFN, 0, checksum]) +
                 b"".join(piece[5:11]) + b"\x00\x00" + b"".join(piece[11:13]))
        entries.append(entry)
    return list(reversed(entries))


def dir_entry(name: bytes, attr: int, cluster: int, size: int) -> bytes:
    return struct.pack("<11sBBBHHHHHHHI", name, attr, 0, 0, FAT_TIME, FAT_DATE, FAT_DATE,
                       0, FAT_TIME, FAT_DATE, cluster, size)


def build_image(files) -> bytes:
    # Directory tree: path -> list of (name, child path or file data).
    dirs = {"": []}
    for path, data in files:
        parts = path.split("/")
        for depth in range(1, len(parts)):
            parent, child = "/".join(parts[:depth - 1]), "/".join(parts[:depth])
            if child not in dirs:
                dirs[child] = []
                dirs[parent].append((parts[depth - 1], child))
        dirs["/".join(parts[:-1])].append((parts[-1], data))

    def entries_needed(directory):
        count = 0 if directory == "" else 2
        for name, _ in dirs[directory]:
            count += 1 + (len(name) + 13) // 13  # short entry plus long-name entries
        return count

    # Allocate clusters: subdirectories first, then files, each contiguous.
    next_cluster = 2
    first_cluster, cluster_counts = {}, {}
    for directory in dirs:
        if directory:
            count = -(-entries_needed(directory) * 32 // SECTOR)
            first_cluster[directory], cluster_counts[directory] = next_cluster, count
            next_cluster += count
    file_clusters = {}
    for path, data in files:
        count = max(1, -(-len(data) // SECTOR)) if data else 0
        file_clusters[path] = (next_cluster if count else 0, count)
        next_cluster += count
    clusters = next_cluster - 2
    if clusters >= 4085:
        raise SystemExit(f"{clusters} clusters do not fit FAT12")
    fat_sectors = -(-((clusters + 2) * 3 // 2 + 1) // SECTOR)
    root_sectors = ROOT_ENTRIES * 32 // SECTOR
    data_start = 1 + FAT_COUNT * fat_sectors + root_sectors
    total_sectors = data_start + clusters
    if total_sectors > 0xFFFF:
        raise SystemExit("image too large for a 16-bit sector count")

    image = bytearray(total_sectors * SECTOR)
    boot = struct.pack("<3s8sHBHBHHBHHHII", b"\xEB\x3C\x90", b"REFLEX  ", SECTOR, 1, 1, FAT_COUNT,
                       ROOT_ENTRIES, total_sectors, 0xF8, fat_sectors, 1, 1, 0, 0)
    boot += struct.pack("<BBBI11s8s", 0x80, 0, 0x29, VOLUME_ID, VOLUME_LABEL.ljust(11), b"FAT12   ")
    image[0:len(boot)] = boot
    image[510:512] = b"\x55\xAA"

    fat = bytearray(fat_sectors * SECTOR)

    def set_fat(cluster, value):
        offset = cluster * 3 // 2
        if cluster & 1:
            fat[offset] = (fat[offset] & 0x0F) | ((value << 4) & 0xF0)
            fat[offset + 1] = (value >> 4) & 0xFF
        else:
            fat[offset] = value & 0xFF
            fat[offset + 1] = (fat[offset + 1] & 0xF0) | ((value >> 8) & 0x0F)

    set_fat(0, 0xFF8)
    set_fat(1, 0xFFF)

    def chain(start, count):
        for index in range(count):
            set_fat(start + index, 0xFFF if index == count - 1 else start + index + 1)

    for directory in first_cluster:
        chain(first_cluster[directory], cluster_counts[directory])
    for start, count in file_clusters.values():
        if count:
            chain(start, count)
    for copy in range(FAT_COUNT):
        offset = (1 + copy * fat_sectors) * SECTOR
        image[offset:offset + len(fat)] = fat

    def directory_bytes(directory):
        taken, out = set(), bytearray()
        if directory == "":
            out += dir_entry(VOLUME_LABEL.ljust(11), ATTR_VOLUME, 0, 0)
        else:
            parent = directory.rpartition("/")[0]
            out += dir_entry(b".          ", ATTR_DIRECTORY, first_cluster[directory], 0)
            out += dir_entry(b"..         ", ATTR_DIRECTORY, first_cluster.get(parent, 0), 0)
        for name, item in dirs[directory]:
            short = short_name(name, taken)
            for entry in lfn_entries(name, short):
                out += entry
            if isinstance(item, str):
                out += dir_entry(short, ATTR_DIRECTORY, first_cluster[item], 0)
            else:
                path = (directory + "/" if directory else "") + name
                start, _ = file_clusters[path]
                out += dir_entry(short, ATTR_ARCHIVE, start, len(item))
        return out

    root = directory_bytes("")
    if len(root) > ROOT_ENTRIES * 32:
        raise SystemExit("root directory overflow")
    root_offset = (1 + FAT_COUNT * fat_sectors) * SECTOR
    image[root_offset:root_offset + len(root)] = root

    def cluster_offset(cluster):
        return (data_start + cluster - 2) * SECTOR

    for directory in first_cluster:
        content = directory_bytes(directory)
        if len(content) > cluster_counts[directory] * SECTOR:
            raise SystemExit(f"directory {directory} overflow")
        offset = cluster_offset(first_cluster[directory])
        image[offset:offset + len(content)] = content
    for path, data in files:
        start, count = file_clusters[path]
        if count:
            offset = cluster_offset(start)
            image[offset:offset + len(data)] = data
    return bytes(image)


def render(image: bytes) -> str:
    lines = [
        "// Generated by tools/generate_usb_drive_image.py from firmware/features/usb_drive/files",
        "// and Adapt Manager sources / Classic2USB MiSTer maps. Do not edit.",
        f"constexpr uint32_t kUsbDriveSectorCount = {len(image) // SECTOR}u;",
        "alignas(4) const uint8_t kUsbDriveImage[] = {",
    ]
    for offset in range(0, len(image), 24):
        lines.append("".join(f"0x{byte:02x}," for byte in image[offset:offset + 24]))
    lines.append("};")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    args = parser.parse_args()
    text = render(build_image(drive_files()))
    if args.output.exists() and args.output.read_text(encoding="utf-8") == text:
        return 0
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(text, encoding="utf-8", newline="\n")
    print(f"generated {args.output.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
