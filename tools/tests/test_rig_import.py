import os
import re
import runpy
import sys
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "Content" / "Python" / "import_dark_fantasy_theme.py"
BUNDLE = "SK_Hero_Fighter,SK_Hero_Wizard,SK_Foe_Raider,SKEL_Humanoid,animations"
# Calls the script makes while loading, before its main block: lookups only.
READS = {"get_editor_subsystem", "load_module", "AssetToolsHelpers.get_asset_tools"}


class RigReimportRefusalTest(unittest.TestCase):
    """THEME_RIG_REIMPORT is refused before the script touches any asset (no master rebuild, settings or imports)."""

    def run_script(self, request):
        fake = mock.MagicMock(name="unreal")
        with mock.patch.dict(sys.modules, {"unreal": fake}), mock.patch.dict(os.environ, {"THEME_RIG_REIMPORT": request}):
            with self.assertRaises(RuntimeError) as raised:
                runpy.run_path(str(SCRIPT), run_name="__main__")
        calls = {name for name, _, _ in fake.mock_calls}
        unexpected = sorted(name for name in calls if name not in READS)
        self.assertEqual(unexpected, [], "engine calls before the refusal")
        return str(raised.exception)

    def test_full_bundle_request_is_refused_before_any_mutation(self):
        if not (ROOT / "Art" / "rig-pilot").is_dir():
            self.skipTest("Art/rig-pilot is not in this checkout")
        self.assertIn("never deletes or replaces rig assets", self.run_script(BUNDLE))

    def test_partial_and_unknown_requests_are_refused_before_any_mutation(self):
        self.assertIn("whole rig bundle", self.run_script("SK_Hero_Fighter"))
        self.assertIn("unknown assets", self.run_script("SK_Nobody"))

    def test_refusal_precedes_the_main_block(self):
        text = SCRIPT.read_text()
        refusal = re.search(r"\n(\s*)if REIMPORT:\n\1    check_allow_list\(\)\n\1    refuse_reimport\(\)\n", text)
        self.assertIsNotNone(refusal, "the early refusal block")
        self.assertLess(refusal.start(), text.index("master = master_material()"))


if __name__ == "__main__":
    unittest.main()
