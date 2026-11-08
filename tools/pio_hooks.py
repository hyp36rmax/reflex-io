Import("env")

import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

from SCons.Script import COMMAND_LINE_TARGETS


PROJECT_DIR = env.subst("$PROJECT_DIR")


def configure_source_path_maps():
    # SDK assertions retain __FILE__ in the image. Keep workstation paths out
    # of those strings and debug information, including mixed Windows slashes.
    framework_dir = env.PioPlatform().get_package_dir("framework-arduinopico")
    for source_dir, replacement in ((PROJECT_DIR, "."), (framework_dir, "framework-arduinopico")):
        if source_dir:
            for prefix in sorted({source_dir, source_dir.replace("\\", "/")}):
                env.Append(CCFLAGS=[f"-ffile-prefix-map={prefix}={replacement}"])


configure_source_path_maps()

VERIFY_SCRIPT = os.path.join(PROJECT_DIR, "tools", "run_release_checks.py")
HOST_VERIFY_SCRIPT = os.path.join(PROJECT_DIR, "tools", "run_fake_eeprom_tests.py")
COMPILE_DB_PATH = os.path.join(PROJECT_DIR, "build", "vscode", "compile_commands.json")
UF2_EXPORT_DIR = os.path.join(PROJECT_DIR, "dist")
UF2_EXPORT_PATH = os.path.join(UF2_EXPORT_DIR, f"{env['PIOENV']}.uf2")
MODERN2USB_UPDATE_VERIFY_SCRIPT = os.path.join(
    PROJECT_DIR, "tools", "verify_modern2usb_update_artifact.py"
)
MODERN2USB_RECOVERY_UF2_SCRIPT = os.path.join(
    PROJECT_DIR, "tools", "modern2usb_recovery_uf2.py"
)
OLED_LAYOUT_VERIFY_SCRIPT = os.path.join(
    PROJECT_DIR, "tools", "test_oled_shoulder_layouts.py"
)
USB_DRIVE_GENERATOR = os.path.join(
    PROJECT_DIR, "tools", "generate_usb_drive_image.py"
)
PSX_CAPTURE_DRIVE_GENERATOR = os.path.join(
    PROJECT_DIR, "tools", "generate_psx_capture_drive.py"
)
GENESIS_CAPTURE_DRIVE_GENERATOR = os.path.join(
    PROJECT_DIR, "tools", "generate_genesis_capture_drive.py"
)
JOGCON_CAPTURE_DRIVE_GENERATOR = os.path.join(PROJECT_DIR, "tools", "generate_jogcon_capture_drive.py")
N64_CAPTURE_DRIVE_GENERATOR = os.path.join(PROJECT_DIR, "tools", "generate_n64_capture_drive.py")
HOST_CAPTURE_DRIVE_GENERATOR = os.path.join(PROJECT_DIR, "tools", "generate_host_capture_drive.py")
PICOTOOL_DIR = env.PioPlatform().get_package_dir("tool-picotool-rp2040-earlephilhower")
PICOTOOL_EXE = os.path.join(PICOTOOL_DIR, "picotool.exe" if os.name == "nt" else "picotool")
VERIFY_STAMP = "REFLEX_MENU_VERIFIED"
HOST_VERIFY_STAMP = "REFLEX_FAKE_EEPROM_VERIFIED"
OLED_LAYOUT_VERIFY_STAMP = "REFLEX_OLED_LAYOUT_VERIFIED"
VERIFY_OPT_IN = "REFLEX_RUN_MENU_VERIFIER"
HOST_VERIFY_OPT_IN = "REFLEX_RUN_FAKE_EEPROM_VERIFIER"
SKIP_TARGETS = {"clean", "idedata", "__idedata", "envdump"}


def configure_management_identity_build():
    sys.path.insert(0, os.path.join(PROJECT_DIR, "tools"))
    from adapt_build_identity import identity_build
    env.Append(CPPDEFINES=[("ADAPT_IDENTITY_BUILD", '\\"' + identity_build(PROJECT_DIR) + '\\"')])


configure_management_identity_build()


def env_has_define(name):
    # PlatformIO retains inherited flags in BUILD_FLAGS/CPPDEFINES even when a
    # child environment removes them with build_unflags. Hooks must follow the
    # effective compiler policy or RP2040-only variants can inherit updater
    # packaging that their firmware explicitly compiled out.
    for flag in env.get("BUILD_UNFLAGS", []):
        if flag == f"-D{name}" or flag.startswith(f"-D{name}="):
            return False
    for define in env.get("CPPDEFINES", []):
        if define == name:
            return True
        if isinstance(define, (tuple, list)) and define and define[0] == name:
            return True
    for flag in env.get("BUILD_FLAGS", []):
        if flag == f"-D{name}" or flag.startswith(f"-D{name}="):
            return True
    return False


if env_has_define("USB2DB15_USER_BUILD"):
    # A tester artifact must not silently inherit a PC-maintenance stack or
    # diagnostic takeover. Bench variants remain separately buildable.
    if not env_has_define("PRODUCT_USB2DB15"):
        raise Exception("USB2DB15 user build requires the USB2DB15 product")
    for bench_flag in (
        "ENABLE_USB2DB15_SETTINGS_BENCH", "ENABLE_USB2DB15_DUAL_ROLE_BENCH",
        "ENABLE_USB2DB15_HOST_DIAGNOSTICS", "ENABLE_USB2DB15_FAULT_CAPTURE",
        "ENABLE_USB2DB15_TEST_LOG", "ENABLE_HID_COMPAT_CAPTURE",
        "ENABLE_DEBUG_UART", "ENABLE_RUNTIME_SERIAL_DEBUG_CDC",
        "ENABLE_TINYUSB_HOST_ENUM_DIAGNOSTICS", "CFG_TUSB_CONFIG_FILE",
    ):
        if env_has_define(bench_flag):
            raise Exception("USB2DB15 user build forbids " + bench_flag)


