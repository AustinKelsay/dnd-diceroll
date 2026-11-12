import ast
import json
import os
import re
import runpy
import sys
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "Content" / "Python" / "import_dark_fantasy_theme.py"
BUNDLE = ("SK_Hero_Fighter,SK_Hero_Wizard,SK_Foe_Raider,SK_Hero_Rogue,SK_Hero_Cleric,SK_Hero_Barbarian,SK_Hero_Monk,SK_Hero_Paladin,SK_Hero_Ranger,SK_Hero_Sorcerer,SK_Hero_Warlock,SK_Hero_Druid,SK_Foe_Scout,SK_Foe_Archer,SK_Foe_Skirmisher,SK_Foe_Brute,SK_Foe_Warden,"
          "SKEL_Humanoid,animations")
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


def script_table(name):
    for node in ast.parse(SCRIPT.read_text()).body:
        if isinstance(node, ast.Assign) and any(getattr(target, "id", "") == name for target in node.targets):
            return ast.literal_eval(node.value)
    raise AssertionError(name + " not found")


def glb_animations(path):
    data = path.read_bytes()
    if data[:4] != b"glTF":
        return None                      # an LFS pointer in a checkout without the binaries
    length = int.from_bytes(data[12:16], "little")
    return [a.get("name") for a in json.loads(data[20:20 + length]).get("animations", [])]


class ClipOverrideTableTest(unittest.TestCase):
    """CLIP_OVERRIDES (per-class clip overrides, #150) names real rows, real shared clips and real actions in the class's glb."""

    def test_table_matches_the_rig_table_the_clips_and_the_glbs(self):
        overrides = script_table("CLIP_OVERRIDES")
        rig = {row[2]: row for row in script_table("RIG")}
        clips = script_table("FIRST_CLIPS") + [script_table("BRACE_CLIP")]
        self.assertTrue(overrides, "the table is not empty")
        for key, entries in overrides.items():
            self.assertIn(key, rig, key + " is a rig row")
            for shared, (package, action) in entries.items():
                self.assertIn(shared, clips, shared + " is a shared clip")
                self.assertTrue(package.startswith("A_") and package != "A_Humanoid_" + shared, package)
                names = glb_animations(ROOT / "Art" / rig[key][0] / "r001" / (rig[key][1] + ".glb"))
                if names is None:
                    self.skipTest("the glb binaries are not downloaded")
                self.assertIn(action, names, action + " is exported in " + rig[key][1])
                self.assertIn(shared, names, "the shared clip stays in the same glb")


if __name__ == "__main__":
    unittest.main()
