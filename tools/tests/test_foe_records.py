import ast
import json
import unittest
from pathlib import Path

from test_cast_records import CAST, ROOT, dependencies
from dev import FOE_LOOKS_PATTERN

FOES = ("scout", "archer", "skirmisher", "brute", "warden")


class FoeBatchRecordsTest(unittest.TestCase):
    def listed(self):
        text = (CAST / "make_records_foes.py").read_text()
        node = next(n for n in ast.parse(text).body if isinstance(n, ast.Assign) and getattr(n.targets[0], "id", "") == "SHARED")
        namespace = {"CAST": "Art/rig-pilot/cast"}
        return {(ROOT / path).resolve() for path in eval(compile(ast.Expression(node.value), "make_records_foes", "eval"), namespace)}

    def test_the_records_list_every_loaded_source(self):
        listed = self.listed()
        for entry in ("build_foes.py", "review_foes.py"):
            missing = sorted(str(path.relative_to(ROOT)) for path in dependencies(CAST / entry) if path.resolve() not in listed)
            self.assertEqual(missing, [], f"{entry} loads sources the foe records do not hash")
        for path in listed:
            self.assertTrue(path.is_file(), str(path))

    def test_every_blender_command_exits_non_zero_on_an_exception(self):
        for path in (*CAST.glob("*_foes.py"), ROOT / "Art" / "rig-pilot" / "cast" / "README_FOES.md"):
            for line in path.read_text().splitlines():
                if "blender" in line and " -b" in line and "--python" in line:
                    self.assertIn("--python-exit-code 1", line, f"{path.name}: {line.strip()[:100]}")

    def test_every_record_hashes_the_current_sources(self):
        import hashlib
        for key in FOES:
            record = json.loads((ROOT / "Art" / f"sk-foe-{key}" / "r001" / "asset.json").read_text())
            for entry in record["files"]:
                path = ROOT / entry["path"]
                if path.is_file() and path.stat().st_size > 1024 * 1024:
                    continue                       # an LFS blob in a checkout without the binaries is not re-read here
                data = path.read_bytes()
                if data.startswith(b"version https://git-lfs"):
                    continue
                self.assertEqual(hashlib.sha256(data).hexdigest(), entry["sha256"], f"{key}: {entry['path']} changed after the record was written")

    def test_foe_looks_pattern_takes_only_the_rig_keys(self):
        for good in ("Scout", "Skirmisher,Brute,Archer", "Brute,Warden,Archer"):
            self.assertTrue(FOE_LOOKS_PATTERN.fullmatch(good), good)
        for bad in ("", "scout", "Scout,", "Scout Brute", "Boss", "Scout;Brute", "Archer,Archer2"):
            self.assertFalse(FOE_LOOKS_PATTERN.fullmatch(bad), bad)


if __name__ == "__main__":
    unittest.main()