if env_has_define("ENABLE_USB2DB15_TEST_LOG"):
    if not env_has_define("PRODUCT_USB2DB15"):
        raise Exception("Tester log reservation requires USB2DB15")

    def reserve_usb2db15_log(target, source, env):
        sys.path.insert(0, os.path.join(PROJECT_DIR, "tools"))
        from usb2db15_test_log_layout import reserve_log_region
        path = os.path.join(env.subst("$BUILD_DIR"), "memmap_default.ld")
        with open(path, encoding="utf-8") as handle:
            linker = reserve_log_region(handle.read())
        with open(path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(linker)

    env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", env.VerboseAction(
        reserve_usb2db15_log, "Reserving 72 KiB for USB2DB15 diagnostic log"))
    env.Depends("$BUILD_DIR/${PROGNAME}.elf", env.File(
        os.path.join(PROJECT_DIR, "tools", "usb2db15_test_log_layout.py")))


XBOX_WIRELESS_FIRMWARE_SHA256 = (
    "48084d9fa53b9bb04358f3bb127b7495dc8f7bb0b3ca1437bd24ef2b6eabdf66"
)

if env_has_define("ADAPT_ENABLE_N64_CAPTURE") or env_has_define("ADAPT_ENABLE_HOST_CAPTURE"):
    # The host capture reuses the N64 diagnostic's bounded layout and slot.
    def reserve_n64_capture_log(target, source, env):
        sys.path.insert(0, os.path.join(PROJECT_DIR, "tools"))
        from n64_capture_layout import reserve_log_region
        path = os.path.join(env.subst("$BUILD_DIR"), "memmap_default.ld")
        with open(path, encoding="utf-8") as handle:
            linker = reserve_log_region(handle.read())
        with open(path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(linker)
    env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", env.VerboseAction(
        reserve_n64_capture_log, "Bounding N64 diagnostic app; preserving EEPROM/profile addresses"))
    env.Depends("$BUILD_DIR/${PROGNAME}.elf", env.File(
        os.path.join(PROJECT_DIR, "tools", "n64_capture_layout.py")))


def configure_xbox_wireless_firmware():
    """Embed a locally supplied Microsoft MT76 blob in the explicit lab build.

    The proprietary image remains outside version control. Refuse a missing or
    unexpected blob so a laboratory image can never silently ship without the
    exact firmware that its loader was written and tested against.
    """
    if not env_has_define("ENABLE_XBOX_WIRELESS_ADAPTER_FIRMWARE_LAB"):
        return
    if set(COMMAND_LINE_TARGETS) & SKIP_TARGETS:
        return

    source_path = os.environ.get(
        "REFLEX_XBOX_WIRELESS_FIRMWARE",
        os.path.join(
            PROJECT_DIR,
            "build",
            "references",
            "xbox-wireless-firmware",
            "FW_ACC_00U.bin",
        ),
    )
    if not os.path.isfile(source_path):
        raise Exception(
            "Xbox wireless laboratory firmware is missing. Set "
            "REFLEX_XBOX_WIRELESS_FIRMWARE to FW_ACC_00U.bin."
        )
    with open(source_path, "rb") as firmware_file:
        firmware = firmware_file.read()
    digest = hashlib.sha256(firmware).hexdigest()
    if digest != XBOX_WIRELESS_FIRMWARE_SHA256:
        raise Exception(
            "Xbox wireless laboratory firmware SHA-256 mismatch: " + digest
        )

    generated_dir = os.path.join(
        env.subst("$BUILD_DIR"), "generated", "xbox_wireless"
    )
    os.makedirs(generated_dir, exist_ok=True)
    generated_path = os.path.join(generated_dir, "xbox_wireless_firmware.inc")
    with open(generated_path, "w", encoding="ascii", newline="\n") as output:
        output.write("// Generated from a local, hash-verified firmware blob.\n")
        output.write("static const unsigned char kXboxWirelessFirmware[] = {\n")
        for offset in range(0, len(firmware), 16):
            row = ", ".join(f"0x{byte:02X}" for byte in firmware[offset:offset + 16])
            output.write("  " + row + ",\n")
        output.write("};\n")
        output.write(
            "static constexpr unsigned int kXboxWirelessFirmwareSize = "
            f"{len(firmware)}u;\n"
        )
    env.Append(CPPPATH=[generated_dir])
    print(
        "Embedded hash-verified Xbox wireless laboratory firmware "
        f"({len(firmware)} bytes)"
    )


def run_menu_verifier():
    if os.environ.get(VERIFY_OPT_IN) != "1":
        return

    if os.environ.get(VERIFY_STAMP) == "1":
        return

    if set(COMMAND_LINE_TARGETS) & SKIP_TARGETS:
        return

    print("Running structural menu/settings release checks")
    result = subprocess.run([sys.executable, VERIFY_SCRIPT], cwd=PROJECT_DIR)
    if result.returncode != 0:
        raise Exception(f"run_release_checks.py failed with exit code {result.returncode}")

    os.environ[VERIFY_STAMP] = "1"


def run_host_eeprom_verifier():
    if os.environ.get(HOST_VERIFY_OPT_IN) != "1":
        return

    if os.environ.get(HOST_VERIFY_STAMP) == "1":
        return

    if set(COMMAND_LINE_TARGETS) & SKIP_TARGETS:
        return

    print("Running fake EEPROM host verifier")
    result = subprocess.run([sys.executable, HOST_VERIFY_SCRIPT], cwd=PROJECT_DIR)
    if result.returncode == 127:
        print("Warning: skipping fake EEPROM host verifier (no native compiler or configured WSL)")
        os.environ[HOST_VERIFY_STAMP] = "1"
        return
    if result.returncode != 0:
        raise Exception(f"run_fake_eeprom_tests.py failed with exit code {result.returncode}")

    os.environ[HOST_VERIFY_STAMP] = "1"


