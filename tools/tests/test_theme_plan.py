import ast
import json
from pathlib import Path
import py_compile
import re
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


LFS_POINTER = b"version https://git-lfs.github.com/spec/v1"


def is_lfs_pointer(path):
    """True when the file is a Git LFS pointer (CI checks out with lfs: false), not the hydrated export."""
    with open(path, "rb") as handle:
        return handle.read(len(LFS_POINTER)) == LFS_POINTER


class ThemePlanTests(unittest.TestCase):
    def test_scripts_compile(self):
        with tempfile.TemporaryDirectory() as out:
            for path in (SCRIPT, ROOT / "Content" / "Python" / "theme_plan.py"):
                py_compile.compile(str(path), cfile=str(Path(out) / (path.name + "c")), doraise=True)

    def test_every_row_matches_its_asset_record(self):
        seen = set()
        pointers = []
        for asset_id, revision, name, folder, kind, prop, index, look in theme_plan.R002:
            record_path = ROOT / "Art" / asset_id / ("r%03d" % revision) / "asset.json"
            record = json.loads(record_path.read_text())
            self.assertEqual(record["asset_id"], asset_id)
            self.assertEqual(record["revision"], revision)
            self.assertEqual(record["unreal_package"], theme_plan.package_path(folder, name), asset_id)
            export = [entry for entry in record["files"] if entry["role"] == "export" and "_LOD" not in entry["path"]]
            self.assertEqual(len(export), 1)
            source = theme_plan.export_path(asset_id, revision, name)
            self.assertEqual(Path(source), ROOT / export[0]["path"], asset_id)
            if is_lfs_pointer(source):
                pointers.append(asset_id)  # the content hash needs the hydrated file; the record shape above is pointer-safe
            else:
                self.assertEqual(theme_plan.sha256_file(source), export[0]["sha256"], asset_id)
            self.assertEqual(record["import_policy"]["settings"]["nanite"], "off, set explicitly on import")
            self.assertNotIn(asset_id, seen)
            seen.add(asset_id)
        self.assertEqual(len(theme_plan.R002), 25)
        if pointers:
            self.skipTest("LFS pointers, not hydrated exports (skipped the content hash of %d): git lfs pull to run it" % len(pointers))

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
        # The Root world joined the allow-list in #149; nothing else did.
        self.assertEqual(theme_plan.requested_replacements("world-root"), {"world-root"})
        self.assertEqual(theme_plan.requested_replacements("world-vigil,world-root"), {"world-vigil", "world-root"})
        self.assertEqual(theme_plan.requested_replacements("all"), set(theme_plan.REPLACEABLE) | set(theme_plan.ROOT_REPLACEABLE) | set(theme_plan.ROSE_REPLACEABLE))
        for bad in ("prop-pillar-b", "hero-fighter", "prop-root-pillar", "world-root-occluders", "tile-root", "world-vigil,tabletop"):
            with self.assertRaises(ValueError, msg=bad):
                theme_plan.requested_replacements(bad)

    def test_script_replaces_only_through_the_guarded_paths(self):
        text = SCRIPT.read_text()
        # A replacement reimports in place (object identity and references survive); deleting an asset would null references.
        self.assertEqual(text.count("library.delete_asset("), 0)
        self.assertIn("reimport_in_place(", text)
        self.assertIn('set_editor_property("replace_existing", True)', text)
        self.assertIn("remove_staging(", text)
        # Deleting a staging folder that no longer exists logs "Could not find directory": every delete is behind an existence check.
        self.assertEqual(text.count("library.delete_directory("), 2)
        self.assertEqual(text.count("library.does_directory_exist(staging)"), 2)
        self.assertIn("Refusing to overwrite", text)
        self.assertIn("only_if_is_dirty=True", text)
        self.assertIn("Unable to save", text)
        self.assertIn("THEME_MATERIAL_KEPT", text)

    def test_export_readers(self):
        if is_lfs_pointer(theme_plan.export_path("world-vigil", 2, "SM_World_Vigil")):
            self.skipTest("LFS pointers, not hydrated exports")
        world = theme_plan.export_path("world-vigil", 2, "SM_World_Vigil")
        old_world = theme_plan.export_path("world-vigil", 1, "SM_World_Vigil")
        self.assertEqual(theme_plan.triangle_count(world), 130502)
        self.assertTrue(theme_plan.has_alpha_mask(world), "r002 world carries a glow mask in COLOR_0.a")
        self.assertFalse(theme_plan.has_alpha_mask(old_world), "r001 world has a constant alpha")
        self.assertTrue(theme_plan.has_alpha_mask(theme_plan.export_path("fx-mist-card", 1, "SM_Fx_MistCard")), "mist opacity varies")
        self.assertFalse(theme_plan.has_alpha_mask("/nonexistent.glb"))

    def test_lods_follow_the_policy(self):
        """Explicit LOD1 and LOD2 for the world, occluders, obstacle variants, arch and the one scatter over 1k triangles; cards and small scatter exempt."""
        with_lods = {"world-vigil", "world-vigil-occluders", "prop-arch-2cell", "scatter-roots"}
        with_lods |= {"prop-%s%s" % (kind, suffix) for kind in ("pillar", "wall", "stones") for suffix in ("", "-b", "-c")}
        pointers = []
        for asset_id, revision, name, folder, kind, prop, index, look in theme_plan.R002:
            files = theme_plan.lod_exports(asset_id, revision, name)
            recorded = theme_plan.record_lods(asset_id, revision)
            self.assertEqual([entry[0] for entry in recorded], [1, 2] if asset_id in with_lods else [], asset_id)
            self.assertEqual([Path(path).relative_to(ROOT).as_posix() for path in files], [entry[1] for entry in recorded], asset_id)
            if is_lfs_pointer(theme_plan.export_path(asset_id, revision, name)) or any(is_lfs_pointer(file) for file in files):
                pointers.append(asset_id)
                continue
            triangles = theme_plan.triangle_count(theme_plan.export_path(asset_id, revision, name))
            self.assertEqual(asset_id in with_lods, triangles > 1000 or asset_id.startswith("prop-"), asset_id)
            previous = triangles
            for (lod, path, digest), file in zip(recorded, files):
                self.assertEqual(theme_plan.sha256_file(file), digest, path)
                count = theme_plan.triangle_count(file)
                self.assertLess(count, previous, path)
                self.assertGreaterEqual(count / triangles, 0.24 if lod == 2 else 0.48, path)
                self.assertLessEqual(count / triangles, 0.40 if lod == 2 else 0.52, path)
                previous = count
        if pointers:
            self.skipTest("LFS pointers, not hydrated exports (skipped triangle and hash checks of %d): git lfs pull to run them" % len(pointers))

    def test_a_reimport_must_report_the_package(self):
        package = "/Game/DnDDiceRoll/Theme/Props/SM_Prop_Wall"
        texture = "/Game/DnDDiceRoll/Theme/Textures/T_Prop_Wall"
        theme_plan.check_import_result([package + ".SM_Prop_Wall"], package, "wall.glb")
        theme_plan.check_import_result([texture + ".T_Prop_Wall"], texture, "wall.png")
        # A failed mesh import: the original object is still loaded, but nothing was imported.
        with self.assertRaisesRegex(RuntimeError, "did not report"):
            theme_plan.check_import_result([], package, "wall.glb")
        # A failed texture import, and one that reports some other asset instead.
        with self.assertRaisesRegex(RuntimeError, "did not report"):
            theme_plan.check_import_result([], texture, "wall.png")
        with self.assertRaisesRegex(RuntimeError, "did not report"):
            theme_plan.check_import_result([package + ".SM_Prop_Wall"], texture, "wall.png")
        # Extra assets are an error too.
        with self.assertRaisesRegex(RuntimeError, "also created"):
            theme_plan.check_import_result([package + ".SM_Prop_Wall", package + "_Mat.MI"], package, "wall.glb")

    def test_import_over_uses_the_check_and_lod_slots_are_repaired(self):
        text = SCRIPT.read_text()
        self.assertIn("theme_plan.check_import_result(", text)
        self.assertIn("repair_lod_slots(", text)
        self.assertIn("set_lod_material_slot(", text)
        self.assertIn("M_FigureSilhouette", text)
        # The silhouette material carries a graph revision that ensure_material checks.
        revision = re.search(r'^FIGURE_SILHOUETTE_REVISION = "(figure-silhouette-\d+)"$', text, re.M)
        self.assertIsNotNone(revision)
        self.assertIn("figure_silhouette_is_current, FIGURE_SILHOUETTE_REVISION)", text)
        overlay = (ROOT / "Content" / "Python" / "create_board_overlay_material.py").read_text()
        self.assertIn('set_editor_property("disable_depth_test", True)', overlay)
        # board-overlay-3 (#144) adds the per-instance ring colour on top of the depth-test change.
        self.assertIn('GRAPH_REVISION = "board-overlay-3"', overlay)
        self.assertIn("MaterialExpressionPerInstanceCustomData3Vector", overlay)
        # Every grid style shares one component: opacity is read from custom float 3 on top of the Opacity parameter.
        self.assertIn("MaterialExpressionPerInstanceCustomData, -500, 400, const_default_value=1.0, data_index=3", overlay)

    def test_instanced_part_materials_follow_the_revision_pattern(self):
        text = SCRIPT.read_text()
        # The master tint is per-instance custom data behind a bumped graph revision (#144).
        self.assertIn('MASTER_GRAPH_REVISION = "dice-theme-7"', text)
        self.assertIn("MaterialExpressionPerInstanceCustomData,", text)
        # The effect material is created once through ensure_material with its own revision; a rerun keeps a current one.
        self.assertRegex(text, r'(?m)^EFFECT_GRAPH_REVISION = "dice-effect-\d+"$')
        self.assertIn("ensure_material(EFFECT_MATERIAL, build_effect_material, effect_is_current, EFFECT_GRAPH_REVISION)", text)
        # The instancing usage flag is written only when different, on every material an instanced group draws.
        self.assertIn('set_if_different(material, "used_with_instanced_static_meshes", True)', text)
        self.assertIn("for instanced in (master, occluder_parent, card_parent, effect_parent):", text)
        # The runtime loads the effect material by the name the script creates.
        view = (ROOT / "Source" / "DnDDiceRoll" / "Private" / "Presentation" / "ExpeditionBattleView.cpp").read_text()
        self.assertIn("Materials/M_DiceRollEffect.M_DiceRollEffect", view)

    def test_arches_are_for_pillars_and_walls_only(self):
        header = (ROOT / "Source" / "DnDDiceRoll" / "Public" / "Presentation" / "VigilKit.h").read_text()
        self.assertIn("return Recipe == 1 || Recipe == 2;", header)


