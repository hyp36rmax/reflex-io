"""Build and link the project-owned MODERN2USB RP2040 stage3-v2."""

from __future__ import annotations

import hashlib
import os
import pathlib
import re
import subprocess
import sys
import tempfile

Import("env")  # type: ignore[name-defined]  # noqa: F821


def env_has_define(name: str) -> bool:
    # CPPDEFINES still contains inherited -D flags removed by a child
    # environment's build_unflags. Honor the removal before enabling stage3.
    for flag in env.get("BUILD_UNFLAGS", []):
        if flag == f"-D{name}" or flag.startswith(f"-D{name}="):
            return False
    for define in env.get("CPPDEFINES", []):
        if define == name or (
            isinstance(define, (tuple, list)) and define and define[0] == name
        ):
            return True
    return False


if env_has_define("ENABLE_ESP32_RP2040_UPDATE"):
    project_root = pathlib.Path(env.subst("$PROJECT_DIR")).resolve()
    build_root = pathlib.Path(env.subst("$BUILD_DIR")).resolve()
    source_root = project_root / "firmware/targets/modern2usb/rp2040/stage3"
    stage3_build = build_root / "rp2040_stage3_v2"
    # Ninja/CMake cannot create the SDK's deepest object files once a Windows
    # worktree makes this path approach MAX_PATH. Keep the main PlatformIO
    # output in place, but relocate only the reproducible stage-3 sub-build.
    if os.name == "nt" and len(str(stage3_build)) > 150:
        worktree_key = hashlib.sha256(
            str(project_root).encode("utf-8")
        ).hexdigest()[:12]
        stage3_build = (
            pathlib.Path(tempfile.gettempdir())
            / "reflex-adapt-stage3"
            / worktree_key
            / env.subst("$PIOENV")
        )

    platform = env.PioPlatform()
    framework_dir = pathlib.Path(
        platform.get_package_dir("framework-arduinopico")
    ).resolve()
    # CMake and Ninja live in package folders shared with every project here,
    # and the pioarduino ESP32 platforms install their own copies there. Use
    # any installed copy that works; install only when none does
    # (tools/stage3_build_tools.py explains why nothing is pinned).
    tools_dir = str(project_root / "tools")
    if tools_dir not in sys.path:
        sys.path.insert(0, tools_dir)
    from stage3_build_tools import resolve_platform_tool

    core_dir = pathlib.Path(
        os.environ.get("PLATFORMIO_CORE_DIR", pathlib.Path.home() / ".platformio")
    )
    cmake, cmake_version, cmake_installed = resolve_platform_tool(
        platform, "tool-cmake", core_dir
    )
    ninja, ninja_version, ninja_installed = resolve_platform_tool(
        platform, "tool-ninja", core_dir
    )
    print(
        f"stage3-v2: CMake {cmake_version} ({cmake})"
        f"{' installed' if cmake_installed else ''}, "
        f"Ninja {ninja_version} ({ninja}){' installed' if ninja_installed else ''}"
    )
    toolchain_dir = pathlib.Path(
        platform.get_package_dir("toolchain-rp2040-earlephilhower")
    ).resolve()
    toolchain_bin = toolchain_dir / "bin"
    command_env = os.environ.copy()
    command_env["PATH"] = os.pathsep.join(
        (str(toolchain_bin), str(ninja.parent), command_env.get("PATH", ""))
    )

    configure = subprocess.run(
        [
            str(cmake),
            "-S", str(source_root),
            "-B", str(stage3_build),
            "-G", "Ninja",
            f"-DCMAKE_MAKE_PROGRAM={ninja.as_posix()}",
            f"-DREFLEX_FRAMEWORK_DIR={framework_dir.as_posix()}",
            f"-DREFLEX_PROJECT_ROOT={project_root.as_posix()}",
        ],
        cwd=project_root,
        env=command_env,
    )
    if configure.returncode != 0:
        raise RuntimeError(
            f"stage3-v2 CMake configure failed with {configure.returncode}"
        )
    build = subprocess.run(
        [
            str(cmake), "--build", str(stage3_build),
            "--target", "reflex_stage3_blob",
        ],
        cwd=project_root,
        env=command_env,
    )
    if build.returncode != 0:
        raise RuntimeError(f"stage3-v2 build failed with {build.returncode}")

    stage3_bin = stage3_build / "reflex_modern2usb_stage3_v2.bin"
    stage3_obj = stage3_build / "reflex_modern2usb_stage3_v2.o"
    expected_blob_hash_path = source_root / "stage3_blob.sha256"
    # Main firmware places boot2 at 0x0000..0x00FF and partition words at
    # 0x2FF0. The .OTA blob must fit strictly between them.
    maximum_blob_size = 0x2FF0 - 0x0100
    stage3_size = stage3_bin.stat().st_size
    if stage3_size > maximum_blob_size:
        raise RuntimeError(
            f"stage3-v2 is {stage3_size} bytes; maximum is {maximum_blob_size}"
        )
    expected_blob_hash = expected_blob_hash_path.read_text(
        encoding="ascii"
    ).strip().lower()
    if not re.fullmatch(r"[0-9a-f]{64}", expected_blob_hash):
        raise RuntimeError(
            f"invalid stage3 blob hash contract: {expected_blob_hash_path}"
        )
    actual_blob_hash = hashlib.sha256(stage3_bin.read_bytes()).hexdigest()
    if actual_blob_hash != expected_blob_hash:
        raise RuntimeError(
            "stage3-v2 blob changed without review: "
            f"{actual_blob_hash} != {expected_blob_hash}"
        )

    libraries = list(env.get("LIBS", []))
    replacement_count = 0
    replacement = env.File(str(stage3_obj))
    for index, library in enumerate(libraries):
        normalized = str(library).replace("\\", "/").lower()
        if normalized.endswith("/lib/rp2040/ota.o"):
            libraries[index] = replacement
            replacement_count += 1
    if replacement_count != 1:
        raise RuntimeError(
            "expected exactly one Arduino-Pico rp2040/ota.o linker input, "
            f"found {replacement_count}"
        )
    env.Replace(LIBS=libraries)

    def cap_linker_app_region(target, source, env):
        linker_path = build_root / "memmap_default.ld"
        linker = linker_path.read_text(encoding="utf-8")
        linker, flash_count = re.subn(
            r"(FLASH\(rx\)\s*:\s*ORIGIN\s*=\s*0x10000000\s*,\s*LENGTH\s*=\s*)\d+",
            r"\g<1>655360",
            linker,
            count=1,
        )
        linker, partition_count = re.subn(
            r"(LONG\()\d+(\)\s*\n\s*\}\s*>\s*FLASH)",
            r"\g<1>655360\g<2>",
            linker,
            count=1,
        )
        if flash_count != 1 or partition_count != 1:
            raise RuntimeError(
                "could not cap MODERN2USB linker app region at 0x0A0000"
            )
        linker_path.write_text(linker, encoding="utf-8", newline="\n")

    env.AddPreAction(
        "$BUILD_DIR/${PROGNAME}.elf",
        env.VerboseAction(
            cap_linker_app_region,
            "Capping MODERN2USB linker app region at 0x0A0000",
        ),
    )

    print(
        f"MODERN2USB stage3-v2: {stage3_size}/{maximum_blob_size} bytes; "
        f"sha256={actual_blob_hash}; app linker capped at 0x0A0000"
    )