def run_oled_layout_verifier():
    if os.environ.get(OLED_LAYOUT_VERIFY_STAMP) == "1":
        return
    if set(COMMAND_LINE_TARGETS) & SKIP_TARGETS:
        return

    print("Running 10-character OLED controller-layout guard")
    result = subprocess.run(
        [sys.executable, OLED_LAYOUT_VERIFY_SCRIPT], cwd=PROJECT_DIR
    )
    if result.returncode != 0:
        raise Exception(
            "test_oled_shoulder_layouts.py failed with exit code "
            f"{result.returncode}"
        )
    os.environ[OLED_LAYOUT_VERIFY_STAMP] = "1"


def generate_usb_drive_image():
    # Every env compiles features/usb_drive; only drive products include the
    # image, but generating it always keeps the include present. The script
    # rewrites the file only when its contents change.
    if set(COMMAND_LINE_TARGETS) & SKIP_TARGETS:
        return
    result = subprocess.run([sys.executable, USB_DRIVE_GENERATOR], cwd=PROJECT_DIR)
    if result.returncode != 0:
        raise Exception(
            "generate_usb_drive_image.py failed with exit code "
            f"{result.returncode}"
        )

    if env_has_define("ADAPT_ENABLE_N64_CAPTURE"):
        result = subprocess.run([sys.executable, N64_CAPTURE_DRIVE_GENERATOR], cwd=PROJECT_DIR)
        if result.returncode != 0:
            raise Exception("generate_n64_capture_drive.py failed with exit code " + str(result.returncode))

    if env_has_define("ADAPT_ENABLE_HOST_CAPTURE"):
        result = subprocess.run([sys.executable, HOST_CAPTURE_DRIVE_GENERATOR], cwd=PROJECT_DIR)
        if result.returncode != 0:
            raise Exception("generate_host_capture_drive.py failed with exit code " + str(result.returncode))

    if env_has_define("ADAPT_ENABLE_GENESIS_IDENTIFICATION_CAPTURE"):
        result = subprocess.run(
            [sys.executable, GENESIS_CAPTURE_DRIVE_GENERATOR], cwd=PROJECT_DIR
        )
        if result.returncode != 0:
            raise Exception("generate_genesis_capture_drive.py failed with exit code " + str(result.returncode))

    if env_has_define("ADAPT_ENABLE_JOGCON_CAPTURE"):
        result = subprocess.run([sys.executable, JOGCON_CAPTURE_DRIVE_GENERATOR], cwd=PROJECT_DIR)
        if result.returncode != 0:
            raise Exception("generate_jogcon_capture_drive.py failed with exit code " + str(result.returncode))

    if env_has_define("ADAPT_ENABLE_PSX_IDENTIFICATION_CAPTURE"):
        result = subprocess.run(
            [sys.executable, PSX_CAPTURE_DRIVE_GENERATOR], cwd=PROJECT_DIR
        )
        if result.returncode != 0:
            raise Exception(
                "generate_psx_capture_drive.py failed with exit code "
                f"{result.returncode}"
            )


configure_xbox_wireless_firmware()
generate_usb_drive_image()
run_oled_layout_verifier()
run_menu_verifier()
run_host_eeprom_verifier()
os.makedirs(os.path.dirname(COMPILE_DB_PATH), exist_ok=True)
env.Replace(COMPILATIONDB_PATH=COMPILE_DB_PATH)


def write_generated_tinyusb_source(env, node, patch_group, patched_source):
    source_path = node.srcnode().get_abspath()
    relative_dir = os.path.relpath(os.path.dirname(source_path), PROJECT_DIR)
    include_paths = [os.path.dirname(source_path)]
    normalized_source = source_path.replace("\\", "/")
    tinyusb_marker = "/Adafruit_TinyUSB_Arduino/src/"
    marker_index = normalized_source.find(tinyusb_marker)
    if marker_index >= 0:
        tinyusb_src = source_path[:marker_index + len("/Adafruit_TinyUSB_Arduino/src")].replace("/", os.sep)
        relative_dir = os.path.join(
            "tinyusb",
            os.path.relpath(os.path.dirname(source_path), tinyusb_src),
        )
        include_paths.extend([
            tinyusb_src,
            os.path.join(tinyusb_src, "arduino"),
            os.path.join(tinyusb_src, "class"),
            os.path.join(tinyusb_src, "common"),
            os.path.join(tinyusb_src, "device"),
            os.path.join(tinyusb_src, "host"),
        ])
    env.Prepend(CPPPATH=include_paths)
    patched_root = os.path.join(PROJECT_DIR, "build", "patched_sources")
    prospective_path = os.path.join(
        patched_root,
        env["PIOENV"],
        patch_group,
        relative_dir,
        os.path.basename(source_path),
    )
    if os.name == "nt" and len(prospective_path) > 220:
        project_key = hashlib.sha256(
            os.path.abspath(PROJECT_DIR).encode("utf-8")
        ).hexdigest()[:12]
        patch_key = hashlib.sha256(patch_group.encode("utf-8")).hexdigest()[:12]
        patched_root = os.path.join(
            tempfile.gettempdir(), "reflex-adapt-patched", project_key
        )
        patch_group = patch_key
    patched_dir = os.path.join(
        patched_root,
        env["PIOENV"],
        patch_group,
        relative_dir,
    )
    os.makedirs(patched_dir, exist_ok=True)
    patched_path = os.path.join(patched_dir, os.path.basename(source_path))
    with open(patched_path, "w", encoding="utf-8", newline="") as handle:
        handle.write(patched_source)
    print(f"Using generated TinyUSB patch source: {patched_path}")
    return env.File(patched_path)