class RootKitPlanTests(unittest.TestCase):
    """The Root of the World kit r002 rows (#149): the same checks as the Vigil rows, written to be pointer-safe for LFS."""

    def test_every_root_row_matches_its_asset_record(self):
        seen = set()
        pointers = []
        for asset_id, revision, name, folder, kind, prop, index, look in theme_plan.ROOT_R002:
            record = json.loads((ROOT / "Art" / asset_id / ("r%03d" % revision) / "asset.json").read_text())
            self.assertEqual(record["asset_id"], asset_id)
            self.assertEqual(record["revision"], revision)
            self.assertEqual(record["unreal_package"], theme_plan.package_path(folder, name), asset_id)
            export = [entry for entry in record["files"] if entry["role"] == "export" and "_LOD" not in entry["path"]]
            self.assertEqual(len(export), 1)
            source = theme_plan.export_path(asset_id, revision, name)
            self.assertEqual(Path(source), ROOT / export[0]["path"], asset_id)
            if is_lfs_pointer(source):
                pointers.append(asset_id)
            else:
                self.assertEqual(theme_plan.sha256_file(source), export[0]["sha256"], asset_id)
            self.assertEqual(record["import_policy"]["settings"]["nanite"], "off, set explicitly on import")
            self.assertEqual(len(look), 5, asset_id)
            self.assertNotIn(asset_id, seen)
            seen.add(asset_id)
        self.assertEqual(len(theme_plan.ROOT_R002), 25)
        # No id appears in both kits, and no package is claimed twice.
        self.assertFalse(seen & {row[0] for row in theme_plan.R002})
        packages = [theme_plan.package_path(row[3], row[2]) for row in theme_plan.R002 + theme_plan.ROOT_R002]
        self.assertEqual(len(packages), len(set(packages)))
        if pointers:
            self.skipTest("LFS pointers, not hydrated exports (skipped the content hash of %d): git lfs pull to run it" % len(pointers))

    def test_only_the_root_world_replaces_an_r001_package(self):
        old = r001_packages()
        colliding = {row[0] for row in theme_plan.ROOT_R002 if theme_plan.package_path(row[3], row[2]) in old}
        self.assertEqual(colliding, set(theme_plan.ROOT_REPLACEABLE))
        self.assertEqual(set(theme_plan.ROOT_METRIC_GATE), set(theme_plan.ROOT_REPLACEABLE))
        self.assertEqual(set(theme_plan.ROOT_REPLACEABLE), {"world-root"})
        # The Root obstacles are new packages: the r001 pillar, wall and stones belong to the Vigil.
        for row in theme_plan.ROOT_R002:
            if row[0] != "world-root":
                self.assertEqual(row[1], 1, row[0])
        # The new texture packages (T_<mesh without SM_>) are new too: only the world's atlas exists.
        textures = {"T_" + row[2][3:] for row in theme_plan.ROOT_R002 if row[0] not in theme_plan.ROOT_REPLACEABLE}
        existing = {"T_" + package.rsplit("/", 1)[1][3:] for package in old}
        self.assertFalse(textures & existing)

    def test_root_array_slots_are_complete_and_properties_exist_in_the_theme(self):
        header = (ROOT / "Source" / "DnDDiceRoll" / "Public" / "Presentation" / "DiceRollTheme.h").read_text()
        slots = {}
        for row in theme_plan.ROOT_R002:
            prop, index = row[5], row[6]
            self.assertIn(pascal(prop), header)
            if index is not None:
                self.assertNotIn(index, slots.setdefault(prop, set()))
                slots[prop].add(index)
        self.assertEqual({prop: len(found) for prop, found in slots.items()}, theme_plan.ROOT_ARRAY_SIZES)
        for prop, count in theme_plan.ROOT_ARRAY_SIZES.items():
            self.assertEqual(slots[prop], set(range(count)))
        self.assertIn("bRootMetricKit", header)
        # The script sets the Root flag through the same table-driven path as the Vigil's.
        text = SCRIPT.read_text()
        self.assertIn('"flag": "root_metric_kit"', text)
        self.assertIn("theme_plan.ROOT_R002", text)
        self.assertIn("import_kit(kit, theme, parents, replacements)", text)
        # The data-asset order is the record order: variants A, B, C by file name, scatter as the README lists it.
        self.assertEqual([row[0] for row in theme_plan.ROOT_R002 if row[5] == "root_scatter"], [
            "scatter-root-" + word for word in ("mushrooms", "leaves", "twigs", "tendrils", "ferns", "pebbles", "glowmoss", "husks")])

    def test_root_lods_follow_the_policy(self):
        """Explicit LOD1 and LOD2 for the world, occluders, obstacle variants and arch; scatter, mist cards and light shafts exempt."""
        with_lods = {"world-root", "world-root-occluders", "prop-root-arch-2cell"}
        with_lods |= {"prop-root-%s%s" % (kind, suffix) for kind in ("pillar", "wall", "stones") for suffix in ("", "-b", "-c")}
        pointers = []
        for asset_id, revision, name, folder, kind, prop, index, look in theme_plan.ROOT_R002:
            files = theme_plan.lod_exports(asset_id, revision, name)
            recorded = theme_plan.record_lods(asset_id, revision)
            self.assertEqual([entry[0] for entry in recorded], [1, 2] if asset_id in with_lods else [], asset_id)
            self.assertEqual([Path(path).relative_to(ROOT).as_posix() for path in files], [entry[1] for entry in recorded], asset_id)
            if is_lfs_pointer(theme_plan.export_path(asset_id, revision, name)) or any(is_lfs_pointer(file) for file in files):
                pointers.append(asset_id)
                continue
            triangles = theme_plan.triangle_count(theme_plan.export_path(asset_id, revision, name))
            self.assertEqual(asset_id in with_lods, triangles > 1000 or asset_id.startswith("prop-"), asset_id)
            previous = triangles
            for (lod, path, digest), file in zip(recorded, files):
                self.assertEqual(theme_plan.sha256_file(file), digest, path)
                count = theme_plan.triangle_count(file)
                self.assertLess(count, previous, path)
                # The Root world's LOD2 stops at 40 % (25 % failed the silhouette gate, README); the others follow #145's 25 %.
                self.assertGreaterEqual(count / triangles, 0.24 if lod == 2 else 0.48, path)
                self.assertLessEqual(count / triangles, 0.40 if lod == 2 else 0.52, path)
                previous = count
        if pointers:
            self.skipTest("LFS pointers, not hydrated exports (skipped triangle and hash checks of %d): git lfs pull to run them" % len(pointers))

    def test_kinds_and_looks_follow_the_materials(self):
        kinds = {row[0]: row[4] for row in theme_plan.ROOT_R002}
        self.assertEqual(kinds["world-root-occluders"], theme_plan.OCCLUDER)
        for asset_id, kind in kinds.items():
            if asset_id.startswith("fx-root-"):
                self.assertEqual(kind, theme_plan.CARD, asset_id)
            elif asset_id != "world-root-occluders":
                self.assertEqual(kind, theme_plan.LIT, asset_id)
        for row in theme_plan.ROOT_R002:
            roughness, rim, fill, glow, self_lit = row[7]
            self.assertTrue(0.0 <= roughness <= 1.0 and 0.0 <= fill <= 1.0, row[0])
            # No rim (characters only) and no blanket self-light: glow only through the vertex-alpha mask.
            self.assertEqual((rim, self_lit), (0.0, 0.0), row[0])
            self.assertEqual(glow, 1.0 if row[4] != theme_plan.CARD else 0.0, row[0])

    def test_instance_tints_name_kit_rows_and_stay_neutral_or_darker(self):
        ids = {row[0] for row in theme_plan.R002 + theme_plan.ROOT_R002 + theme_plan.VAULT_R002}
        for asset_id, tint in theme_plan.INSTANCE_TINTS.items():
            self.assertIn(asset_id, ids)
            self.assertEqual(len(tint), 3)
            self.assertTrue(all(0.0 < channel <= 1.0 for channel in tint), asset_id)
        # The recorded default: neutral, so a plain run changes no Vigil instance and the lever is explicit for the Root world.
        self.assertEqual(theme_plan.INSTANCE_TINTS["world-root"], (1.0, 1.0, 1.0))
        text = SCRIPT.read_text()
        self.assertIn("tint=theme_plan.INSTANCE_TINTS.get(asset_id)", text)
        self.assertIn("set_material_instance_vector_parameter_value(instance, \"Color\", wanted)", text)

    def test_kit_tags_are_distinct_and_the_script_writes_the_kits_own(self):
        self.assertEqual(theme_plan.KIT_TAGS, ("vigil-kit-r002", "root-kit-r002", "vault-kit-r002", "overview-r002", "vigil-kit-r002-rose"))
        text = SCRIPT.read_text()
        self.assertIn('library.set_metadata_tag(mesh, theme_plan.REVISION_TAG, row_tag)', text)
        self.assertIn('row_tag = theme_plan.ROW_TAGS.get(asset_id, kit["tag"])', text)
        # The r001 pass must not reset a replaced Root world: any kit tag counts.
        self.assertIn("def kit_tagged(mesh, tag=None):", text)

    def test_root_arches_follow_the_pillar_and_wall_rule_of_the_vigil(self):
        header = (ROOT / "Source" / "DnDDiceRoll" / "Public" / "Presentation" / "VigilKit.h").read_text()
        self.assertIn("return Recipe == 1 || Recipe == 2;", header)
        view = (ROOT / "Source" / "DnDDiceRoll" / "Private" / "Presentation" / "ExpeditionBattleView.cpp").read_text()
        self.assertIn("BuildKitObstacles(Encounter, Kind, DecorationSeed, VigilKit::ArchesFor(Kind), Kit)", view)
        self.assertIn("Theme->KitFor(KitKindFor(BoardTheme))", view)
        # A kit-less Root board keeps today's behaviour: Vigil variants (when on) without arches, then r001.
        self.assertIn("BuildKitObstacles(Encounter, Kind, Seed, false, Theme->KitFor(EDiceRollKit::Vigil))", view)


