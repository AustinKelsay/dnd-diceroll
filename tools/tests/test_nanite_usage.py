"""Pure authoring-policy tests; no Unreal import or binary fixture mutation."""
import ast
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import Mock

SCRIPT = Path(__file__).resolve().parents[2] / "Content/Python/import_dark_fantasy_theme.py"


class NaniteUsageTests(unittest.TestCase):
    def setUp(self):
        self.tree = ast.parse(SCRIPT.read_text())
        node = next(n for n in self.tree.body if isinstance(n, ast.FunctionDef) and n.name == "ensure_nanite_usage")
        self.material = Mock()
        self.material.get_name.return_value = "M_DiceRollTheme"
        self.mel = Mock()
        self.mel.recompile_material.return_value = []
        self.log = Mock()
        unreal = SimpleNamespace(MaterialUsage=SimpleNamespace(MATUSAGE_NANITE="Nanite"), log=self.log)
        ns = {"unreal": unreal, "mel": self.mel}
        exec(compile(ast.Module(body=[node], type_ignores=[]), str(SCRIPT), "exec"), ns)
        self.ensure = ns["ensure_nanite_usage"]

    def test_missing_usage_is_enabled_and_explicitly_recompiled(self):
        self.mel.has_material_usage.side_effect = [False, True]
        self.assertTrue(self.ensure(self.material))
        self.mel.set_base_material_usage.assert_called_once_with(self.material, "Nanite", True)
        self.mel.recompile_material.assert_called_once_with(self.material)
        self.log.assert_called_once()

    def test_current_usage_does_not_dirty_or_recompile(self):
        self.mel.has_material_usage.return_value = True
        self.assertFalse(self.ensure(self.material))
        self.mel.set_base_material_usage.assert_not_called()
        self.mel.recompile_material.assert_not_called()
        self.log.assert_not_called()

    def test_compile_failure_is_not_reported_as_success(self):
        self.mel.has_material_usage.return_value = False
        self.mel.recompile_material.return_value = ["compile failed"]
        with self.assertRaisesRegex(RuntimeError, "Material compile reported errors"):
            self.ensure(self.material)
        self.log.assert_not_called()

    def test_failed_flag_readback_is_not_reported_as_success(self):
        self.mel.has_material_usage.return_value = False
        with self.assertRaisesRegex(RuntimeError, "Nanite usage was not enabled"):
            self.ensure(self.material)
        self.log.assert_not_called()

    def test_main_checks_usage_between_master_load_and_save(self):
        main = next(n for n in self.tree.body if isinstance(n, ast.FunctionDef) and n.name == "main")
        calls = [n.value for n in main.body if isinstance(n, ast.Expr) and isinstance(n.value, ast.Call)]
        calls = [n for n in calls if isinstance(n.func, ast.Name) and n.func.id == "ensure_nanite_usage"]
        self.assertEqual(len(calls), 1)
        self.assertEqual(ast.dump(calls[0].args[0]), ast.dump(ast.Name(id="master", ctx=ast.Load())))
        master_index = next(i for i, n in enumerate(main.body) if isinstance(n, ast.Assign) and isinstance(n.value, ast.Call) and isinstance(n.value.func, ast.Name) and n.value.func.id == "master_material")
        usage_index = next(i for i, n in enumerate(main.body) if isinstance(n, ast.Expr) and n.value is calls[0])
        save_index = next(i for i, n in enumerate(main.body) if isinstance(n, ast.If) and isinstance(n.test, ast.UnaryOp) and isinstance(n.test.operand, ast.Call) and isinstance(n.test.operand.func, ast.Attribute) and n.test.operand.func.attr == "save_directory")
        self.assertLess(master_index, usage_index)
        self.assertLess(usage_index, save_index)


if __name__ == "__main__":
    unittest.main()
