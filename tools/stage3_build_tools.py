"""Find the CMake and Ninja that the MODERN2USB stage-3 sub-build runs.

PlatformIO keeps one package directory per tool name (`tool-cmake`,
`tool-ninja`) for every project on the machine. The pioarduino ESP32 platforms
install their own CMake and Ninja there from their own URLs, and the RP2040
platform's package spec (owner `platformio`) never matches those copies. Asking
PlatformIO for its own copy then replaces the installed one; when another
build is using it, the swap half-finishes and leaves both builds broken. Stage
3 therefore takes any installed copy that runs and meets the RP2040 platform's
version requirement, and installs only when none does. The stage-3 blob hash
check still fails the build if a different tool version changes the output.
"""

from __future__ import annotations

import os
import pathlib
import re
import subprocess
from typing import Callable, Iterable, Optional

EXE = ".exe" if os.name == "nt" else ""
EXECUTABLES = {"tool-cmake": "bin/cmake" + EXE, "tool-ninja": "ninja" + EXE}


def tool_version(executable: pathlib.Path, run=subprocess.run) -> Optional[str]:
    try:
        result = run([str(executable), "--version"], capture_output=True,
                     text=True, timeout=60)
    except (OSError, subprocess.SubprocessError):
        return None
    if result.returncode != 0:
        return None
    match = re.search(r"\d+\.\d+(?:\.\d+)?", result.stdout)
    return match.group(0) if match else None


def usable(name: str, root: pathlib.Path, accepts: Callable[[str], bool],
           run=subprocess.run) -> Optional[tuple[pathlib.Path, str]]:
    executable = root / EXECUTABLES[name]
    if not executable.is_file():
        return None
    version = tool_version(executable, run)
    if version is None or not accepts(version):
        return None
    if name == "tool-cmake":
        # CMake still prints its version after an interrupted reinstall has
        # deleted its modules, then fails to configure ("Could not find
        # CMAKE_ROOT"). Require the module tree for this version.
        major_minor = ".".join(version.split(".")[:2])
        if not (root / "share" / f"cmake-{major_minor}").is_dir():
            return None
    return executable, version


def resolve(name: str, candidates: Iterable[Optional[os.PathLike]],
            accepts: Callable[[str], bool],
            install: Callable[[], Optional[os.PathLike]],
            run=subprocess.run) -> tuple[pathlib.Path, str, bool]:
    """Return (executable, version, installed) for the first usable copy."""
    checked: list[pathlib.Path] = []
    for candidate in candidates:
        if candidate is None:
            continue
        root = pathlib.Path(candidate)
        if root in checked:
            continue
        checked.append(root)
        found = usable(name, root, accepts, run)
        if found:
            return found[0], found[1], False
    installed = install()
    found = usable(name, pathlib.Path(installed), accepts, run) if installed else None
    if found:
        return found[0], found[1], True
    raise RuntimeError(
        f"stage3-v2 found no usable {name} in "
        f"{', '.join(str(p) for p in checked) or 'no candidate'}, "
        "and installing the RP2040 platform's copy did not provide one")


def resolve_platform_tool(platform, name: str, core_dir: os.PathLike,
                          run=subprocess.run) -> tuple[pathlib.Path, str, bool]:
    """Resolve `name` for a PlatformIO platform without replacing other copies."""
    import semantic_version  # PlatformIO's own dependency

    requirements = platform.get_package_spec(name).requirements

    def accepts(version: str) -> bool:
        return requirements is None or requirements.match(
            semantic_version.Version.coerce(version))

    any_owner = platform.pm.get_package(name)  # matches by name, any owner
    candidates = [
        platform.get_package_dir(name),
        any_owner.path if any_owner else None,
        pathlib.Path(core_dir) / "packages" / name,
    ]

    def install():
        package = platform.install_package(name)
        return package.path if package else None

    return resolve(name, candidates, accepts, install, run)