if __name__ == "__main__":
    unittest.main()


class VaultKitPlanTests(unittest.TestCase):
    """The Vault kit r002 rows (#149): a third kit through the same table-driven import, all new packages, pointer-safe for LFS."""

    def test_every_vault_row_matches_its_asset_record(self):
        seen = set()
        pointers = []
        for asset_id, revision, name, folder, kind, prop, index, look in theme_plan.VAULT_R002:
            record = json.loads((ROOT / "Art" / asset_id / ("r%03d" % revision) / "asset.json").read_text())
            self.assertEqual(record["asset_id"], asset_id)
            self.assertEqual(record["revision"], revision)
            self.assertEqual(record["unreal_package"], theme_plan.package_path(folder, name), asset_id)
            export = [entry for entry in record["files"] if entry["role"] == "export" and "_LOD" not in entry["path"]]
            self.assertEqual(len(export), 1)
            source = theme_plan.export_path(asset_id, revision, name)
            self.assertEqual(Path(source), ROOT / export[0]["path"], asset_id)
            if is_lfs_pointer(source):
                pointers.append(asset_id)
            else:
                self.assertEqual(theme_plan.sha256_file(source), export[0]["sha256"], asset_id)
            self.assertEqual(record["import_policy"]["settings"]["nanite"], "off, set explicitly on import")
            self.assertEqual(len(look), 5, asset_id)
            self.assertNotIn(asset_id, seen)
            seen.add(asset_id)
        self.assertEqual(len(theme_plan.VAULT_R002), 25)
        # No id appears in another kit, and no package is claimed twice.
        self.assertFalse(seen & {row[0] for row in theme_plan.R002 + theme_plan.ROOT_R002})
        packages = [theme_plan.package_path(row[3], row[2]) for row in theme_plan.R002 + theme_plan.ROOT_R002 + theme_plan.VAULT_R002]
        self.assertEqual(len(packages), len(set(packages)))
        if pointers:
            self.skipTest("LFS pointers, not hydrated exports (skipped the content hash of %d): git lfs pull to run it" % len(pointers))

    def test_the_vault_replaces_nothing(self):
        old = r001_packages()
        colliding = {row[0] for row in theme_plan.VAULT_R002 if theme_plan.package_path(row[3], row[2]) in old}
        self.assertEqual(colliding, set())
        self.assertEqual(theme_plan.VAULT_METRIC_GATE, ("world-vault",))
        self.assertFalse({row[0] for row in theme_plan.VAULT_R002} & set(theme_plan.ALL_REPLACEABLE))
        textures = {"T_" + row[2][3:] for row in theme_plan.VAULT_R002}
        self.assertFalse(textures & {"T_" + package.rsplit("/", 1)[1][3:] for package in old})

    def test_vault_array_slots_are_complete_and_properties_exist_in_the_theme(self):
        header = (ROOT / "Source" / "DnDDiceRoll" / "Public" / "Presentation" / "DiceRollTheme.h").read_text()
        slots = {}
        for row in theme_plan.VAULT_R002:
            prop, index = row[5], row[6]
            self.assertIn(pascal(prop), header)
            if index is not None:
                self.assertNotIn(index, slots.setdefault(prop, set()))
                slots[prop].add(index)
        self.assertEqual({prop: len(found) for prop, found in slots.items()}, theme_plan.VAULT_ARRAY_SIZES)
        for prop, count in theme_plan.VAULT_ARRAY_SIZES.items():
            self.assertEqual(slots[prop], set(range(count)))
        # The theme's own completeness test asks for the same counts.
        source = (ROOT / "Source" / "DnDDiceRoll" / "Private" / "Presentation" / "DiceRollTheme.cpp").read_text()
        self.assertIn("AllSet(VaultScatter, 8)", source)
        self.assertIn("AllSet(VaultMistCards, 3)", source)
        self.assertIn("AllSet(VaultLightShafts, 2)", source)
        self.assertIn("bVaultMetricKit", header)
        text = SCRIPT.read_text()
        self.assertIn('"flag": "vault_metric_kit"', text)
        self.assertIn("theme_plan.VAULT_R002", text)
        self.assertEqual([row[0] for row in theme_plan.VAULT_R002 if row[5] == "vault_scatter"], [
            "scatter-vault-" + word for word in ("bones", "skulls", "candles", "rubble", "chains", "urns", "relics", "crystals")])

    def test_vault_lods_follow_the_policy(self):
        """Explicit LODs for the world, obstacle variants and arch (LOD1 and LOD2), the occluders LOD1 only; scatter, cards and shafts exempt."""
        two = {"world-vault", "prop-vault-arch-2cell"} | {"prop-vault-%s%s" % (kind, suffix) for kind in ("pillar", "wall", "stones") for suffix in ("", "-b", "-c")}
        one = {"world-vault-occluders"}
        pointers = []
        for asset_id, revision, name, folder, kind, prop, index, look in theme_plan.VAULT_R002:
            files = theme_plan.lod_exports(asset_id, revision, name)
            recorded = theme_plan.record_lods(asset_id, revision)
            self.assertEqual([entry[0] for entry in recorded], [1, 2] if asset_id in two else [1] if asset_id in one else [], asset_id)
            self.assertEqual([str(Path(path).relative_to(ROOT)) for path in files], [entry[1] for entry in recorded], asset_id)
            if is_lfs_pointer(theme_plan.export_path(asset_id, revision, name)) or any(is_lfs_pointer(file) for file in files):
                pointers.append(asset_id)
                continue
            triangles = theme_plan.triangle_count(theme_plan.export_path(asset_id, revision, name))
            previous = triangles
            for (lod, path, digest), file in zip(recorded, files):
                self.assertEqual(theme_plan.sha256_file(file), digest, path)
                count = theme_plan.triangle_count(file)
                self.assertLess(count, previous, path)
                if asset_id in one:
                    # Pinned sharp-edge vertices stop the collapse at about 54 % (README); LOD2 would equal LOD1, so there is none.
                    self.assertTrue(0.50 <= count / triangles <= 0.60, path)
                else:
                    self.assertGreaterEqual(count / triangles, 0.24 if lod == 2 else 0.48, path)
                    self.assertLessEqual(count / triangles, 0.40 if lod == 2 else 0.52, path)
                previous = count
        if pointers:
            self.skipTest("LFS pointers, not hydrated exports (skipped triangle and hash checks of %d): git lfs pull to run them" % len(pointers))

    def test_vault_kinds_looks_and_tints(self):
        kinds = {row[0]: row[4] for row in theme_plan.VAULT_R002}
        self.assertEqual(kinds["world-vault-occluders"], theme_plan.OCCLUDER)
        for asset_id, kind in kinds.items():
            if asset_id.startswith("fx-vault-"):
                self.assertEqual(kind, theme_plan.CARD, asset_id)
            elif asset_id != "world-vault-occluders":
                self.assertEqual(kind, theme_plan.LIT, asset_id)
        for row in theme_plan.VAULT_R002:
            roughness, rim, fill, glow, self_lit = row[7]
            self.assertTrue(0.0 <= roughness <= 1.0 and 0.0 <= fill <= 1.0, row[0])
            self.assertEqual((rim, self_lit), (0.0, 0.0), row[0])
            self.assertEqual(glow, 0.0 if row[4] == theme_plan.CARD else 0.25 if row[0] == "world-vault-occluders" else 1.0, row[0])
        self.assertEqual(theme_plan.INSTANCE_TINTS["world-vault"], (1.0, 1.0, 1.0))
        self.assertEqual(theme_plan.INSTANCE_TINTS["world-vault-occluders"], (1.0, 1.0, 1.0))
        self.assertEqual(theme_plan.VAULT_TAG, "vault-kit-r002")

    def test_the_vault_flag_follows_the_world_tag_and_the_theme_falls_back_to_the_vigil(self):
        text = SCRIPT.read_text()
        self.assertIn("THEME_VAULT_COMPLETE", text)
        view = (ROOT / "Source" / "DnDDiceRoll" / "Private" / "Presentation" / "ExpeditionBattleView.cpp").read_text()
        self.assertIn("Kit.Kind == EDiceRollKit::Vault", view)
        self.assertIn("Theme->KitFor(EDiceRollKit::Vigil)", view)
        theme = (ROOT / "Source" / "DnDDiceRoll" / "Private" / "Presentation" / "DiceRollTheme.cpp").read_text()
        self.assertIn("if (Kind == EDiceRollKit::Vault && !VaultKitComplete()) { Kind = EDiceRollKit::Vigil; }", theme)


