#!/usr/bin/env python3
"""Release guard for OLED shoulder-button spacing and layout routing."""

from __future__ import annotations

import ast
import re
import unittest
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MENU = ROOT / "firmware" / "menu"
LAYOUT_SOURCES = (
    MENU / "menu_pad_layouts_data.cpp",
    MENU / "menu_pad_layouts_data_arcade.cpp",
    MENU / "menu_pad_layouts_data_classic.cpp",
    MENU / "menu_pad_layouts_data_sega.cpp",
    MENU / "menu_virtual_output_pad.cpp",
)

CHARACTER_WIDTH = 6
HALF_CHARACTER = CHARACTER_WIDTH // 2
MAX_CONTROLLER_WIDTH = 10 * CHARACTER_WIDTH
SHOULDER_MASKS = ("GPAD_L1", "GPAD_R1", "GPAD_L2", "GPAD_R2")
WIDE_LEFT_GLYPHS = {
    "PAD_WIDE_CIRCLE_LEFT_ON",
    "PAD_WIDE_RECT_LEFT_ON",
    "PAD_WIDE_TRIANGLE_LEFT_ON",
}
FOUR_SHOULDER_LAYOUTS = {
    "padLayoutModernOutput",
    "padLayoutPS3",
    "padLayoutPS4DualSense",
    "padLayoutPSX",
    "padLayoutPSXDigital",
    "padLayoutPSXDualShock",
    "padLayoutUsbModern",
    "padLayoutUsbSixFace",
    "padLayoutUsbSwitch",
    "padLayoutUsbSwitch2",
    "padLayoutUsbXboxModern",
    "padLayoutUsbXInput",
    "padLayoutUsbLuna",
    "padLayoutWiiUPro",
}
SPECIAL_FOUR_SHOULDER_LAYOUTS = {"padLayoutWii", "padLayoutPSXJogcon"}

ARRAY_PATTERN = re.compile(
    r"const\s+PadButton\s+(padLayout[A-Za-z0-9_]+)\s*\[\]\s*=\s*"
    r"\{(.*?)\n\};",
    flags=re.DOTALL,
)
ENTRY_PATTERN = re.compile(
    r"\{\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^}]+)\}"
)


@dataclass(frozen=True)
class Entry:
    mask: str
    row: int
    col: int
    on: str
    off: str


def integer_expression(text: str) -> int:
    node = ast.parse(text.strip(), mode="eval").body

    def evaluate(current: ast.AST) -> int:
        if isinstance(current, ast.Constant) and isinstance(current.value, int):
            return current.value
        if isinstance(current, ast.UnaryOp) and isinstance(current.op, ast.USub):
            return -evaluate(current.operand)
        if isinstance(current, ast.BinOp):
            left = evaluate(current.left)
            right = evaluate(current.right)
            if isinstance(current.op, ast.Add):
                return left + right
            if isinstance(current.op, ast.Sub):
                return left - right
            if isinstance(current.op, ast.Mult):
                return left * right
        raise ValueError(f"unsupported integer expression: {text}")

    return evaluate(node)


def read_layouts() -> dict[str, list[Entry]]:
    layouts: dict[str, list[Entry]] = {}
    for path in LAYOUT_SOURCES:
        source = path.read_text(encoding="utf-8")
        for match in ARRAY_PATTERN.finditer(source):
            name, body = match.groups()
            if name in layouts:
                raise AssertionError(f"duplicate OLED pad layout: {name}")
            entries: list[Entry] = []
            for entry_match in ENTRY_PATTERN.finditer(body):
                mask, row, col, on, off = (
                    value.strip() for value in entry_match.groups()
                )
                if not mask.startswith("GPAD_") or "|" in mask:
                    continue
                entries.append(
                    Entry(
                        mask=mask,
                        row=integer_expression(row),
                        col=integer_expression(col),
                        on=on,
                        off=off,
                    )
                )
            layouts[name] = entries
    return layouts


