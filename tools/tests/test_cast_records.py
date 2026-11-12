import ast
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CAST = ROOT / "Art" / "rig-pilot" / "cast" / "scripts"
R001 = ROOT / "Art" / "rig-pilot" / "r001" / "scripts"
STDLIB_OR_BLENDER = {"bpy", "bmesh", "mathutils", "numpy", "json", "math", "os", "sys", "re", "tempfile", "subprocess", "hashlib", "pathlib", "colorsys", "statistics", "importlib"}
LOADED_FILE = re.compile(r"os\.path\.join\((HERE|C\.R001)\s*,\s*'([\w]+\.py)'\)")


def local_module(name):
    for folder in (CAST, R001):
        if (folder / f"{name}.py").is_file():
            return folder / f"{name}.py"
    return None


def dependencies(path):
    """The repository files a Blender script loads: its local imports and the files it executes (exec(compile(open(...)))), transitively."""
    seen, queue = set(), [path]
    while queue:
        current = queue.pop()
        if current in seen:
            continue
        seen.add(current)
        text = current.read_text()
        for node in ast.walk(ast.parse(text)):
            names = [alias.name.split(".")[0] for alias in node.names] if isinstance(node, ast.Import) else [node.module.split(".")[0]] if isinstance(node, ast.ImportFrom) and node.module else []
            for name in names:
                found = local_module(name) if name not in STDLIB_OR_BLENDER else None
                if found:
                    queue.append(found)
        for base, name in LOADED_FILE.findall(text):
            queue.append((CAST if base == "HERE" else R001) / name)
    return seen


class BatchFourRecordsTest(unittest.TestCase):
    def test_the_records_list_every_loaded_source(self):
        text = (CAST / "make_records4.py").read_text()
        namespace = {}
        # Only the three constants are needed: evaluate their assignments without running the script (it writes records).
        for name in ("CAST", "CAST_SCRIPTS", "R001_SCRIPTS", "SHARED"):
            node = next(n for n in ast.parse(text).body if isinstance(n, ast.Assign) and getattr(n.targets[0], "id", "") == name)
            namespace[name] = eval(compile(ast.Expression(node.value), "make_records4", "eval"), dict(namespace))
        listed = {(ROOT / path).resolve() for path in namespace["SHARED"]}
        for entry in ("build4.py", "review4.py"):
            missing = sorted(str(path.relative_to(ROOT)) for path in dependencies(CAST / entry) if path.resolve() not in listed)
            self.assertEqual(missing, [], f"{entry} loads sources the asset records do not hash")
        for path in listed:
            self.assertTrue(path.is_file(), str(path))

    def test_every_copyable_blender_command_exits_non_zero_on_an_exception(self):
        for path in [*CAST.glob("*4.py"), CAST / "palette_tiles.py", CAST / "palette_preview.py", ROOT / "Art" / "rig-pilot" / "cast" / "README_BATCH4.md"]:
            for line in path.read_text().splitlines():
                if re.search(r"\bblender\b.* -b\b|\bblender -b\b", line) and "--python" in line:
                    self.assertIn("--python-exit-code 1", line, f"{path.name}: {line.strip()[:100]}")


if __name__ == "__main__":
    unittest.main()
