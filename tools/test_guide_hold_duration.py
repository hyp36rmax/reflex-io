#!/usr/bin/env python3
"""Host-side source regression checks for independent Guide Hold feature."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def source(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

class GuideHoldTests(unittest.TestCase):
    def test_storage_retains_flash_record_size(self):
        settings = source("firmware/core/settings_store.h")
        registry = source("firmware/core/settings_registry.h")
        self.assertIn("sizeof(GlobalSettingsRecord) == 22", settings)
        self.assertIn("SettingId::GuideHoldTime", registry)
        self.assertIn("GUIDE_HOLD_TIME_DEFAULT = 4", settings)
        self.assertIn("GUIDE_HOLD_TIME_MASK = 0x0E", settings)
        self.assertIn("encodeCaptureHotkey", settings)
        self.assertIn("encodeGuideHoldTime", settings)

    def test_classic2usb_uses_global_hold_others_retain_baseline(self):
        hotkeys = source("firmware/core/controller_runtime_hotkeys.cpp")
        self.assertIn("hotkeyHoldMsForSetting(menu_guide_hold_time)", hotkeys)
        self.assertIn("#if defined(PRODUCT_CLASSIC2USB)", hotkeys)
        self.assertIn("1000u", hotkeys)
        self.assertNotIn("kHoldStartHomeMs = 1000", hotkeys)

    def test_webhid_feature_detect_and_persist(self):
        protocol = source("firmware/output/usb/webhid/webhid_protocol.h")
        report = source("firmware/output/usb/webhid/webhid_settings_reports.cpp")
        self.assertIn("WEBHID_SETTINGS_GUIDE_HOLD_MARKER   0x80", protocol)
        self.assertIn("WEBHID_SETTING_GUIDE_HOLD           50", protocol)
        self.assertIn("case WEBHID_SETTING_GUIDE_HOLD:", report)
        self.assertIn("saveSystemSettingByte(SettingId::GuideHoldTime, value)", report)
        self.assertIn("value >= 1 && value <= 6", report)

    def test_adapt_web_support_and_compatibility(self):
        html = source("web/Adapt.html")
        self.assertIn('id="setting-guide-hold"', html)
        self.assertIn('id="settings-guide-hold"', html)
        self.assertIn("activeAdaptProduct.id === 'Classic2USB'", html)
        self.assertIn("settings.guideHoldTime !== null", html)
        self.assertIn("[WEBHID_SETTING_GUIDE_HOLD, guideHold]", html)
        self.assertIn("item.id === -WEBHID_SETTING_GUIDE_HOLD", html)

if __name__ == "__main__":
    unittest.main(verbosity=2)