def patch_tinyusb_host_source(env, node):
    # Keep the tracked intent in tools/tinyusb_patches/*.patch. The middleware
    # applies the same small patches to one generated source file so multiple
    # host fixes cannot compile competing copies of TinyUSB's usbh.c.
    source_path = node.srcnode().get_abspath()
    env.Append(CPPPATH=[os.path.dirname(source_path)])
    with open(source_path, "r", encoding="utf-8") as handle:
        source = handle.read()

    patched = source
    patch_groups = []

    if env_has_define("ENABLE_USB_BT_HCI"):
        needle = """      tuh_enum_descriptor_device_cb(daddr, desc_device); // callback
      tuh_descriptor_get_string_langid(daddr, _usbh_epbuf.ctrl, 2,
                                       process_enumeration, ENUM_GET_STRING_LANGUAGE_ID);
      break;
"""
        replacement = """      tuh_enum_descriptor_device_cb(daddr, desc_device); // callback
      if (desc_device->iManufacturer == 0 &&
          desc_device->iProduct == 0 &&
          desc_device->iSerialNumber == 0) {
        uint8_t const config_idx = 0;
        TU_LOG_USBH("Skip string descriptors; get Configuration[%u] Descriptor (9 bytes)\\r\\n", config_idx);
        TU_ASSERT(tuh_descriptor_get_configuration(daddr, config_idx, _usbh_epbuf.ctrl, 9,
                                                   process_enumeration, ENUM_GET_FULL_CONFIG_DESC),);
        break;
      }
      tuh_descriptor_get_string_langid(daddr, _usbh_epbuf.ctrl, 2,
                                       process_enumeration, ENUM_GET_STRING_LANGUAGE_ID);
      break;
"""
        if needle not in patched:
            raise Exception("TinyUSB host enum patch anchor not found")
        patched = patched.replace(needle, replacement, 1)
        patch_groups.append("bt-host-skip-empty-strings")

    if env_has_define("ENABLE_TINYUSB_HOST_ENUM_DIAGNOSTICS"):
        diagnostics_needle = """static usbh_data_t _usbh_data = {
  .controller_id = TUSB_INDEX_INVALID_8,
};
"""
        diagnostics_replacement = diagnostics_needle + """
static volatile uint8_t g_reflex_last_enum_state = 0xFF;
static volatile uint8_t g_reflex_last_enum_result = XFER_RESULT_INVALID;
static volatile uint32_t g_reflex_enum_callback_count = 0;
static volatile uint32_t g_reflex_enum_failure_count = 0;

uint8_t reflex_tinyusb_host_enumerating_daddr(void) {
  return _usbh_data.enumerating_daddr;
}
uint8_t reflex_tinyusb_host_control_stage(void) {
  return _usbh_data.ctrl_xfer_info.stage;
}
uint8_t reflex_tinyusb_host_control_daddr(void) {
  return _usbh_data.ctrl_xfer_info.daddr;
}
uint8_t reflex_tinyusb_host_dev0_hub_addr(void) {
  return _usbh_data.dev0_bus.hub_addr;
}
uint8_t reflex_tinyusb_host_dev0_hub_port(void) {
  return _usbh_data.dev0_bus.hub_port;
}
uint8_t reflex_tinyusb_host_last_enum_state(void) {
  return g_reflex_last_enum_state;
}
uint8_t reflex_tinyusb_host_last_enum_result(void) {
  return g_reflex_last_enum_result;
}
uint32_t reflex_tinyusb_host_enum_callback_count(void) {
  return g_reflex_enum_callback_count;
}
uint32_t reflex_tinyusb_host_enum_failure_count(void) {
  return g_reflex_enum_failure_count;
}
"""
        declaration_needle = """static void process_enumeration(tuh_xfer_t* xfer) {
  // Retry a few times while enumerating since device can be unstable when starting up
  static uint8_t failed_count = 0;
"""
        declaration_replacement = """static void process_enumeration(tuh_xfer_t* xfer) {
  // Retry a few times while enumerating since device can be unstable when starting up
  static uint8_t failed_count = 0;
  g_reflex_last_enum_state = (uint8_t) xfer->user_data;
  g_reflex_last_enum_result = (uint8_t) xfer->result;
  g_reflex_enum_callback_count++;
  if (xfer->result != XFER_RESULT_SUCCESS) {
    g_reflex_enum_failure_count++;
  }
  // Some full-speed hubs publish reset-change one or more status polls after
  // the nominal reset delay. Bound the follow-up polls so enumeration cannot
  // become permanently stranded or loop forever.
  static uint8_t hub_reset_status_retry_count = 0;
"""
        reset_needle = """    case ENUM_HUB_RERSET: {
      hub_port_status_response_t port_status;
      hub_port_get_status_local(dev0_bus->hub_addr, dev0_bus->hub_port, &port_status);

      if (!port_status.status.connection) {
        TU_LOG_USBH("Device unplugged from hub while debouncing\\r\\n");
        enum_full_complete();
        return;
      }

      TU_ASSERT(hub_port_reset(dev0_bus->hub_addr, dev0_bus->hub_port, process_enumeration, ENUM_HUB_GET_STATUS_AFTER_RESET),);
      break;
    }
"""
        reset_replacement = reset_needle.replace(
            "      TU_ASSERT(hub_port_reset(dev0_bus->hub_addr, dev0_bus->hub_port, process_enumeration, ENUM_HUB_GET_STATUS_AFTER_RESET),);",
            """      hub_reset_status_retry_count = 0;
      if (!hub_port_reset(dev0_bus->hub_addr, dev0_bus->hub_port,
                          process_enumeration,
                          ENUM_HUB_GET_STATUS_AFTER_RESET)) {
        enum_full_complete();
        return;
      }""",
        )
        clear_needle = """      if (port_status.change.reset) {
        // Acknowledge Port Reset Change
        TU_ASSERT(hub_port_clear_reset_change(dev0_bus->hub_addr, dev0_bus->hub_port, process_enumeration, ENUM_HUB_CLEAR_RESET_COMPLETE),);
      } else {
        // maybe retry if reset change not set but we need timeout to prevent infinite loop
        // TU_ASSERT(hub_port_get_status(dev0_bus->hub_addr, dev0_bus->hub_port, NULL, process_enumeration, ENUM_HUB_CLEAR_RESET_COMPLETE),);
      }
"""
        clear_replacement = """      if (port_status.change.reset) {
        hub_reset_status_retry_count = 0;
        // Acknowledge Port Reset Change
        TU_ASSERT(hub_port_clear_reset_change(dev0_bus->hub_addr, dev0_bus->hub_port, process_enumeration, ENUM_HUB_CLEAR_RESET_COMPLETE),);
      } else if (++hub_reset_status_retry_count < 8) {
        tusb_time_delay_ms_api(ENUM_RESET_HUB_DELAY_MS);
        TU_ASSERT(hub_port_get_status(dev0_bus->hub_addr, dev0_bus->hub_port, NULL, process_enumeration, ENUM_HUB_CLEAR_RESET),);
      } else if (port_status.status.connection &&
                 !port_status.status.reset) {
        TU_LOG_USBH("Hub reset complete without reset-change; continue\\r\\n");
        hub_reset_status_retry_count = 0;
        TU_ASSERT(hub_port_get_status(dev0_bus->hub_addr, dev0_bus->hub_port, NULL, process_enumeration, ENUM_HUB_CLEAR_RESET_COMPLETE),);
      } else {
        TU_LOG_USBH("Hub reset-change timeout\\r\\n");
        hub_reset_status_retry_count = 0;
        enum_full_complete();
      }
"""
        task_start_needle = """void tuh_task_ext(uint32_t timeout_ms, bool in_isr) {
  (void) in_isr; // not implemented yet

"""
        task_start_replacement = """void tuh_task_ext(uint32_t timeout_ms, bool in_isr) {
  (void) in_isr; // not implemented yet

  // An onboard hub can queue several attach events while another device is
  // enumerating. Bound work after complete events so the cooperative host
  // owner can service transfers and deadlines without dequeuing or reordering
  // the next FIFO event.
  uint8_t processed_events = 0;

"""
        task_end_needle = """      default:
        break;
    }

#if CFG_TUSB_OS != OPT_OS_NONE && CFG_TUSB_OS != OPT_OS_PICO
"""
        task_end_replacement = """      default:
        break;
    }

    if (++processed_events >= 32) {
      return;
    }

#if CFG_TUSB_OS != OPT_OS_NONE && CFG_TUSB_OS != OPT_OS_PICO
"""
        for anchor, replacement in (
                (diagnostics_needle, diagnostics_replacement),
                (declaration_needle, declaration_replacement),
                (reset_needle, reset_replacement),
                (clear_needle, clear_replacement),
                (task_start_needle, task_start_replacement),
                (task_end_needle, task_end_replacement)):
            if anchor not in patched:
                raise Exception("TinyUSB delayed hub reset-change patch anchor not found")
            patched = patched.replace(anchor, replacement, 1)
        patch_groups.append("hub-reset-change-retry")
        patch_groups.append("host-task-fairness-budget")
        patch_groups.append("host-enumeration-diagnostics")
        initial_status_needle = """    TU_ASSERT(hub_port_get_status(dev0_bus->hub_addr, dev0_bus->hub_port, NULL,
                                  process_enumeration, ENUM_HUB_RERSET));
"""
        initial_status_replacement = """    // The native hub handler and Modern2USB recovery both refresh the hub's
    // cached port status before queueing attach. Reuse that validated cache;
    // another GET_STATUS here can collide with the hub interrupt transaction.
    tuh_xfer_t xfer = {
      .daddr = dev0_bus->hub_addr,
      .result = XFER_RESULT_SUCCESS,
      .user_data = ENUM_HUB_RERSET
    };
    process_enumeration(&xfer);
"""
        if initial_status_needle not in patched:
            raise Exception("TinyUSB cached hub attach anchor not found")
        patched = patched.replace(
            initial_status_needle, initial_status_replacement, 1
        )
        patch_groups.append("host-hub-cached-attach")
        reset_recovery_needle = "ENUM_RESET_RECOVERY_DELAY_MS = 10,"
        reset_recovery_replacement = "ENUM_RESET_RECOVERY_DELAY_MS = 50,"
        if reset_recovery_needle not in patched:
            raise Exception("TinyUSB reset recovery delay anchor not found")
        patched = patched.replace(
            reset_recovery_needle, reset_recovery_replacement, 1
        )
        patch_groups.append("host-reset-recovery-delay")

    if not patch_groups:
        return node
    return write_generated_tinyusb_source(
        env, node, "+".join(patch_groups), patched
    )


