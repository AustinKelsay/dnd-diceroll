import ast
import json
from pathlib import Path
import py_compile
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Content" / "Python"))
import theme_plan  # noqa: E402  (pure Python half of the theme import; no `unreal`)

SCRIPT = ROOT / "Content" / "Python" / "import_dark_fantasy_theme.py"


def r001_packages():
    """The package paths of the r001 ASSETS table, read from the import script without running it."""
    tree = ast.parse(SCRIPT.read_text())
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(getattr(t, "id", "") == "ASSETS" for t in node.targets):
            return {theme_plan.package_path(row[2], row[1]) for row in ast.literal_eval(node.value)}
    raise AssertionError("ASSETS not found")


def pascal(snake):
    return "".join(part.capitalize() for part in snake.split("_"))


class ThemePlanTests(unittest.TestCase):
    def test_scripts_compile(self):
        with tempfile.TemporaryDirectory() as out:
            for path in (SCRIPT, ROOT / "Content" / "Python" / "theme_plan.py"):
                py_compile.compile(str(path), cfile=str(Path(out) / (path.name + "c")), doraise=True)

    def test_every_row_matches_its_asset_record(self):
        seen = set()
        for asset_id, revision, name, folder, kind, prop, index, look in theme_plan.R002:
            record_path = ROOT / "Art" / asset_id / ("r%03d" % revision) / "asset.json"
            record = json.loads(record_path.read_text())
            self.assertEqual(record["asset_id"], asset_id)
            self.assertEqual(record["revision"], revision)
            self.assertEqual(record["unreal_package"], theme_plan.package_path(folder, name), asset_id)
            export = [entry for entry in record["files"] if entry["role"] == "export"]
            self.assertEqual(len(export), 1)
            source = theme_plan.export_path(asset_id, revision, name)
            self.assertEqual(Path(source), ROOT / export[0]["path"], asset_id)
            self.assertEqual(theme_plan.sha256_file(source), export[0]["sha256"], asset_id)
            self.assertEqual(record["import_policy"]["settings"]["nanite"], "off, set explicitly on import")
            self.assertNotIn(asset_id, seen)
            seen.add(asset_id)
        self.assertEqual(len(theme_plan.R002), 25)

    def test_new_names_never_collide_with_r001_and_only_four_replace(self):
        old = r001_packages()
        colliding = {row[0] for row in theme_plan.R002 if theme_plan.package_path(row[3], row[2]) in old}
        self.assertEqual(colliding, set(theme_plan.REPLACEABLE))
        self.assertEqual(set(theme_plan.METRIC_GATE), set(theme_plan.REPLACEABLE))

    def test_array_slots_are_complete_and_property_names_exist_in_the_theme(self):
        header = (ROOT / "Source" / "DnDDiceRoll" / "Public" / "Presentation" / "DiceRollTheme.h").read_text()
        slots = {}
        for row in theme_plan.R002:
            prop, index = row[5], row[6]
            self.assertIn(pascal(prop), header)
            if index is not None:
                self.assertNotIn(index, slots.setdefault(prop, set()))
                slots[prop].add(index)
        self.assertEqual({prop: len(found) for prop, found in slots.items()}, theme_plan.ARRAY_SIZES)
        for prop, count in theme_plan.ARRAY_SIZES.items():
            self.assertEqual(slots[prop], set(range(count)))
        self.assertIn("bMetricKit", header)

    def test_replacement_allow_list(self):
        self.assertEqual(theme_plan.requested_replacements(None), set())
        self.assertEqual(theme_plan.requested_replacements(""), set())
        self.assertEqual(theme_plan.requested_replacements("world-vigil, prop-wall"), {"world-vigil", "prop-wall"})
        self.assertEqual(theme_plan.requested_replacements("all"), set(theme_plan.REPLACEABLE))
        for bad in ("prop-pillar-b", "hero-fighter", "world-root", "world-vigil,tabletop"):
            with self.assertRaises(ValueError, msg=bad):
                theme_plan.requested_replacements(bad)

    def test_script_replaces_only_through_the_guarded_paths(self):
        text = SCRIPT.read_text()
        self.assertEqual(text.count("library.delete_asset("), 2)  # mesh and texture of a requested replacement, in one function
        self.assertIn("Refusing to overwrite", text)
        self.assertIn("only_if_is_dirty=True", text)
        self.assertIn("Unable to save", text)
        self.assertIn("THEME_MATERIAL_KEPT", text)

    def test_export_readers(self):
        world = theme_plan.export_path("world-vigil", 2, "SM_World_Vigil")
        old_world = theme_plan.export_path("world-vigil", 1, "SM_World_Vigil")
        self.assertEqual(theme_plan.triangle_count(world), 130502)
        self.assertTrue(theme_plan.has_alpha_mask(world), "r002 world carries a glow mask in COLOR_0.a")
        self.assertFalse(theme_plan.has_alpha_mask(old_world), "r001 world has a constant alpha")
        self.assertTrue(theme_plan.has_alpha_mask(theme_plan.export_path("fx-mist-card", 1, "SM_Fx_MistCard")), "mist opacity varies")
        self.assertFalse(theme_plan.has_alpha_mask("/nonexistent.glb"))

    def test_lod_discovery(self):
        self.assertEqual(theme_plan.lod_exports("world-vigil", 2, "SM_World_Vigil"), [], "r002 authors no LOD exports")


if __name__ == "__main__":
    unittest.main()
