"""The source identity a firmware build embeds as ADAPT_IDENTITY_BUILD.

The build hook and the public export share this, so the export can prove a UF2
came from the tree it is about to publish.
"""

import hashlib
import os
import subprocess


IDENTITY_PATHS = (
    "firmware", "platformio.ini", "tools/pio_hooks.py", "tools/adapt_build_identity.py",
    "tools/usb2db15_test_log_layout.py", "tools/generate_usb_drive_image.py",
    "tools/generate_psx_capture_drive.py",
    "tools/generate_genesis_capture_drive.py",
    "tools/generate_jogcon_capture_drive.py",
    "tools/generate_n64_capture_drive.py",
    "tools/generate_host_capture_drive.py",
    "tools/n64_capture_layout.py",
    "tools/adapt_manager", "mister/pix4rflx.sh", "tools/release_assets/classic2usb/mister/config/inputs",
    "third_party/licenses/GIMX-firmwares-GPL-3.0-LICENSE", "CHANGELOG.md",
)

# The vendored libraries are build inputs too, so a change to one (the PIO-USB
# receive bound, 2026-11-11) changes the ID. Under firmware/src/RetroZord only
# platformio.ini's lib_dir is built; the other donor snapshots and zips there
# are not (test_export_public_release.py checks this against lib_dir).
BUILT_RETROZORD = "firmware/src/RetroZord/PicoZordXinputDEV-main-20260418/"


def hashed_path(relative):
    if relative.startswith("firmware/src/RetroZord/"):
        return relative.startswith(BUILT_RETROZORD)
    return True


def identity_bytes(data):
    # A checkout with core.autocrlf=true (Windows) has CRLF where an LF checkout
    # has LF, so hashing raw bytes gave one commit two BUILD IDs and two images
    # (2026-11-11, d62a3b6f). Git stores text as LF, and binary files are the
    # same bytes in every checkout, so hashing with CRLF folded to LF is the
    # same for every checkout of a commit.
    return data.replace(b"\r\n", b"\n")


def identity_build(project_dir):
    # Hash tracked + untracked application sources, not build outputs. One
    # constant is shared by every translation unit and both identity transports.
    files = subprocess.check_output(
        ["git", "ls-files", "-c", "-o", "--exclude-standard", "--", *IDENTITY_PATHS],
        cwd=project_dir, text=True).splitlines()
    digest = hashlib.sha256()
    hashed = 0
    for relative in sorted(set(files)):
        if not hashed_path(relative):
            continue
        path = os.path.join(project_dir, relative)
        if not os.path.isfile(path):
            continue
        digest.update(relative.encode("utf-8") + b"\0")
        with open(path, "rb") as source:
            digest.update(identity_bytes(source.read()))
        hashed += 1
    # git lists nothing for a tree inside another checkout's ignored folder (a
    # `git archive` export under build/), and hashing nothing gives the same
    # src-e3b0c44298fc1c149afb for every such build.
    if not hashed:
        raise RuntimeError(
            f"{project_dir}: git lists none of the build identity sources, so the "
            "build ID would name no source. Build in a checkout or a git clone.")
    return "src-" + digest.hexdigest()[:20]