def read_all_source_entries() -> list[tuple[str, Entry]]:
    entries: list[tuple[str, Entry]] = []
    for path in LAYOUT_SOURCES:
        source = path.read_text(encoding="utf-8")
        for entry_match in ENTRY_PATTERN.finditer(source):
            mask, row, col, on, off = (
                value.strip() for value in entry_match.groups()
            )
            if not mask.startswith("GPAD_"):
                continue
            entries.append((
                path.name,
                Entry(
                    mask=mask,
                    row=integer_expression(row),
                    col=integer_expression(col),
                    on=on,
                    off=off,
                ),
            ))
    return entries


def one(entries: list[Entry], mask: str) -> Entry:
    matches = [entry for entry in entries if entry.mask == mask]
    if len(matches) != 1:
        raise AssertionError(
            f"expected exactly one {mask}, found {len(matches)}"
        )
    return matches[0]


class OledShoulderLayoutReleaseGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.layouts = read_layouts()
        cls.mode_source = (
            MENU / "menu_pad_layouts_mode.cpp"
        ).read_text(encoding="utf-8")
        cls.player_source = (
            MENU / "menu_pad_layouts_player.cpp"
        ).read_text(encoding="utf-8")
        cls.render_source = (
            MENU / "menu_main_display_pad_render.cpp"
        ).read_text(encoding="utf-8")
        cls.status_source = (
            MENU / "menu_main_display_status.cpp"
        ).read_text(encoding="utf-8")

    def test_every_controller_layout_fits_ten_character_cells(self) -> None:
        entries = read_all_source_entries()
        self.assertTrue(entries)
        for source_name, entry in entries:
            with self.subTest(source=source_name, mask=entry.mask, col=entry.col):
                self.assertGreaterEqual(entry.col, 0)
                right_edge = entry.col + (
                    2 * CHARACTER_WIDTH
                    if entry.on in WIDE_LEFT_GLYPHS
                    else CHARACTER_WIDTH
                )
                self.assertLessEqual(
                    right_edge,
                    MAX_CONTROLLER_WIDTH,
                    f"{source_name} {entry.mask} reaches pixel {right_edge}; "
                    f"maximum is {MAX_CONTROLLER_WIDTH}",
                )

    def test_octa_pro_digital_triggers_do_not_render_analog_gauges(self) -> None:
        start = self.render_source.index(
            "bool suppressAnalogTriggerGaugeForFrame"
        )
        body = self.render_source[start : start + 500]
        self.assertIn("frame->controller_type_name[0] == 'O'", body)
        self.assertIn("frame->controller_type_name[4] == 'P'", body)

    def test_all_four_shoulder_layouts_are_inventoried(self) -> None:
        discovered: set[str] = set()
        for name, entries in self.layouts.items():
            if name == "padLayoutPSXNeGcon":
                continue
            shoulder_entries = {
                entry.mask: entry
                for entry in entries
                if entry.mask in SHOULDER_MASKS
                and entry.on == "PAD_SHOULDER_ON"
                and entry.row == 0
                and entry.off == "PAD_SHOULDER_OFF"
            }
            if set(shoulder_entries) == set(SHOULDER_MASKS):
                discovered.add(name)
        self.assertEqual(
            discovered, FOUR_SHOULDER_LAYOUTS | SPECIAL_FOUR_SHOULDER_LAYOUTS
        )

    def test_four_shoulder_layouts_use_half_character_inner_inset(self) -> None:
        for name in sorted(FOUR_SHOULDER_LAYOUTS):
            with self.subTest(layout=name):
                entries = self.layouts[name]
                l1 = one(entries, "GPAD_L1")
                r1 = one(entries, "GPAD_R1")
                l2 = one(entries, "GPAD_L2")
                r2 = one(entries, "GPAD_R2")
                for entry in (l1, r1, l2, r2):
                    self.assertEqual(entry.row, 0)
                    self.assertEqual(entry.on, "PAD_SHOULDER_ON")
                    self.assertEqual(entry.off, "PAD_SHOULDER_OFF")
                self.assertEqual(l2.col % CHARACTER_WIDTH, 0)
                self.assertEqual(r2.col % CHARACTER_WIDTH, 0)
                self.assertEqual(l1.col % CHARACTER_WIDTH, HALF_CHARACTER)
                self.assertEqual(r1.col % CHARACTER_WIDTH, HALF_CHARACTER)
                self.assertLess(l2.col, l1.col)
                self.assertLess(l1.col, r1.col)
                self.assertLess(r1.col, r2.col)
                self.assertGreaterEqual(
                    l1.col - l2.col, CHARACTER_WIDTH + HALF_CHARACTER
                )
                self.assertGreaterEqual(
                    r2.col - r1.col, CHARACTER_WIDTH + HALF_CHARACTER
                )
                self.assertEqual(l2.col + r2.col, l1.col + r1.col)

    def test_wii_classic_places_digital_shoulders_outside_analog_gauges(self) -> None:
        entries = self.layouts["padLayoutWii"]
        self.assertEqual((one(entries, "GPAD_L1").row, one(entries, "GPAD_L1").col), (0, 0))
        self.assertEqual((one(entries, "GPAD_L2").row, one(entries, "GPAD_L2").col), (0, 9))
        self.assertEqual((one(entries, "GPAD_R2").row, one(entries, "GPAD_R2").col), (0, 45))
        self.assertEqual((one(entries, "GPAD_R1").row, one(entries, "GPAD_R1").col), (0, 54))

    def test_three_shoulder_nintendo_layouts_are_inset(self) -> None:
        n64 = self.layouts["padLayoutN64"]
        n64_l = one(n64, "GPAD_L1")
        n64_r = one(n64, "GPAD_R1")
        n64_z = one(n64, "GPAD_L2")
        self.assertEqual((n64_l.row, n64_l.col), (0, 21))
        self.assertEqual((n64_r.row, n64_r.col), (0, 33))
        self.assertEqual(n64_z.on, "PAD_SHOULDER_ON")

        gamecube = self.layouts["padLayoutGC"]
        gc_l = one(gamecube, "GPAD_L2")
        gc_r = one(gamecube, "GPAD_R2")
        gc_z = one(gamecube, "GPAD_SELECT")
        self.assertEqual((gc_l.row, gc_l.col), (0, 9))
        self.assertEqual((gc_r.row, gc_r.col), (0, 45))
        self.assertEqual((gc_z.row, gc_z.col), (1, 54))
        gc_start = one(gamecube, "GPAD_START")
        self.assertEqual((gc_start.row, gc_start.col), (2, 27))

    def assert_type_routes_to(
        self, controller_type: str, layout_name: str
    ) -> None:
        marker = f'std::strcmp(typeName, "{controller_type}")'
        start = self.mode_source.index(marker)
        end = self.mode_source.index("return true;", start)
        self.assertIn(
            f"*layout = {layout_name};",
            self.mode_source[start:end],
        )

    def test_usb_controller_families_route_to_audited_layouts(self) -> None:
        routes = {
            "Wii U Pro": "padLayoutWiiUPro",
            "Switch 2 Pro": "padLayoutUsbSwitch2",
            "Switch Pro": "padLayoutUsbSwitch",
            "N64 NSO": "padLayoutN64",
            "DualShock3": "padLayoutPS3",
            "DualShock4": "padLayoutPS4DualSense",
            "DualSense": "padLayoutPS4DualSense",
            "DS Edge": "padLayoutPS4DualSense",
            "Xbox Classic": "padLayoutUsbXbox",
            "XB One/S": "padLayoutUsbXboxModern",
            "XInput": "padLayoutUsbXInput",
            "8BitDo": "padLayoutUsbModern",
            "Dinput": "padLayoutUsbModern",
        }
        for controller_type, layout_name in routes.items():
            with self.subTest(controller=controller_type):
                self.assert_type_routes_to(controller_type, layout_name)

    def test_xbox_modern_share_is_centered_below_guide(self) -> None:
        entries = self.layouts["padLayoutUsbXboxModern"]
        guide = one(entries, "GPAD_HOME")
        share = one(entries, "GPAD_EXTRA0")
        self.assertEqual(share.row, guide.row + 1)
        self.assertEqual(share.col, guide.col + HALF_CHARACTER)
        self.assertEqual((share.on, share.off), ("PAD_RECT_ON", "PAD_RECT_OFF"))

    def test_original_xbox_uses_duke_face_and_system_layout(self) -> None:
        entries = self.layouts["padLayoutUsbXbox"]
        self.assertFalse(any(entry.mask == "GPAD_HOME" for entry in entries))
        expected = {
            "GPAD_L2": (0, 0),
            "GPAD_R2": (0, 54),
            "GPAD_UP": (1, 12),
            "GPAD_DOWN": (3, 12),
            "GPAD_LEFT": (2, 6),
            "GPAD_RIGHT": (2, 18),
            "GPAD_SELECT": (2, 0),
            "GPAD_START": (3, 6),
            "GPAD_L3": (1, 0),
            "GPAD_R3": (3, 30),
            "GPAD_Y": (1, 42),
            "GPAD_X": (2, 36),
            "GPAD_B": (2, 48),
            "GPAD_A": (3, 42),
            "GPAD_L1": (3, 48),
            "GPAD_R1": (2, 54),
        }
        self.assertEqual(
            {mask: (one(entries, mask).row, one(entries, mask).col)
             for mask in expected},
            expected,
        )
        for mask in ("GPAD_SELECT", "GPAD_START", "GPAD_L1", "GPAD_R1"):
            entry = one(entries, mask)
            self.assertEqual((entry.on, entry.off), ("PAD_FACE_ON", "PAD_FACE_OFF"))

    def test_negcon_uses_bottom_row_horizontal_twist_gauge(self) -> None:
        route = self.player_source.index('std::strcmp(typeName, "neGcon")')
        self.assertIn("padLayoutPSXNeGcon", self.player_source[route : route + 250])
        self.assertIn("kNeGconTwistGaugeRow = 7", self.render_source)
        self.assertIn("kNeGconTwistGaugeWidth = 60", self.render_source)
        self.assertIn(
            "drawCenterOutSignedAxisGauge(frame != nullptr ? frame->LX : 0",
            self.render_source,
        )
        self.assertIn(
            'std::strcmp(port1.controller_type_name, "neGcon") == 0',
            self.status_source,
        )
        self.assertIn(
            'std::strcmp(port2.controller_type_name, "neGcon") == 0',
            self.status_source,
        )
        self.assertIn('std::strcmp(port2_name, "neGcon") == 0', self.status_source)

    def test_classic_modes_and_playstation_types_route_to_audited_layouts(
        self,
    ) -> None:
        for mode, layout_name in (
            ("RZORD_WII", "padLayoutWii"),
            ("RZORD_GAMECUBE", "padLayoutGC"),
            ("RZORD_PSX", "padLayoutPSX"),
        ):
            with self.subTest(mode=mode):
                start = self.mode_source.index(f"case {mode}:")
                self.assertIn(
                    f"*layout = {layout_name};",
                    self.mode_source[start : start + 250],
                )
        for controller_type, layout_name in (
            ("DualShock", "padLayoutPSXDualShock"),
            ("DualShock2", "padLayoutPSXDualShock"),
            ("Digital", "padLayoutPSXDigital"),
        ):
            with self.subTest(controller=controller_type):
                marker = f'std::strcmp(typeName, "{controller_type}")'
                start = self.player_source.index(marker)
                self.assertIn(
                    f"*layout = {layout_name};",
                    self.player_source[start : start + 500],
                )


if __name__ == "__main__":
    unittest.main()