def patch_tinyusb_hub_callback_source(env, node):
    # TinyUSB 3.7.3 stores the intercepted hub-port status completion callback
    # in one global. Do not replace the owner until the new EP0 request has
    # actually been accepted; otherwise a busy submission corrupts the
    # callback for the request already in flight.
    source_path = node.srcnode().get_abspath()
    with open(source_path, "r", encoding="utf-8") as handle:
        source = handle.read()

    function_anchor = "bool hub_port_get_status("
    next_function_anchor = "bool hub_port_get_status_local("
    if function_anchor not in source or next_function_anchor not in source:
        raise Exception(
            f"TinyUSB hub callback function anchor not found: {source_path}")
    prefix, remainder = source.split(function_anchor, 1)
    body, suffix = remainder.split(next_function_anchor, 1)

    scalar_complete = "    user_complete_cb = complete_cb;\n"
    scalar_null = "    user_complete_cb = NULL;\n"
    indexed_complete = (
        "    user_complete_cb[get_hub_index(hub_addr)] = complete_cb;\n")
    indexed_null = (
        "    user_complete_cb[get_hub_index(hub_addr)] = NULL;\n")
    if scalar_complete in body and scalar_null in body:
        body = body.replace(scalar_complete, "", 1)
        body = body.replace(scalar_null, "", 1)
        owner_update = (
            "  user_complete_cb = (hub_port != 0) ? complete_cb : NULL;\n")
    elif indexed_complete in body and indexed_null in body:
        body = body.replace(indexed_complete, "", 1)
        body = body.replace(indexed_null, "", 1)
        owner_update = (
            "  user_complete_cb[get_hub_index(hub_addr)] = "
            "(hub_port != 0) ? complete_cb : NULL;\n")
    else:
        raise Exception(
            f"TinyUSB hub callback ownership anchor not found: {source_path}")

    submit = "  TU_VERIFY(tuh_control_xfer(&xfer));\n"
    if submit not in body:
        raise Exception(
            f"TinyUSB hub callback submit anchor not found: {source_path}")
    body = body.replace(submit, submit + owner_update, 1)
    patched = (
        prefix + function_anchor + body + next_function_anchor + suffix)
    return write_generated_tinyusb_source(
        env, node, "hub-callback-ownership", patched
    )


