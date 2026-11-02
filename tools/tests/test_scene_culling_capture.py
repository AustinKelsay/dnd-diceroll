"""Capture safety and exact-symptom classification, without a live debugger."""
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from diagnostics.scene_culling_lldb import is_scene_culling_assertion, verify_identity


class SceneCaptureTests(unittest.TestCase):
    def test_unrelated_editor_refused_even_with_its_correct_hash(self):
        with tempfile.TemporaryDirectory() as directory:
            editor = Path(directory) / "MacOS" / "UnrealEditor"
            editor.parent.mkdir()
            editor.write_bytes(b"editor")
            with self.assertRaisesRegex(ValueError, "not an editor"):
                verify_identity(str(editor), str(editor), hashlib.sha256(b"editor").hexdigest())

    def test_wrong_executable_or_modified_binary_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            game = Path(directory) / "Game.app" / "Contents" / "MacOS" / "DnDDiceRoll"
            game.parent.mkdir(parents=True)
            game.write_bytes(b"original")
            other = game.with_name("OtherGame")
            other.write_bytes(b"original")
            digest = hashlib.sha256(game.read_bytes()).hexdigest()
            with self.assertRaisesRegex(ValueError, "differs"):
                verify_identity(str(other), str(game), digest)
            self.assertEqual(verify_identity(str(game), str(game), digest), game.resolve())
            game.write_bytes(b"changed")
            with self.assertRaisesRegex(ValueError, "SHA-256"):
                verify_identity(str(game), str(game), digest)

    def test_only_exact_uploader_scene_signature_is_red(self):
        reserve = ["FDebug::CheckVerifyFailedImpl2", "FSceneCullingBuilder::ProcessPostSceneUpdate"]
        upload = ["TBufferScatterUploader::ResizeAndUploadTo", "FSceneCullingBuilder::UploadToGPU"]
        self.assertTrue(is_scene_culling_assertion("UploadDataProxy == nullptr", reserve))
        self.assertTrue(is_scene_culling_assertion("UploadDataProxy == nullptr", upload))
        self.assertFalse(is_scene_culling_assertion("DebugTaskCounter++ == 0", reserve))
        self.assertFalse(is_scene_culling_assertion("UploadDataProxy == nullptr", ["OtherRenderer"]))
        self.assertFalse(is_scene_culling_assertion("", ["LLM", "main"]))
