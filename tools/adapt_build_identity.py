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


def identity_build(project_dir):
    # Hash tracked + untracked application sources, not build outputs. One
    # constant is shared by every translation unit and both identity transports.
    files = subprocess.check_output(
        ["git", "ls-files", "-c", "-o", "--exclude-standard", "--", *IDENTITY_PATHS],
        cwd=project_dir, text=True).splitlines()
    digest = hashlib.sha256()
    for relative in sorted(set(files)):
        if relative.startswith("firmware/src/"):
            continue
        path = os.path.join(project_dir, relative)
        if not os.path.isfile(path):
            continue
        digest.update(relative.encode("utf-8") + b"\0")
        with open(path, "rb") as source:
            digest.update(source.read())
    return "src-" + digest.hexdigest()[:20]