def patch_tinyusb_device_descriptor_hooks(env, node):
    # Keep the tracked intent in tools/tinyusb_patches/xinput-autodetect-descriptor-hooks.patch.
    source_path = node.srcnode().get_abspath()
    with open(source_path, "r", encoding="utf-8") as handle:
        source = handle.read()

    replacements = [
        (
            """// TinyUSB stack callbacks
//--------------------------------------------------------------------+

extern "C" {
""",
            """// TinyUSB stack callbacks
//--------------------------------------------------------------------+

// Hook function pointers for descriptor overrides (used by XInput/AUTO detect).
uint8_t const* (*tud_descriptor_device_hook)(void) = nullptr;
uint8_t const* (*tud_descriptor_configuration_hook)(uint8_t index) = nullptr;
uint16_t const* (*tud_descriptor_string_hook)(uint8_t index, uint16_t langid) = nullptr;

extern "C" {
""",
        ),
        (
            """uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *)&TinyUSBDevice._desc_device;
}
""",
            """uint8_t const *tud_descriptor_device_cb(void) {
  if (tud_descriptor_device_hook) {
    return tud_descriptor_device_hook();
  }
  return (uint8_t const *)&TinyUSBDevice._desc_device;
}
""",
        ),
        (
            """uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    return TinyUSBDevice._desc_cfg;
}
""",
            """uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    if (tud_descriptor_configuration_hook) {
      return tud_descriptor_configuration_hook(index);
    }
    return TinyUSBDevice._desc_cfg;
}
""",
        ),
        (
            """uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
   if (index == 0xEE)
     return get_ms_os_10_string_ptr();
  return TinyUSBDevice.descriptor_string_cb(index, langid);
}
""",
            """uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  if (index == STRID_MANUFACTURER) {
    string_manufacturer_read_cb();
  }

  if (tud_descriptor_string_hook) {
    return tud_descriptor_string_hook(index, langid);
  }

  if (index == 0xEE) {
    return get_ms_os_10_string_ptr();
  }

  return TinyUSBDevice.descriptor_string_cb(index, langid);
}
""",
        ),
    ]

    patched = source
    for patch_index, (needle, replacement) in enumerate(replacements):
        if needle not in patched:
            raise Exception(f"TinyUSB device descriptor hook patch anchor {patch_index} not found in {source_path}")
        patched = patched.replace(needle, replacement, 1)
    return write_generated_tinyusb_source(env, node, "xinput-autodetect-descriptor-hooks", patched)


