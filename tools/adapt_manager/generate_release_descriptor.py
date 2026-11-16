#!/usr/bin/env python3
"""Generate the exact-hash catalog descriptor published with an Adapt release."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from firmware_version import resolve_version

# Every product releases into misteraddons/reflex-io, which Manager reads as one
# feed, so each product needs its own tag. Classic2USB kept the plain vX.Y.Z.
PRODUCTS = {
    "classic2usb": {
        "display_name": "Reflex Adapt Classic2USB",
        "family": "Classic2USB",
        "tag_prefix": "v",
        "header": "classic2usb.h",
    },
    "jvsio2usb": {
        "display_name": "Reflex Adapt JVSIO2USB",
        "family": "JVSIO2USB",
        "tag_prefix": "jvsio2usb-v",
        "header": "jvsio2usb.h",
    },
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def descriptor(uf2: Path, version: str, product_id: str = "classic2usb") -> dict[str, object]:
    product = PRODUCTS[product_id]
    family = product["family"]
    tag = f"{product['tag_prefix']}{version}"
    result = {
        "schema_version": 1,
        # Manager 1.0.0 drops to its cached catalog when any descriptor fails
        # validation, so a descriptor must never require a newer Manager.
        "manager_min_version": "1.0.0",
        "products": [{
            "id": product_id,
            "display_name": product["display_name"],
            "backend": "rp2040-uf2",
            "releases": [{
                "family": product_id,
                "version": version,
                "tag": tag,
                "channel": "stable",
                "recommended": True,
                "notes": f"{family} release firmware.",
                "eeprom_schema": 1,
                "hardware_compatibility": {
                    "policy": "all-published-revisions",
                    "product_family": family,
                    "mcu": "RP2040",
                    "flash": "2 MB",
                },
                "artifacts": [{
                    "id": product_id,
                    "display_name": family,
                    "description": f"{product['display_name']} {tag}",
                    "filename": uf2.name,
                    "url": (
                        "https://github.com/misteraddons/reflex-io/releases/download/"
                        f"{tag}/{uf2.name}"
                    ),
                    "bytes": uf2.stat().st_size,
                    "sha256": sha256(uf2),
                    "channel": "stable",
                }],
            }],
        }],
    }
    firmware_tag = f"v{version}"  # FIRMWARE_VERSION_TAG, which INFO reports
    if tag != firmware_tag:
        # Manager verifies the rebooted adapter's INFO TAG against this.
        result["products"][0]["releases"][0]["firmware_tag"] = firmware_tag
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--uf2", type=Path, required=True)
    parser.add_argument("--version", default="")
    parser.add_argument("--product", choices=sorted(PRODUCTS), default="classic2usb")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    version = args.version or resolve_version(
        ROOT / "firmware/firmware_build_info.h",
        ROOT / "firmware/config/products" / PRODUCTS[args.product]["header"])
    args.output.write_text(
        json.dumps(descriptor(args.uf2, version, args.product), indent=2) + "\n",
        encoding="utf-8")
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
