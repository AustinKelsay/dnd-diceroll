"""Real disposable Git repositories exercise contribution checks without Unreal."""
from contextlib import contextmanager
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from contribute import check_repository, editor_problems, preflight


class ContributionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="contribution space ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.environment = patch.dict(os.environ, {
            "GIT_CONFIG_GLOBAL": os.devnull, "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_AUTHOR_NAME": "Test", "GIT_AUTHOR_EMAIL": "test@example.invalid",
            "GIT_COMMITTER_NAME": "Test", "GIT_COMMITTER_EMAIL": "test@example.invalid",
        })
        self.environment.start()
        self.addCleanup(self.environment.stop)
        self.git("init", "-b", "feature/test")
        self.git("remote", "add", "origin", "https://github.com/AustinKelsay/expedition-rpg.git")
        for name in ("tools/versions.json", ".mcp.json", ".codex/config.toml", "Config/DefaultEditorPerProjectUserSettings.ini"):
            self.write(name, (Path(__file__).resolve().parents[2] / name).read_bytes())
        self.write("README.md", b"[Guide](docs/guide.md)\n")
        self.write("docs/guide.md", b"A guide.\n")
        self.write(".gitattributes", b"*.umap filter=lfs diff=lfs merge=lfs -text\n")
        self.write(".gitignore", b".local/\n")
        self.git("add", ".")
        self.git("commit", "-m", "fixture")
        self.git("update-ref", "refs/remotes/origin/staging", "HEAD")

    def git(self, *args, data=None):
        return subprocess.run(["git", "-C", str(self.root), *args], input=data,
                              check=True, capture_output=True).stdout

    def write(self, name, data):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def install_lfs(self):
        if not shutil.which("git-lfs"):
            self.skipTest("Git LFS is required for local-readiness integration tests")
        self.git("lfs", "install", "--local")

    def asset(self):
        pointer = b"version https://git-lfs.github.com/spec/v1\noid sha256:" + b"a" * 64 + b"\nsize 4\n"
        self.write("Content/map.umap", b"game")
        oid = self.git("hash-object", "-w", "--stdin", data=pointer).decode().strip()
        self.git("update-index", "--add", "--cacheinfo", f"100644,{oid},Content/map.umap")
        self.git("commit", "-m", "asset")
        return pointer

    @contextmanager
    def unchanged(self):
        before_index = (self.root / ".git/index").read_bytes()
        hooks = self.root / ".git/hooks"
        before_hooks = {p.name: p.read_bytes() for p in hooks.iterdir() if p.is_file()}
        before = {p.relative_to(self.root): p.read_bytes() for p in self.root.rglob("*")
                  if p.is_file() and ".git" not in p.relative_to(self.root).parts}
        yield
        self.assertEqual(before_index, (self.root / ".git/index").read_bytes())
        self.assertEqual(before_hooks, {p.name: p.read_bytes() for p in hooks.iterdir() if p.is_file()})
        self.assertEqual(before, {p.relative_to(self.root): p.read_bytes() for p in self.root.rglob("*")
                                 if p.is_file() and ".git" not in p.relative_to(self.root).parts})

    def test_repository_accepts_pointer_storage_without_hydration(self):
        pointer = self.asset()
        self.write("Content/map.umap", pointer)
        with self.unchanged():
            self.assertEqual(check_repository(self.root), [])

    def test_repository_rejects_raw_lfs_blob(self):
        oid = self.git("hash-object", "-w", "--stdin", data=b"raw asset").decode().strip()
        self.git("update-index", "--add", "--cacheinfo", f"100644,{oid},Content/map.umap")
        self.assertTrue(any("LFS pointer" in p for p in check_repository(self.root)))

    def test_repository_rejects_generated_files_and_tokens_without_printing_value(self):
        self.write("Saved/output.log", b"output")
        self.write("Config/Generated.ini", b"SecurityToken=private-value\n")
        self.git("add", "Saved", "Config")
        problems = "\n".join(check_repository(self.root))
        self.assertIn("Saved/output.log", problems)
        self.assertIn("Config/Generated.ini", problems)
        self.assertNotIn("private-value", problems)

    def test_repository_checks_endpoint_consistency_and_links(self):
        self.write(".mcp.json", b'{"mcpServers":{"unreal":{"url":"http://127.0.0.1:9999/mcp"}}}')
        self.write("docs/guide.md", b"[Missing](absent.md)\n")
        self.git("add", ".mcp.json", "docs")
        problems = "\n".join(check_repository(self.root))
        self.assertIn("endpoint", problems)
        self.assertIn("absent.md", problems)

    def test_preflight_dirty_untracked_and_interrupted_work_is_preserved(self):
        self.install_lfs()
        self.write("README.md", b"unfinished work\n")
        self.write("notes.txt", b"keep this\n")
        (self.root / ".git/MERGE_HEAD").write_bytes(self.git("rev-parse", "HEAD"))
        with self.unchanged():
            report = preflight(self.root, inspect_editor=lambda root: [])
        text = "\n".join(report["problems"])
        self.assertIn("README.md", text)
        self.assertIn("notes.txt", text)
        self.assertIn("MERGE_HEAD", text)

    def test_preflight_accepts_clean_checkout_and_reports_cached_base(self):
        self.install_lfs()
        with self.unchanged():
            report = preflight(self.root, inspect_editor=lambda root: [])
        self.assertEqual(report["problems"], [])
        self.assertEqual(report["branch"], "feature/test")
        self.assertEqual(report["base"], "origin/staging")
        self.assertEqual(report["ahead"], 0)
        self.assertEqual(report["behind"], 0)

    def test_preflight_checks_push_destination_and_hook(self):
        self.install_lfs()
        self.git("remote", "set-url", "--push", "origin", "https://github.com/someone/other.git")
        (self.root / ".git/hooks/pre-push").unlink()
        problems = "\n".join(preflight(self.root, inspect_editor=lambda root: ["Editor is open"]) ["problems"])
        self.assertIn("origin push", problems)
        self.assertIn("pre-push", problems)
        self.assertIn("Editor is open", problems)

    def test_status_does_not_recreate_missing_hook_for_hydrated_lfs_asset(self):
        self.install_lfs()
        self.write("Content/map.umap", b"real hydrated asset\n")
        self.git("add", "Content/map.umap")
        self.git("commit", "-m", "real LFS asset")
        hook = self.root / ".git/hooks/pre-push"
        hook.unlink()
        with self.unchanged():
            report = preflight(self.root, inspect_editor=lambda root: [])
        self.assertFalse(hook.exists())
        self.assertIn("pre-push", "\n".join(report["problems"]))

    def test_preflight_inspects_active_relative_and_absolute_custom_hooks(self):
        self.install_lfs()
        content = (self.root / ".git/hooks/pre-push").read_bytes()
        custom = self.root / ".local/team-hooks"
        custom.mkdir(parents=True)
        hook = custom / "pre-push"
        hook.write_bytes(content)
        hook.chmod(0o755)
        for setting in (".local/team-hooks", str(custom)):
            with self.subTest(setting=setting):
                self.git("config", "core.hooksPath", setting)
                with self.unchanged():
                    self.assertEqual(preflight(self.root, inspect_editor=lambda root: []) ["problems"], [])
                hook.unlink()
                with self.unchanged():
                    problems = preflight(self.root, inspect_editor=lambda root: []) ["problems"]
                self.assertIn("pre-push", "\n".join(problems))
                hook.write_bytes(content)
                hook.chmod(0o755)

    def test_preflight_hydration_is_distinct_from_pointer_integrity(self):
        self.install_lfs()
        pointer = self.asset()
        self.write("Content/map.umap", pointer)
        problems = "\n".join(preflight(self.root, inspect_editor=lambda root: []) ["problems"])
        self.assertIn("download", problems)
        self.assertEqual(check_repository(self.root), [])

    def test_preflight_reports_staging_advance_without_fetching(self):
        self.install_lfs()
        original = self.git("rev-parse", "HEAD").decode().strip()
        self.write("new.txt", b"upstream\n")
        self.git("add", "new.txt")
        self.git("commit", "-m", "upstream")
        self.git("update-ref", "refs/remotes/origin/staging", "HEAD")
        self.git("checkout", "-B", "feature/work", original)
        report = preflight(self.root, inspect_editor=lambda root: [])
        self.assertEqual(report["behind"], 1)
        self.assertEqual(report["ahead"], 0)

    def test_editor_inspection_reuses_closed_gate_without_mcp_or_mutation(self):
        with patch("contribute.sys.platform", "darwin"), patch("contribute.platform.machine", return_value="arm64"), \
                patch("workspace.Workspace.discover") as discover, patch("dev.Dev") as inspector_type:
            inspector = inspector_type.return_value
            inspector.require_closed.side_effect = RuntimeError("This project is open")
            with self.assertRaisesRegex(RuntimeError, "project is open"):
                editor_problems(self.root)
            inspector.listener.assert_not_called()
            self.assertEqual([call[0] for call in inspector.method_calls], ["require_closed"])
            inspector.require_closed.side_effect = None
            inspector.listener.return_value = 123
            self.assertIn("occupied", editor_problems(self.root)[0])
            self.assertEqual([call[0] for call in inspector.method_calls],
                             ["require_closed", "require_closed", "listener"])
            discover.assert_called_with(self.root)

    def test_unsupported_editor_inspection_is_explicit(self):
        with patch("contribute.sys.platform", "win32"):
            self.assertIn("unverified", editor_problems(self.root)[0])


if __name__ == "__main__":
    unittest.main()