def patch_tinyusb_usbd_ms_os_request_hook(env, node):
    # Keep the tracked intent in tools/tinyusb_patches/xinput-autodetect-descriptor-hooks.patch.
    source_path = node.srcnode().get_abspath()
    with open(source_path, "r", encoding="utf-8") as handle:
        source = handle.read()

    needle = """      TU_LOG_USBD(" String[%u]\\r\\n", desc_index);

      // String Descriptor always uses the desc set from user
      uint8_t const* desc_str = (uint8_t const*) tud_descriptor_string_cb(desc_index, tu_le16toh(p_request->wIndex));
"""
    replacement = """      TU_LOG_USBD(" String[%u]\\r\\n", desc_index);

      if (desc_index == 0xEE) {
        extern void tud_string_0xee_requested_cb(void) TU_ATTR_WEAK;
        if (tud_string_0xee_requested_cb) {
          tud_string_0xee_requested_cb();
        }
      }

      // String Descriptor always uses the desc set from user
      uint8_t const* desc_str = (uint8_t const*) tud_descriptor_string_cb(desc_index, tu_le16toh(p_request->wIndex));
"""
    if needle not in source:
        raise Exception(f"TinyUSB USBD MS OS request hook patch anchor not found in {source_path}")
    patched = source.replace(needle, replacement, 1)

    setup_needle = """static bool process_control_request(uint8_t rhport, tusb_control_request_t const * p_request) {
  usbd_control_set_complete_callback(NULL);
"""
    setup_replacement = """static bool process_control_request(uint8_t rhport, tusb_control_request_t const * p_request) {
  extern void tud_setup_request_observed_cb(tusb_control_request_t const* request) TU_ATTR_WEAK;
  if (tud_setup_request_observed_cb) {
    tud_setup_request_observed_cb(p_request);
  }

  usbd_control_set_complete_callback(NULL);
"""
    if setup_needle not in patched:
        raise Exception(f"TinyUSB setup request observer hook anchor not found in {source_path}")
    patched = patched.replace(setup_needle, setup_replacement, 1)

    xfer_needle = """        if (0 == epnum) {
          usbd_control_xfer_cb(event.rhport, ep_addr, (xfer_result_t) event.xfer_complete.result,
                               event.xfer_complete.len);
        } else {
          usbd_class_driver_t const* driver = get_driver(_usbd_dev.ep2drv[epnum][ep_dir]);
          TU_ASSERT(driver,);

          TU_LOG_USBD("  %s xfer callback\\r\\n", driver->name);
          driver->xfer_cb(event.rhport, ep_addr, (xfer_result_t) event.xfer_complete.result, event.xfer_complete.len);
        }
"""
    xfer_replacement = """        if (0 == epnum) {
          usbd_control_xfer_cb(event.rhport, ep_addr, (xfer_result_t) event.xfer_complete.result,
                               event.xfer_complete.len);
        } else {
          uint8_t const driver_id = _usbd_dev.ep2drv[epnum][ep_dir];
          extern bool tud_xfer_complete_override_cb(uint8_t rhport,
                                                    uint8_t ep_addr,
                                                    uint8_t driver_id,
                                                    xfer_result_t result,
                                                    uint32_t xferred_bytes) TU_ATTR_WEAK;
          if (tud_xfer_complete_override_cb &&
              tud_xfer_complete_override_cb(event.rhport,
                                            ep_addr,
                                            driver_id,
                                            (xfer_result_t) event.xfer_complete.result,
                                            event.xfer_complete.len)) {
            break;
          }

          usbd_class_driver_t const* driver = get_driver(driver_id);
          TU_ASSERT(driver,);

          TU_LOG_USBD("  %s xfer callback\\r\\n", driver->name);
          driver->xfer_cb(event.rhport, ep_addr, (xfer_result_t) event.xfer_complete.result, event.xfer_complete.len);
        }
"""
    if xfer_needle not in patched:
        raise Exception(f"TinyUSB xfer complete override hook anchor not found in {source_path}")
    patched = patched.replace(xfer_needle, xfer_replacement, 1)
    return write_generated_tinyusb_source(env, node, "xinput-autodetect-descriptor-hooks", patched)


def patch_tinyusb_pio_clear_stall(env, node):
    source_path = node.srcnode().get_abspath()
    with open(source_path, "r", encoding="utf-8") as handle:
        source = handle.read()

    needle = """bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;

  return true;
}
"""
    replacement = """bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  uint8_t const pio_rhport = RHPORT_PIO(rhport);

  // CLEAR_FEATURE(ENDPOINT_HALT) resets the device endpoint to DATA0.
  // Mirror that reset in Pico-PIO-USB before the class retries a transfer.
  for (uint8_t ep_idx = 0; ep_idx < PIO_USB_EP_POOL_CNT; ep_idx++) {
    endpoint_t *ep = PIO_USB_ENDPOINT(ep_idx);
    if (ep->size != 0 && ep->root_idx == pio_rhport &&
        ep->dev_addr == dev_addr && ep->ep_num == ep_addr) {
      ep->stalled = false;
      ep->has_transfer = false;
      ep->transfer_started = false;
      ep->transfer_aborted = false;
      ep->data_id = 0;
      ep->actual_len = 0;
      ep->total_len = 0;
      return true;
    }
  }

  return false;
}
"""
    if needle not in source:
        raise Exception(
            f"TinyUSB PIO endpoint recovery patch anchor not found in {source_path}"
        )
    patched = source.replace(needle, replacement, 1)
    return write_generated_tinyusb_source(
        env, node, "pio-usb-endpoint-recovery", patched
    )


def patch_tinyusb_native_host_sequence(env, node):
    sys.path.insert(0, os.path.join(PROJECT_DIR, "tools"))
    from tinyusb_native_host_patch import patch_native_host_sequence
    source_path = node.srcnode().get_abspath()
    with open(source_path, encoding="utf-8") as source_file:
        patched = patch_native_host_sequence(source_file.read())
    env.Prepend(CPPPATH=[os.path.join(PROJECT_DIR, "firmware", "platform")])
    return write_generated_tinyusb_source(env, node, "native-host-sequence-recovery", patched)


if env_has_define("ENABLE_RP2040_NATIVE_HOST_SEQ_RECOVERY"):
    if not (env_has_define("PRODUCT_USB2DB15") and
            (env_has_define("ENABLE_USB2DB15_HOST_DIAGNOSTICS") or
             env_has_define("ENABLE_USB2DB15_TEST_LOG") or
             env_has_define("USB2DB15_USER_BUILD"))):
        raise Exception("Native USB sequence recovery requires a USB2DB15 bench or user build")
    env.AddBuildMiddleware(
        patch_tinyusb_native_host_sequence,
        "*Adafruit_TinyUSB_Arduino*portable*raspberrypi*rp2040*hcd_rp2040.c",
    )


if env_has_define("ENABLE_XBOX_WIRELESS_ADAPTER_FIRMWARE_LAB"):
    env.AddBuildMiddleware(
        patch_tinyusb_pio_clear_stall,
        "*Adafruit_TinyUSB_Arduino*portable*raspberrypi*pio_usb*hcd_pio_usb.c",
    )


