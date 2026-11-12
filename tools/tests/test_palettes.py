import ast
import importlib.util
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CAST = ROOT / "Art" / "rig-pilot" / "cast"
PAL = CAST / "palettes"
CATALOG = ROOT / "Source" / "DnDDiceRoll" / "Private" / "Party" / "BuildCatalog.cpp"
SCRIPT = ROOT / "Content" / "Python" / "import_dark_fantasy_theme.py"
CPP = ROOT / "Source" / "DnDDiceRoll" / "Private" / "Presentation" / "ExpeditionPalette.cpp"


def generator():
    spec = importlib.util.spec_from_file_location("palettes_generator", CAST / "scripts" / "palettes.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def catalog_ids():
    ids = set(re.findall(r'TEXT\("([a-z]+\.[a-z\-]+)"\)', CATALOG.read_text()))
    return ids | {"barbarian." + path for path in ("berserker", "totem-warrior", "beast", "wild-magic")}


class PaletteTableTests(unittest.TestCase):
    """The subclass palette table (#150 batch 4): authored data, its generated outputs and the material that reads them."""

    def setUp(self):
        self.gen = generator()
        self.src = json.loads((PAL / "palettes.json").read_text())
        self.res = json.loads((PAL / "palettes.resolved.json").read_text())

    def test_generated_outputs_are_current(self):
        resolved = self.gen.resolve()
        self.assertEqual(json.loads(json.dumps(resolved)), self.res, "palettes.resolved.json is stale: run palettes.py")
        self.assertEqual((PAL / "swatches.html").read_text(), self.gen.html(resolved))
        self.assertEqual(self.gen.INL.read_text(), self.gen.inl(resolved), "ExpeditionPaletteTable.inl is stale: run palettes.py")

    def test_every_catalog_build_has_exactly_one_row(self):
        self.assertEqual(set(self.src["builds"]), catalog_ids())
        self.assertEqual(len(self.src["builds"]), 70)

    def test_classes_are_the_thirteen_party_classes_and_slots_use_real_tiles(self):
        regions = self.gen.REGIONS
        self.assertEqual(len(self.src["classes"]), 13)
        tiles = json.loads((PAL / "tiles.json").read_text())
        for key, cls in self.src["classes"].items():
            self.assertEqual(set(cls["tiles"]), {"A", "B", "C", "G", "X"}, key)
            for slot, names in cls["tiles"].items():
                self.assertTrue(names and all(name in regions for name in names), key + " " + slot)
                self.assertTrue(all(tiles[key][name]["area_share"] > 0 for name in names if slot != "G"), key + " " + slot + " covers a tile the mesh never uses")
            # The accent must be visible at the default zoom: at least a tenth of the surface.
            share = sum(tiles[key][name]["area_share"] for name in cls["tiles"]["X"])
            self.assertGreaterEqual(share, 0.10, key + " accent tiles cover too little of the figure")
            # A build slot never claims a tile that another build slot of the class also claims (the colours would fight).
            claimed = [name for slot in ("A", "B", "C") for name in cls["tiles"][slot]]
            self.assertEqual(len(claimed), len(set(claimed)), key + " slots overlap")

    def test_variants_keep_the_class_brightness(self):
        for bid, build in self.res["builds"].items():
            for slot, value in build["slots"].items():
                self.assertGreaterEqual(value["luma_ratio"], 0.8, bid + " " + slot + " is darker than its class")
                h, s, ratio = value["spec"]
                self.assertTrue(0 <= h < 360 and 0 <= s <= 1 and ratio > 0, bid)
        for key, cls in self.res["classes"].items():
            for slot in "ABCGX":
                self.assertGreater(cls["slots"][slot]["reference"], 0, key + " " + slot)
        for hue, accent in zip(self.src["accent"]["hues"], self.res["classes"]["fighter"]["accent"]):
            self.assertRegex(accent["hex"], r"^#[0-9a-f]{6}$")
        self.assertEqual(len(self.src["accent"]["hues"]), 4)

    def test_the_fighter_and_artificer_defaults_are_corrected(self):
        default = self.res["classes"]["fighter"]["default"]["slots"]
        self.assertEqual(list(default), ["A"])
        champion = self.res["builds"]["fighter.champion"]["slots"]
        self.assertEqual(default["A"]["hex"], champion["A"]["hex"])
        # The warm silver is lighter than the old blue steel (#6890ad-ish blue at luma 0.47).
        self.assertGreater(default["A"]["spec"][2], 1.05)
        for key, cls in self.res["classes"].items():
            if key not in ("fighter", "artificer"):   # the two whose class colours merged with the Vault stone take a corrected default
                self.assertEqual(cls["default"]["slots"], {}, key + " default is the class's own look")

    def test_tile_order_matches_the_atlas_and_the_material(self):
        tree = ast.parse((ROOT / "Art" / "rig-pilot" / "r001" / "scripts" / "rigkit.py").read_text())
        regions = next(ast.literal_eval(node.value) for node in tree.body if isinstance(node, ast.Assign) and getattr(node.targets[0], "id", "") == "REGIONS")
        self.assertEqual(regions, self.gen.REGIONS)
        script = SCRIPT.read_text()
        self.assertIn('PALETTE_SLOTS = ("A", "B", "C", "G", "X")', script)
        self.assertIn('"PalMask" + slot', script)
        self.assertIn('"Pal" + slot', script)
        cpp = CPP.read_text()
        self.assertIn('TEXT("PalMask%s")', cpp)
        self.assertIn('TEXT("Pal%s")', cpp)
        # Masks are scalars read as float: every bit pattern must be exactly representable.
        for cls in self.res["classes"].values():
            for slot in cls["slots"].values():
                self.assertLess(slot["mask"], 1 << 13)

    def test_the_master_keeps_every_existing_input_when_masks_are_zero(self):
        script = SCRIPT.read_text()
        # The palette block sits between the texture sample and the vertex-colour multiply; with all masks 0 each lerp returns its A input, the texture.
        self.assertIn("palette = palette_block(material, tex, -3000, -1800)", script)
        self.assertIn('scalar(material, "PalMask" + slot, 0.0,', script)
        # The Power node's input pins are "Base" and "Exp": "Exponent" connects nothing and silently leaves the exponent at 2 (every tile then reads the same bit).
        self.assertIn('power, "Exp")', script)
        self.assertNotIn('power, "Exponent")', script)
        self.assertIn("mel.connect_material_expressions(current, current_out, blend, \"A\")", script)


if __name__ == "__main__":
    unittest.main()