class OverviewPlanTests(unittest.TestCase):
    """The overview set r002 (#149, the last part): the war table and three dioramas as a fourth kit, and the Vigil occluders with the high-resolution rose."""

    def test_every_overview_row_matches_its_asset_record_and_replaces_nothing(self):
        old = r001_packages()
        pointers = []
        for asset_id, revision, name, folder, kind, prop, index, look in theme_plan.OVERVIEW_R002:
            record = json.loads((ROOT / "Art" / asset_id / ("r%03d" % revision) / "asset.json").read_text())
            self.assertEqual((record["asset_id"], record["revision"]), (asset_id, revision))
            self.assertEqual(record["unreal_package"], theme_plan.package_path(folder, name), asset_id)
            self.assertNotIn(theme_plan.package_path(folder, name), old, "the r001 table and dioramas stay as the fallback")
            export = [entry for entry in record["files"] if entry["role"] == "export"]
            self.assertEqual(len(export), 1)
            source = theme_plan.export_path(asset_id, revision, name)
            self.assertEqual(Path(source), ROOT / export[0]["path"], asset_id)
            if is_lfs_pointer(source):
                pointers.append(asset_id)
            else:
                self.assertEqual(theme_plan.sha256_file(source), export[0]["sha256"], asset_id)
            self.assertEqual(kind, theme_plan.LIT)
            self.assertEqual(theme_plan.record_lods(asset_id, revision), [], "one low-count mesh: no LODs")
            self.assertIsNone(index)
        self.assertEqual(len(theme_plan.OVERVIEW_R002), 4)
        ids = [row[0] for row in theme_plan.OVERVIEW_R002]
        self.assertFalse(set(ids) & set(theme_plan.ALL_REPLACEABLE))
        self.assertEqual(set(theme_plan.OVERVIEW_METRIC_GATE), set(ids))
        taken = {row[0] for row in theme_plan.R002 + theme_plan.ROOT_R002 + theme_plan.VAULT_R002}
        self.assertFalse(taken & set(ids))
        packages = [theme_plan.package_path(row[3], row[2]) for row in theme_plan.R002 + theme_plan.ROOT_R002 + theme_plan.VAULT_R002 + theme_plan.OVERVIEW_R002]
        self.assertEqual(len(packages), len(set(packages)))
        if pointers:
            self.skipTest("LFS pointers, not hydrated exports (skipped the content hash of %d): git lfs pull to run it" % len(pointers))

    def test_overview_properties_exist_in_the_theme_and_the_script_runs_the_kit(self):
        header = (ROOT / "Source" / "DnDDiceRoll" / "Public" / "Presentation" / "DiceRollTheme.h").read_text()
        for row in theme_plan.OVERVIEW_R002:
            self.assertIn(pascal(row[5]), header)
        self.assertIn("bOverviewR002", header)
        text = SCRIPT.read_text()
        self.assertIn('"flag": "overview_r002"', text)
        self.assertIn("theme_plan.OVERVIEW_R002", text)
        self.assertIn("THEME_OVERVIEW_COMPLETE", text)
        self.assertEqual(theme_plan.OVERVIEW_TAG, "overview-r002")

    def test_the_vigil_rose_replace_is_allow_listed_tagged_and_matches_its_record(self):
        self.assertEqual(set(theme_plan.ROSE_REPLACEABLE), {"world-vigil-occluders"})
        self.assertEqual(theme_plan.requested_replacements("world-vigil-occluders"), {"world-vigil-occluders"})
        # It is not part of the world's metric gate (that stays the four r001 packages) and it is a revision of a package that already exists.
        self.assertNotIn("world-vigil-occluders", theme_plan.METRIC_GATE)
        self.assertEqual(theme_plan.ROW_TAGS, {"world-vigil-occluders": theme_plan.ROSE_TAG})
        self.assertIn(theme_plan.ROSE_TAG, theme_plan.KIT_TAGS)
        row = next(row for row in theme_plan.R002 if row[0] == "world-vigil-occluders")
        self.assertEqual(row[1], 2)
        record = json.loads((ROOT / "Art" / "world-vigil-occluders" / "r002" / "asset.json").read_text())
        self.assertEqual(record["revision"], 2)
        self.assertEqual(record["unreal_package"], theme_plan.package_path(row[3], row[2]))
        self.assertEqual([entry[0] for entry in theme_plan.record_lods("world-vigil-occluders", 2)], [1, 2])
        source = theme_plan.export_path("world-vigil-occluders", 2, row[2])
        if is_lfs_pointer(source):
            self.skipTest("LFS pointer, not the hydrated export: git lfs pull to run it")
        # The atlas it embeds is the 4096 x 2048 one with the rose, and nothing else about the file's contract moved.
        header, binary = theme_plan.read_glb(source)
        view = header["bufferViews"][header["images"][0]["bufferView"]]
        png = binary[view.get("byteOffset", 0):view.get("byteOffset", 0) + view["byteLength"]]
        self.assertEqual(int.from_bytes(png[16:20], "big"), 4096)
        self.assertEqual(int.from_bytes(png[20:24], "big"), 2048)
        self.assertTrue(theme_plan.has_alpha_mask(source))
