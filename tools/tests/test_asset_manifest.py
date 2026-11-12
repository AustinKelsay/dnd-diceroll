import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from asset_manifest import inspect_record, plan_revision


class AssetManifestTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.first = self.record(1)

    def record(self, revision, export=b"export-v1"):
        files = []
        for role, name, content in [("source", "source.txt", b"editable"),
                                    ("export", "mesh.obj", export),
                                    ("reference", "style.txt", b"approved reference")]:
            path = f"Art/camp-chest/r{revision}/{name}"
            target = self.root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
            files.append({"role": role, "path": path, "sha256": hashlib.sha256(content).hexdigest()})
        return {"schema_version": 1, "asset_id": "camp-chest", "revision": revision,
                "unreal_package": "/Game/DnDDiceRoll/Props/SM_CampChest", "issue": "#16",
                "provenance": {"author": "fixture", "license": "original", "origin": "test fixture"},
                "recipe": {"method": "handmade", "style": "approved reference"},
                "tools": {"fixture": "1"}, "files": files,
                "import_policy": {"name": "static-prop", "version": 1, "settings": {"scale": 1}},
                "validation": {"state": "pending", "evidence": []}}

    def save(self, record, name="record.json"):
        path = self.root / name
        path.write_text(json.dumps(record))
        return path

    def test_initial_repeat_and_revision_are_read_only(self):
        first_path = self.save(self.first)
        first = inspect_record(first_path, self.root)
        self.assertEqual(plan_revision(first)["action"], "new")
        self.assertEqual(plan_revision(first, first)["action"], "unchanged")
        second_path = self.save(self.record(2, b"weathered export"), "r2.json")
        before = {p.relative_to(self.root): p.read_bytes() for p in self.root.rglob("*") if p.is_file()}
        second = inspect_record(second_path, self.root)
        result = plan_revision(second, first)
        self.assertEqual(result["action"], "reimport")
        self.assertEqual(result["unreal_package"], first["unreal_package"])
        self.assertEqual(before, {p.relative_to(self.root): p.read_bytes() for p in self.root.rglob("*") if p.is_file()})

    def test_same_export_and_changed_policy_or_metadata(self):
        second = self.record(2)
        self.assertEqual(plan_revision(second, self.first)["action"], "metadata_only")
        second["import_policy"]["version"] = 2
        self.assertEqual(plan_revision(second, self.first)["action"], "reimport")
        second["import_policy"]["version"] = 1
        second["import_policy"]["settings"]["scale"] = 2
        self.assertEqual(plan_revision(second, self.first)["action"], "reimport")

    def test_refuses_identity_target_and_revision_rewrites(self):
        for key, value in [("asset_id", "other"), ("unreal_package", "/Game/DnDDiceRoll/Other"),
                           ("revision", 1), ("revision", 3)]:
            with self.subTest(key=key, value=value):
                second = self.record(2)
                second[key] = value
                with self.assertRaises(ValueError):
                    plan_revision(second, self.first)

    def test_refuses_stale_missing_and_lfs_pointer_files(self):
        target = self.root / self.first["files"][0]["path"]
        for content in [b"overwritten", b"version https://git-lfs.github.com/spec/v1\noid sha256:abc\n"]:
            target.write_bytes(content)
            with self.assertRaises(ValueError):
                inspect_record(self.save(self.first), self.root)
        target.unlink()
        with self.assertRaises(ValueError):
            inspect_record(self.save(self.first), self.root)

    def test_refuses_path_escape_and_symlink(self):
        outside = self.root.parent / (self.root.name + "-outside.txt")
        outside.write_text("outside")
        self.addCleanup(outside.unlink)
        link = self.root / "Art/link.txt"
        # Windows without Developer Mode cannot create native symlinks. In that
        # case model the resolver boundary; other hosts still exercise a real link.
        from contextlib import nullcontext
        from unittest.mock import patch
        scope = nullcontext()
        try:
            link.symlink_to(outside)
        except OSError as exc:
            if getattr(exc, "winerror", None) != 1314:
                raise
            original = Path.resolve
            def resolve(path, *args, **kwargs):
                return outside if path == link else original(path, *args, **kwargs)
            scope = patch.object(Path, "resolve", resolve)
        with scope:
            for path in ["../outside", str(outside), "Art/../record.json", "Art/link.txt", "Saved/file", "Art\\bad"]:
                candidate = copy.deepcopy(self.first)
                candidate["files"][0]["path"] = path
                with self.subTest(path=path), self.assertRaises(ValueError):
                    inspect_record(self.save(candidate), self.root)

    def test_refuses_malformed_contracts(self):
        candidates = []
        for key, value in [("schema_version", 2), ("revision", True), ("tools", {}),
                           ("unreal_package", "/Game/Other/SM_Chest"), ("recipe", {}), ("files", [])]:
            candidate = copy.deepcopy(self.first)
            candidate[key] = value
            candidates.append(candidate)
        candidate = copy.deepcopy(self.first)
        candidate["validation"] = {"state": "passed", "evidence": []}
        candidates.append(candidate)
        candidate = copy.deepcopy(self.first)
        candidate["files"].append(candidate["files"][0])
        candidates.append(candidate)
        for candidate in candidates:
            with self.subTest(candidate=candidate), self.assertRaises(ValueError):
                inspect_record(self.save(candidate), self.root)

    def test_cli_refuses_duplicate_keys_nan_and_bad_json(self):
        script = Path(__file__).resolve().parents[1] / "asset_manifest.py"
        for content in ['{"schema_version":1,"schema_version":2}', '{"cost":NaN}', '{"cost":1e999}', '[1]', '{']:
            path = self.root / "bad.json"
            path.write_text(content)
            result = subprocess.run([sys.executable, str(script), str(path), "--root", str(self.root)],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn("Traceback", result.stderr)

    def test_cli_validates_previous_files_before_planning(self):
        first = self.save(self.first)
        second = self.save(self.record(2, b"v2"), "r2.json")
        script = Path(__file__).resolve().parents[1] / "asset_manifest.py"
        command = [sys.executable, str(script), str(second), "--previous", str(first), "--root", str(self.root)]
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["action"], "reimport")
        (self.root / self.first["files"][0]["path"]).write_text("lost old source")
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)

    def test_existing_revision_requires_previous_record(self):
        with self.assertRaises(ValueError):
            plan_revision(self.record(2))

    def test_export_filename_changes_and_sidecar_changes_require_reimport(self):
        second = self.record(2)
        second["files"][1]["path"] = "Art/camp-chest/r2/renamed.obj"
        self.assertEqual(plan_revision(second, self.first)["action"], "reimport")
        second = self.record(2)
        second["files"].append({"role": "export", "path": "Art/camp-chest/r2/material.txt", "sha256": "a" * 64})
        self.assertEqual(plan_revision(second, self.first)["action"], "reimport")

    def test_nested_json_types_cannot_hide_policy_changes_or_record_rewrites(self):
        self.first["import_policy"]["settings"] = {"nested": [{"enabled": 1}]}
        first = inspect_record(self.save(self.first), self.root)
        second = self.record(2)
        second["import_policy"]["settings"] = {"nested": [{"enabled": True}]}
        second = inspect_record(self.save(second, "r2.json"), self.root)
        self.assertEqual(plan_revision(second, first)["action"], "reimport")
        rewritten = copy.deepcopy(first)
        rewritten["recipe"]["value"] = [1]
        first["recipe"]["value"] = [True]
        with self.assertRaises(ValueError):
            plan_revision(rewritten, first)

    def test_texture_subfolder_associations_survive_revision_directory_changes(self):
        second = self.record(2)
        for record, colors in [(self.first, (b"wood", b"metal")), (second, (b"metal", b"wood"))]:
            for folder, content in zip(("wood", "metal"), colors):
                path = f"Art/camp-chest/r{record['revision']}/{folder}/albedo.png"
                target = self.root / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(content)
                record["files"].append({"role": "export", "path": path,
                                        "sha256": hashlib.sha256(content).hexdigest()})
        first = inspect_record(self.save(self.first), self.root)
        second = inspect_record(self.save(second, "r2.json"), self.root)
        self.assertEqual(plan_revision(second, first)["action"], "reimport")
        for entry, old in zip(second["files"], first["files"]):
            entry["sha256"] = old["sha256"]
        self.assertEqual(plan_revision(second, first)["action"], "metadata_only")

    def test_only_documented_revision_containers_can_move_without_reimport(self):
        second = self.record(2)
        second["files"][1]["path"] = "Art/camp-chest/r002/mesh.obj"
        self.assertEqual(plan_revision(second, self.first)["action"], "metadata_only")
        second["files"][1]["path"] = "Art/other/r2/mesh.obj"
        self.assertEqual(plan_revision(second, self.first)["action"], "reimport")


if __name__ == "__main__":
    unittest.main()