env.AddBuildMiddleware(
    patch_tinyusb_device_descriptor_hooks,
    "*Adafruit_TinyUSB_Arduino*arduino*Adafruit_USBD_Device.cpp",
)
env.AddBuildMiddleware(
    patch_tinyusb_usbd_ms_os_request_hook,
    "*Adafruit_TinyUSB_Arduino*device*usbd.c",
)


if (env_has_define("ENABLE_USB_BT_HCI") or
        env_has_define("ENABLE_TINYUSB_HOST_ENUM_DIAGNOSTICS")):
    env.AddBuildMiddleware(
        patch_tinyusb_host_source,
        "*Adafruit_TinyUSB_Arduino*host*usbh.c",
    )

if env_has_define("ENABLE_TINYUSB_HOST_ENUM_DIAGNOSTICS"):
    env.AddBuildMiddleware(
        patch_tinyusb_hub_callback_source,
        "*Adafruit_TinyUSB_Arduino*host*hub.c",
    )

def export_uf2(target=None, source=None, env=None, **kwargs):
    os.makedirs(UF2_EXPORT_DIR, exist_ok=True)
    result = subprocess.run(
        [PICOTOOL_EXE, "uf2", "convert", str(source[0]), UF2_EXPORT_PATH],
        cwd=PROJECT_DIR,
    )
    if result.returncode != 0:
        raise Exception(f"picotool uf2 convert failed with exit code {result.returncode}")
    if env_has_define("ENABLE_ESP32_RP2040_UPDATE"):
        result = subprocess.run(
            [sys.executable, MODERN2USB_RECOVERY_UF2_SCRIPT, UF2_EXPORT_PATH],
            cwd=PROJECT_DIR,
        )
        if result.returncode != 0:
            raise Exception(
                "MODERN2USB recovery UF2 packaging failed with exit code "
                f"{result.returncode}"
            )
        result = subprocess.run(
            [
                sys.executable,
                MODERN2USB_UPDATE_VERIFY_SCRIPT,
                "--firmware",
                os.path.join(
                    env.subst("$BUILD_DIR"),
                    "firmware.bin",
                ),
                "--linker",
                os.path.join(env.subst("$BUILD_DIR"), "memmap_default.ld"),
                "--uf2",
                UF2_EXPORT_PATH,
            ],
            cwd=PROJECT_DIR,
        )
        if result.returncode != 0:
            raise Exception(
                "exported MODERN2USB UF2 verification failed with exit code "
                f"{result.returncode}"
            )
    print(f"Exported UF2: {UF2_EXPORT_PATH}")


env.AddCustomTarget(
    "uf2",
    [
        "$BUILD_DIR/firmware.elf",
        "$BUILD_DIR/firmware.bin",
    ],
    env.VerboseAction(export_uf2, "Exporting UF2 to $PROJECT_DIR/dist/"),
    title="Build UF2",
    description="Build firmware and export dist/<env>.uf2"
)


def remove_stale_modern2usb_signed_package(firmware_path, env):
    """Never leave an old portal package beside a newly built raw image."""
    package_path = os.path.join(
        env.subst("$BUILD_DIR"), "modern2usb-update.bin.signed"
    )
    if not os.path.isfile(package_path):
        return

    with open(str(firmware_path), "rb") as firmware_file:
        firmware = firmware_file.read()
    with open(package_path, "rb") as package_file:
        package = package_file.read()

    signature_size = 256
    trailer_size = 4
    package_matches = (
        len(package) > signature_size + trailer_size
        and int.from_bytes(package[-trailer_size:], "little") == signature_size
        and package[: -(signature_size + trailer_size)] == firmware
    )
    if package_matches:
        return

    os.remove(package_path)
    print(
        "Removed stale MODERN2USB signed updater package; sign the current "
        f"firmware.bin before web/serial update: {package_path}"
    )

def verify_modern2usb_update_layout(target=None, source=None, env=None, **kwargs):
    firmware_path = target[0] if target else os.path.join(
        env.subst("$BUILD_DIR"), f"{env.subst('$PROGNAME')}.bin"
    )
    uf2_path = os.path.join(
        env.subst("$BUILD_DIR"), f"{env.subst('$PROGNAME')}.uf2"
    )
    result = subprocess.run(
        [sys.executable, MODERN2USB_RECOVERY_UF2_SCRIPT, uf2_path],
        cwd=PROJECT_DIR,
    )
    if result.returncode != 0:
        raise Exception(
            "MODERN2USB recovery UF2 packaging failed with exit code "
            f"{result.returncode}"
        )
    result = subprocess.run(
        [
            sys.executable,
            MODERN2USB_UPDATE_VERIFY_SCRIPT,
            "--firmware",
            str(firmware_path),
            "--linker",
            os.path.join(env.subst("$BUILD_DIR"), "memmap_default.ld"),
            "--uf2",
            uf2_path,
        ],
        cwd=PROJECT_DIR,
    )
    if result.returncode != 0:
        raise Exception(
            "verify_modern2usb_update_artifact.py failed with exit code "
            f"{result.returncode}"
        )

    remove_stale_modern2usb_signed_package(firmware_path, env)

if env_has_define("ENABLE_ESP32_RP2040_UPDATE"):
    # Stage3-v2 keeps the existing LittleFS command format, but the reviewed
    # 12 KiB boot-region identity and 640 KiB primary boundary are both
    # unconditional build gates.
    env.AddPostAction(
        "$BUILD_DIR/${PROGNAME}.bin",
        env.VerboseAction(
            verify_modern2usb_update_layout,
            "Verifying MODERN2USB RP2040 rollback/update layout",
        ),
    )
