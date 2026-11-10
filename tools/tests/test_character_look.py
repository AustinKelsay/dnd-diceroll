import ast
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Content" / "Python"))
import theme_plan  # noqa: E402  (pure Python half of the theme import; no `unreal`)

SCRIPT = ROOT / "Content" / "Python" / "import_dark_fantasy_theme.py"

# Graphics overhaul #148: every character material reads against the dark ground with a stronger Rim and Fill.
CHARACTER_RIM = 1.6
CHARACTER_FILL = 1.25
# The material instances a headless import must change, and only these (names are the MI_* packages in /Game/DnDDiceRoll/Theme/Materials).
CHANGED = {
    "MI_Hero_Fighter", "MI_Hero_Rogue", "MI_Hero_Cleric", "MI_Hero_Wizard",
    "MI_Foe_Raider", "MI_Foe_Scout", "MI_Foe_Archer",
    "MI_SK_Hero_Fighter", "MI_SK_Hero_Wizard", "MI_SK_Foe_Raider",
}
# Roughness, glow and self-light of each character, unchanged by #148.
CHARACTER_OTHERS = {
    "SM_Hero_Fighter": (0.55, 1.0, 0.0), "SM_Hero_Rogue": (0.6, 1.0, 0.0), "SM_Hero_Cleric": (0.6, 1.0, 0.0),
    "SM_Hero_Wizard": (0.65, 1.0, 0.0), "SM_Foe_Raider": (0.6, 1.0, 0.0), "SM_Foe_Scout": (0.6, 1.0, 0.0),
    "SM_Foe_Archer": (0.6, 1.0, 0.0),
    "SK_Hero_Fighter": (0.55, 1.0, 0.0), "SK_Hero_Wizard": (0.65, 1.0, 0.0), "SK_Foe_Raider": (0.6, 1.0, 0.0),
}
# Rim and Fill of every other r001 row, before and after #148.
NON_CHARACTER = {
    "SM_World_Root": (0.0, 0.0), "SM_World_Vigil": (0.0, 0.0), "SM_Tile_Root": (0.0, 0.0), "SM_Tile_Vigil": (0.0, 0.0),
    "SM_Prop_Stones": (0.0, 0.05), "SM_Prop_Pillar": (0.0, 0.05), "SM_Prop_Wall": (0.0, 0.05), "SM_Prop_Brazier": (0.0, 0.05),
    "SM_Prop_Flame": (0.0, 0.0), "SM_Prop_Banner": (0.0, 0.05), "SM_Prop_Cage": (0.0, 0.05),
    "SM_Tabletop": (0.0, 0.0), "SM_Diorama_Root": (0.0, 0.0), "SM_Diorama_Vigil": (0.0, 0.0),
}


def table(name):
    tree = ast.parse(SCRIPT.read_text())
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(getattr(target, "id", "") == name for target in node.targets):
            return ast.literal_eval(node.value)
    raise AssertionError(name + " not found")


def static_rows():
    """name -> (folder, (roughness, rim, fill, glow, self_lit)) of the r001 ASSETS table."""
    return {row[1]: (row[2], tuple(row[4:])) for row in table("ASSETS")}


def rig_rows():
    return {row[1]: row[3] for row in table("RIG")}


class CharacterLookTest(unittest.TestCase):
    def test_every_character_carries_the_stronger_rim_and_fill(self):
        rows = {name: look for name, (folder, look) in static_rows().items() if folder == "Characters"}
        rows.update(rig_rows())
        self.assertEqual(set(rows), set(CHARACTER_OTHERS), "the character roster changed: update this test and the docs/assets.md change set")
        for name, (roughness, rim, fill, glow, self_lit) in rows.items():
            self.assertEqual(rim, CHARACTER_RIM, name + " Rim")
            self.assertEqual(fill, CHARACTER_FILL, name + " Fill")
            self.assertEqual((roughness, glow, self_lit), CHARACTER_OTHERS[name], name + " roughness, glow and self-light are unchanged")

    def test_non_character_rows_are_unchanged(self):
        others = {name: (look[1], look[2]) for name, (folder, look) in static_rows().items() if folder != "Characters"}
        self.assertEqual(others, NON_CHARACTER)
        for row in theme_plan.R002:
            self.assertLessEqual(row[7][1], 0.0, row[2] + " Rim (r002 props and worlds have none)")
            self.assertLessEqual(row[7][2], 0.05, row[2] + " Fill")

    def test_the_change_set_is_exactly_the_character_instances(self):
        names = {"MI_" + name[3:] for name, (folder, _) in static_rows().items() if folder == "Characters"}
        names |= {"MI_" + name for name in rig_rows()}
        self.assertEqual(names, CHANGED)

    def test_the_import_writes_a_scalar_only_when_it_differs(self):
        text = SCRIPT.read_text()
        self.assertIn("abs(mel.get_material_instance_scalar_parameter_value(instance, parameter) - value) > 1e-6", text)
        self.assertIn("if changed:\n        mel.update_material_instance(instance)", text)


if __name__ == "__main__":
    unittest.main()
