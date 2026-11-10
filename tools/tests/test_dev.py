import argparse
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from contextlib import contextmanager
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from workspace import Workspace, automation_result, entered_world, log_events, project_argument, require_clean, source_digest, validate_bootstrap, validate_runtime
from mcp_client import MCP, decode_response
from scaffold import new_class
import dev as dev_module
import struct
from dev import Dev, build_warnings, core_count, load_poses, parse_resolution, resolution_argument, verify_capture


class Response(io.BytesIO):
    def __init__(self, content, kind="application/json"):
        super().__init__(content.encode())
        self.headers = {"Content-Type": kind}


class WorkspaceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="unreal workspace ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in ("Config", "tools", "Source/Game"):
            (self.root / name).mkdir(parents=True)
        (self.root / "Game.uproject").write_text(json.dumps({"Modules": [{"Name": "Game"}]}))
        (self.root / "tools/project.json").write_text(json.dumps({"module":"Game", "editor_target":"CustomEditor", "game_target":"CustomGame", "content_root":"/Game/Starter"}))
        (self.root / "tools/versions.json").write_text(json.dumps({"unreal_engine":"5.8.4", "unreal_changelist":58902052, "mcp_url":"http://127.0.0.1:18765/mcp"}))
        (self.root / "Config/DefaultEngine.ini").write_text("[/Script/EngineSettings.GameMapsSettings]\nEditorStartupMap=/Game/Starter/Sandbox\n")
        for target in ("CustomEditor", "CustomGame"):
            (self.root / f"Source/{target}.Target.cs").touch()
        self.w = Workspace.discover(self.root)

    def test_discovery_uses_explicit_targets(self):
        self.assertEqual(self.w.settings["editor_target"], "CustomEditor")
        self.assertEqual(self.w.project.name, "Game.uproject")

    def test_ambiguous_project_requires_selection(self):
        (self.root / "Copy.uproject").write_text((self.root / "Game.uproject").read_text())
        with self.assertRaises(ValueError): Workspace.discover(self.root)
        self.assertEqual(Workspace.discover(self.root, "Game.uproject").project, self.w.project)

    def test_missing_project(self):
        self.w.project.unlink()
        with self.assertRaises(ValueError): Workspace.discover(self.root)

    def test_wrong_module(self):
        self.w.project.write_text('{"Modules":[]}')
        with self.assertRaises(ValueError): Workspace.discover(self.root)

    def test_exact_argument_with_spaces(self):
        self.assertTrue(project_argument(f'/Applications/UnrealEditor {self.w.project} -nosplash', self.w.project))
        self.assertTrue(project_argument(f'UnrealEditor "{self.w.project}"', self.w.project))
        for command in ("UnrealEditor", f'UnrealEditor {self.w.project}.backup', f'UnrealEditor /other/{self.w.project.name}', f'UnrealEditor -abslog={self.w.project}'):
            self.assertFalse(project_argument(command, self.w.project), command)

    def test_runtime_identity_rejects_copies_and_stale_pid(self):
        status = {"project_file": str(self.w.project), "pid": 123, "engine_version": "5.8.4-58902052+++UE5+Release-5.8"}
        validate_runtime(status, self.w, 123)
        for field, value in (("project_file", str(self.root / "copy/Game.uproject")), ("pid", 456), ("engine_version", "5.8.3-123+++UE5")):
            with self.assertRaises(RuntimeError): validate_runtime({**status, field:value}, self.w, 123)

    def test_scaffold_refuses_overwrite_and_invalid_input(self):
        header, source = new_class(self.w, "component", "StarterProbe")
        before = header.read_bytes()
        self.assertIn(b"UStarterProbe : public UActorComponent", before)
        with self.assertRaises(ValueError): new_class(self.w, "component", "StarterProbe")
        self.assertEqual(header.read_bytes(), before)
        for name in ("../Escape", "bad_name", "X", "Name;Quit"):
            with self.assertRaises(ValueError): new_class(self.w, "actor", name)
        self.assertTrue(source.exists())

    def test_build_refuses_any_associated_editor_without_using_mcp(self):
        dev = Dev(self.w)
        with patch.object(dev, "require_engine"), patch.object(dev, "processes", return_value=[{"pid":123}]), patch.object(dev, "execute") as execute:
            with self.assertRaises(RuntimeError): dev.build()
            execute.assert_not_called()

    def test_build_receipt_tracks_source_deletions_and_binary_changes(self):
        binary_folder = self.root / "Binaries/Linux"
        binary_folder.mkdir(parents=True)
        (binary_folder / "UnrealEditor.modules").write_text('{"Modules":{"Game":"libUnrealEditor-Game.so"}}')
        binary = binary_folder / "libUnrealEditor-Game.so"
        binary.write_bytes(b"compiled")
        self.w.local.mkdir()
        source = self.root / "Source/Game/Example.cpp"
        source.write_text("// source")
        dev = Dev(self.w, host="linux", cores=2)
        with patch.object(dev, "require_engine"), patch.object(dev, "require_closed"), patch.object(dev, "run_logged", return_value=0) as run:
            dev.build()  # A successful no-action build is valid too.
        command = [str(a) for a in run.call_args.args[0]]
        self.assertTrue(command[0].endswith("Build/BatchFiles/Linux/Build.sh"))
        self.assertEqual(command[1:4], ["CustomEditor", "Linux", "Development"])
        self.assertIn("-MaxParallelActions=2", command)
        receipt = json.loads((self.w.local / "build.json").read_text())
        self.assertEqual(receipt, dev.build_identity())
        previous = source_digest(self.w)
        source.unlink()
        self.assertNotEqual(previous, source_digest(self.w))
        self.assertNotEqual(receipt, dev.build_identity())
        source.write_text("// source")
        binary.write_bytes(b"changed")
        self.assertNotEqual(receipt, dev.build_identity())

    def test_build_fails_on_compiler_or_ubt_warnings_and_exit_code(self):
        self.w.local.mkdir()
        for output, code, message in (("Foo.cpp:3:5: warning: unused variable 'x' [-Wunused-variable]\n", 0, "warning"),
                                      ("WARNING: Plugin 'X' is missing\n", 0, "warning"),
                                      ("Result: Failed\n", 6, "exit 6")):
            dev = Dev(self.w, host="linux")
            def run(args, log, output=output, code=code):
                log.write_text(output)
                return code
            with patch.object(dev, "require_engine"), patch.object(dev, "require_closed"), patch.object(dev, "run_logged", side_effect=run):
                with self.assertRaisesRegex(RuntimeError, message): dev.build()
            self.assertFalse((self.w.local / "build.json").exists())
        self.assertEqual(build_warnings("Compile Warning.cpp\nTotal: 0 warning(s), 0 error(s)\n-Wno-unused\n"), [])

    def test_config_guard_reports_recursive_change_even_when_run_fails(self):
        nested = self.root / "Config/Linux/LinuxEngine.ini"
        nested.parent.mkdir()
        nested.write_text("[A]\n")
        dev = Dev(self.w, host="linux")
        with self.assertRaisesRegex(RuntimeError, r"Config/Linux/LinuxEngine.ini.*run also failed: engine crashed") as caught:
            with dev.config_guard():
                nested.write_text("[A]\nToken=x\n")
                raise OSError("engine crashed")
        self.assertIsInstance(caught.exception.__cause__, OSError)
        with self.assertRaisesRegex(RuntimeError, "Config/New.ini"):
            with dev.config_guard():
                (self.root / "Config/New.ini").write_text("")
        with dev.config_guard():
            pass

    def test_editor_waits_for_a_bindable_mcp_port(self):
        import socket
        dev = Dev(self.w, host="linux")
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as holder:
            holder.bind(("127.0.0.1", 0))
            dev.endpoint = dev.endpoint._replace(netloc=f"127.0.0.1:{holder.getsockname()[1]}")
            with patch("dev.time.sleep"):
                with self.assertRaisesRegex(RuntimeError, "still held"): dev.wait_for_free_port(timeout=0)
        dev.wait_for_free_port(timeout=0)

    def test_cores_must_be_positive(self):
        import argparse
        for bad in ("0", "-1", "two"):
            with self.assertRaises(argparse.ArgumentTypeError): core_count(bad)
        self.assertEqual(core_count("2"), 2)
        with self.assertRaisesRegex(ValueError, "1 or more"): Dev(self.w, host="linux", cores=0)
        dev = Dev(self.w, host="mac", cores=2)
        self.assertIn("-corelimit=2", dev.automation_command("Project.", self.root, self.root / "a.log"))

    def test_doctor_reports_missing_tools_and_guides_off_ultra(self):
        dev = Dev(self.w, host="linux")
        out = io.StringIO()
        with patch("dev.shutil.which", side_effect=lambda name: None if name == "lsof" else "/usr/bin/" + name), patch("sys.stdout", out), \
             patch.object(dev, "require_engine", return_value="5.8.4"), patch.object(dev, "check_plugins", return_value="ok"), \
             patch.object(dev, "check_clang", return_value="clang"), patch.object(dev, "check_configs", return_value="ok"), \
             patch.object(dev, "execute", return_value=Mock(stdout="git-lfs")):
            with self.assertRaisesRegex(RuntimeError, "Tool lsof"): dev.doctor()
        self.assertIn("FAIL Tool lsof: `lsof` is not installed", out.getvalue())
        self.assertIn("pacman -S lsof", out.getvalue())
        with patch.object(dev, "listener", side_effect=lambda: dev.tool("lsof")), patch("dev.shutil.which", return_value=None):
            with self.assertRaisesRegex(RuntimeError, "not installed"): dev.listener()
        out = io.StringIO()
        with patch("dev.current_host", return_value=None), patch("sys.stdout", out):
            Dev(self.w).doctor()
        self.assertIn("Not on PlebDev Ultra", out.getvalue())
        self.assertIn("SKIP", out.getvalue())

    def test_package_keeps_original_failure_when_game_ignores_sigterm(self):
        import subprocess
        self.w.local.mkdir()
        with (self.root / "Config/DefaultEngine.ini").open("a") as ini: ini.write("GameDefaultMap=/Game/Starter/Sandbox\n")
        dev = Dev(self.w, host="linux")
        process = Mock(pid=77)
        process.poll.return_value = None
        process.wait.side_effect = subprocess.TimeoutExpired("game", 30)
        binary = self.root / "Builds/game"
        clock = iter([0, 0, 100, 100])
        with patch.object(dev, "require_engine"), patch.object(dev, "require_closed"), patch.object(dev, "execute"), \
             patch.object(dev, "packaged_binary", return_value=binary), patch("dev.subprocess.Popen", return_value=process), \
             patch("dev.time.monotonic", side_effect=lambda: next(clock)), patch("dev.time.sleep"):
            with self.assertRaisesRegex(RuntimeError, "did not exit after one SIGTERM.*Earlier failure: Packaged game did not enter") as caught:
                dev.package()
        process.terminate.assert_called_once_with()
        self.assertIsInstance(caught.exception.__cause__, RuntimeError)

    def test_close_refuses_dirty_and_pie_without_quit(self):
        dev = Dev(self.w, host="linux")
        for state in ({"dirty_packages":["/Temp/Unsaved"], "pie_active":False}, {"dirty_packages":[], "pie_active":True}):
            @contextmanager
            def connected(): yield Mock(), state
            with patch.object(dev, "require_engine"), patch.object(dev, "os_identity", return_value=123), patch.object(dev, "connected", connected), patch.object(dev, "execute") as execute, patch("dev.os.kill") as kill:
                with self.assertRaises(RuntimeError): dev.close()
                execute.assert_not_called()
                kill.assert_not_called()

    def test_operation_lock_is_bounded(self):
        first, second = Dev(self.w), Dev(self.w)
        with first.operation():
            with self.assertRaises(RuntimeError):
                with second.operation(): pass

    def test_skill_verification_refuses_open_editor(self):
        dev = Dev(self.w)
        with patch.object(dev, "require_engine"), patch.object(dev, "processes", return_value=[{"pid":123}]), patch.object(dev, "execute") as execute:
            with self.assertRaises(RuntimeError): dev.verify_skills()
            execute.assert_not_called()

    def test_skill_verification_requires_current_source_and_engine_readback(self):
        import hashlib
        source = self.root / "docs/agents/editor-authoring.md"
        source.parent.mkdir(parents=True)
        source.write_text("Project instructions\n")
        self.w.local.mkdir()
        dev = Dev(self.w, host="linux")
        report = {"project_file": str(self.w.project), "engine_version": "5.8.4-58902052+++UE5",
                  "instructions_sha256": hashlib.sha256(source.read_bytes()).hexdigest()}
        invalid = [None, {**report, "project_file": "/other/Game.uproject"},
                   {**report, "engine_version": "5.8.3-123+++UE5"},
                   {**report, "instructions_sha256": "stale"}]
        for value in [*invalid, report]:
            def run(args, console, timeout):
                log = Path(next(str(a).split("=", 1)[1] for a in args if str(a).startswith("-abslog=")))
                log.write_text("LogPython: " + ("no readback" if value is None else "PROJECT_SKILL_VERIFIED " + json.dumps(value)))
                self.assertIn("-RenderOffscreen", [str(a) for a in args])
                self.assertNotIn("-nullrhi", [str(a) for a in args])
                return 0
            with patch.object(dev, "require_engine"), patch.object(dev, "require_closed"), patch.object(dev, "run_bounded", side_effect=run):
                if value in invalid:
                    with self.assertRaises(RuntimeError): dev.verify_skills()
                    self.assertFalse(list(self.w.local.glob("skills/*/verified.json")))
                else:
                    dev.verify_skills()
        self.assertEqual(len(list(self.w.local.glob("skills/*/verified.json"))), 1)

    def fake_automation(self, log_text, code=0, rewrite_config=False, crash=None, state="Success"):
        def run(args, log, timeout):
            output = Path(next(str(a).split("=", 1)[1] for a in args if str(a).startswith("-ReportExportPath=")))
            (output / "index.json").write_text(json.dumps({"tests":[{"state":state, "fullTestPath":"Project.A"}]}), encoding="utf-8-sig")
            if crash:
                (self.root / "Saved/Crashes" / crash).mkdir(parents=True)
            (output / "automation.log").write_text(log_text)
            if rewrite_config:
                with (self.root / "Config/DefaultEngine.ini").open("a") as ini: ini.write("[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]\n")
            self.commands.append([str(a) for a in args])
            return code
        self.commands = []
        return run

    def test_automation_command_reads_unreal_bom_report(self):
        dev = Dev(self.w, host="linux", cores=2)
        self.w.local.mkdir()
        with patch.object(dev, "require_engine"), patch.object(dev, "require_closed"), patch.object(dev, "run_bounded", side_effect=self.fake_automation("LogAutomation: Display: completed\n")):
            dev.test("Project.Starter.")
        self.assertEqual(len(list(self.w.local.glob("automation/*/verified.json"))), 1)
        command = self.commands[0]
        self.assertTrue(command[0].endswith("Binaries/Linux/UnrealEditor-Cmd"))
        self.assertIn("-ExecCmds=Automation RunTests Project.Starter.;SoftQuit", command)
        self.assertIn("-RenderOffscreen", command)
        self.assertIn("-corelimit=2", command)
        for flag in ("-nullrhi", "-TestExit=Automation Test Queue Empty"):
            self.assertNotIn(flag, command)

    def test_automation_gate_refuses_warnings_exit_code_and_config_rewrite(self):
        self.w.local.mkdir()
        cases = (({"log_text": "LogTemp: Warning: something\n"}, "zero"),
                 ({"log_text": "", "code": 1}, "exited 1"),
                 ({"log_text": "", "rewrite_config": True}, "changed tracked config"))
        for kwargs, message in cases:
            dev = Dev(self.w, host="linux")
            original = (self.root / "Config/DefaultEngine.ini").read_bytes()
            with patch.object(dev, "require_engine"), patch.object(dev, "require_closed"), patch.object(dev, "run_bounded", side_effect=self.fake_automation(**kwargs)):
                with self.assertRaisesRegex(RuntimeError, message): dev.test("Project.")
            (self.root / "Config/DefaultEngine.ini").write_bytes(original)
        dev = Dev(self.w, host="linux")
        with patch.object(dev, "require_engine"), patch.object(dev, "require_closed"), patch.object(dev, "run_bounded", side_effect=self.fake_automation("LogTemp: Warning: something\n")):
            dev.test("Project.", allow_warnings=True)

    def test_automation_reports_a_failed_lifecycle_even_when_tests_fail(self):
        """#157: a crash at shutdown is a failed lifecycle, and is never hidden behind test results."""
        self.w.local.mkdir()
        cases = (({"log_text": "", "crash": "crashinfo-A"}, "Tests passed but the run is a failed lifecycle: new crash report.*crashinfo-A"),
                 ({"log_text": "", "code": -11, "state": "Fail", "crash": "crashinfo-B"}, "did not pass.*Failed lifecycle: Engine was killed by signal 11; new crash report.*crashinfo-B"),
                 ({"log_text": "", "code": -6}, "failed lifecycle: Engine was killed by signal 6"))
        for kwargs, message in cases:
            dev = Dev(self.w, host="linux")
            with patch.object(dev, "require_engine"), patch.object(dev, "require_closed"), patch.object(dev, "run_bounded", side_effect=self.fake_automation(**kwargs)):
                with self.assertRaisesRegex(RuntimeError, message): dev.test("Project.")
        self.assertFalse(list(self.w.local.glob("automation/*/verified.json")))
        # A crash report left by an earlier run is not this run's crash.
        dev = Dev(self.w, host="linux")
        with patch.object(dev, "require_engine"), patch.object(dev, "require_closed"), patch.object(dev, "run_bounded", side_effect=self.fake_automation("")):
            dev.test("Project.")
        self.assertEqual(len(list(self.w.local.glob("automation/*/verified.json"))), 1)

    def test_linux_layout_and_unsupported_host(self):
        dev = Dev(self.w, host="linux")
        self.assertTrue(str(dev.editor).endswith("Engine/Binaries/Linux/UnrealEditor"))
        self.assertTrue(str(dev.cmd).endswith("Engine/Binaries/Linux/UnrealEditor-Cmd"))
        self.assertEqual(dev.platform, "Linux")
        self.assertEqual(Dev(self.w, host="mac").platform, "Mac")
        with patch("dev.current_host", return_value=None):
            with self.assertRaisesRegex(RuntimeError, "Linux x86_64"): Dev(self.w).check_host()

    def test_linux_close_sends_exactly_one_sigterm_after_clean_gate(self):
        import signal
        dev = Dev(self.w, host="linux")
        @contextmanager
        def connected(): yield Mock(), {"dirty_packages":[], "pie_active":False}
        with patch.object(dev, "require_engine"), patch.object(dev, "os_identity", return_value=123), patch.object(dev, "connected", connected), \
             patch.object(dev, "runtime", return_value={"dirty_packages":[], "pie_active":False}), patch.object(dev, "processes", return_value=[]), \
             patch.object(dev, "listener", return_value=None), patch("dev.os.kill") as kill, \
             patch("dev.pid_exists", side_effect=[True, True, False]), patch("dev.time.sleep") as sleep:
            dev.close()
        kill.assert_called_once_with(123, signal.SIGTERM)
        self.assertEqual(sleep.call_count, 2)  # waits until the PID is really gone
        with patch.object(dev, "require_engine"), patch("dev.os.kill") as kill:
            with self.assertRaisesRegex(RuntimeError, "save dialog"): dev.close(request_quit=True)
            kill.assert_not_called()

    def test_linux_package_limits_and_binary_discovery(self):
        dev = Dev(self.w, host="linux", cores=2)
        self.assertEqual(dev.package_limits(), ["-ubtargs=-MaxParallelActions=2", "-AdditionalCookerOptions=-corelimit=2", "-AdditionalIoStoreOptions=-corelimit=2"])
        self.assertEqual(Dev(self.w, host="linux").package_limits(), [])
        archive = self.root / "Builds/stamp"
        binary = archive / "Linux/CustomGame/Binaries/Linux/CustomGame"
        binary.parent.mkdir(parents=True)
        (archive / "Linux/CustomGame.sh").write_text("#!/bin/sh\n")
        binary.write_text("")
        with self.assertRaises(RuntimeError): dev.packaged_binary(archive)  # not executable
        binary.chmod(0o755)
        self.assertEqual(dev.packaged_binary(archive), binary)

    def test_smoke_preserves_start_failure_when_cleanup_fails(self):
        self.w.local.mkdir()
        self.w.log.parent.mkdir(parents=True)
        self.w.log.touch()
        dev = Dev(self.w)
        client = Mock()
        def call(toolset, tool, arguments=None):
            if tool == "get_current_level": return self.w.startup_map
            if tool == "find_actors": return ["floor"]
            if tool == "get_label": return "SandboxFloor"
            if tool == "StartPIE": raise RuntimeError("start timed out")
            if tool == "IsPIERunning": raise RuntimeError("connection lost")
            raise AssertionError(tool)
        client.call.side_effect = call
        @contextmanager
        def connected(): yield client, {"dirty_packages":[], "pie_active":False}
        with patch.object(dev, "connected", connected):
            with self.assertRaisesRegex(RuntimeError, "start timed out.*connection lost.*PIE state unknown"):
                dev.smoke()
        report = json.loads((self.w.local / "smoke-failure.json").read_text())
        self.assertEqual(report["failure"], "start timed out")
        self.assertEqual(report["pie_state"], "unknown")
        self.assertFalse((self.w.local / "smoke-result.json").exists())


def fake_png(width, height):
    return b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR" + struct.pack(">II", width, height) + b"\x08\x06\x00\x00\x00"


class GalleryTests(unittest.TestCase):
    setUp = WorkspaceTests.setUp  # Same throwaway workspace.

    SHOTS = [{"name": "battle-a", "mode": "battle", "yaw": -90, "distance": "default"},
             {"name": "table", "mode": "overview"}, {"name": "menu", "mode": "start"}]

    def poses(self, shots=None, name="poses.json", **extra):
        path = self.root / name
        path.write_text(json.dumps({"version": 1, "shots": self.SHOTS if shots is None else shots, **extra}))
        return path

    def fixture(self, name="board-a"):
        folder = self.root / "Saved/CaptureFixtures" / name
        folder.mkdir(parents=True, exist_ok=True)
        (folder / "current-v1.json").write_text("{}")
        (folder / "fixture.json").write_text('{"seed": 7}')

    def ready(self):
        dev = Dev(self.w, host="linux", cores=2)
        self.w.local.mkdir(exist_ok=True)
        identity = {"fake": "identity"}
        (self.w.local / "build.json").write_text(json.dumps(identity))
        patches = [patch.object(dev, "require_engine"), patch.object(dev, "require_closed"), patch.object(dev, "build_identity", return_value=identity),
                   patch.object(dev, "execute", return_value=Mock(stdout="abc123\n"))]
        for item in patches:
            item.start()
            self.addCleanup(item.stop)
        return dev

    @staticmethod
    def write_run(args, skip=(), size=None, status="ok"):
        out = Path(next(a for a in args if str(a).startswith("-ExpeditionCaptureOut=")).split("=", 1)[1])
        width = int(next(a for a in args if str(a).startswith("-ResX=")).split("=")[1])
        height = int(next(a for a in args if str(a).startswith("-ResY=")).split("=")[1])
        shots = []
        for shot in GalleryTests.SHOTS:
            if shot["name"] in skip:
                shots.append({"name": shot["name"], "status": "skipped", "reason": "not here"})
                continue
            (out / f"{shot['name']}.png").write_bytes(fake_png(*(size or (width, height))))
            shots.append({"name": shot["name"], "status": "captured", "file": f"{shot['name']}.png", "camera": {}})
        (out / "manifest.json").write_text(json.dumps({"status": status, "seed": 7, "boardWidth": 16, "boardHeight": 11, "themed": True, "shots": shots}))
        return 0

    def test_resolution_parsing(self):
        self.assertEqual(parse_resolution("1920x1080"), (1920, 1080))
        for bad in ("1920", "1920X1080", "0x0", "100x100", "9000x9000", "1920x1080x2", "-1920x1080", "axb"):
            with self.assertRaises(ValueError, msg=bad): parse_resolution(bad)
        with self.assertRaises(argparse.ArgumentTypeError): resolution_argument("big")

    def test_poses_validation(self):
        self.assertEqual(len(load_poses(self.poses())), 3)
        self.assertEqual(len(load_poses(self.poses([{"name": "battle-far", "mode": "battle", "yaw": 0, "distance": "far"}]))), 1)
        self.assertEqual(len(load_poses(Path(__file__).resolve().parents[1] / "capture/poses.json")), 27)
        bad = [{"version": 2, "shots": self.SHOTS}, {"version": 1, "shots": []}, {"version": 1}, {"version": True, "shots": self.SHOTS},
               {"version": 1, "shots": ["x"]}, {"version": 1, "shots": [{"mode": "start"}]}, {"version": 1, "shots": [{"name": "a b", "mode": "start"}]},
               {"version": 1, "shots": [{"name": "a", "mode": "orbit"}]}, {"version": 1, "shots": [{"name": "a", "mode": "battle", "distance": 1300}]},
               {"version": 1, "shots": [{"name": "a", "mode": "battle", "yaw": 0, "distance": 100}]},
               {"version": 1, "shots": [{"name": "a", "mode": "battle", "yaw": 0, "distance": "near"}]},
               {"version": 1, "shots": [{"name": "a", "mode": "battle", "yaw": 0, "distance": "Far"}]},
               {"version": 1, "shots": [{"name": "a", "mode": "battle", "yaw": 500, "distance": 1300}]},
               {"version": 1, "shots": [{"name": "a", "mode": "start"}, {"name": "a", "mode": "start"}]},
               {"version": 1, "shots": [{"name": "a", "mode": "tooltip", "yaw": 0, "distance": 1300}]},
               {"version": 1, "shots": [{"name": "a", "mode": "tooltip", "action": "jump", "yaw": 0, "distance": 1300}]},
               {"version": 1, "shots": [{"name": "a", "mode": "hint", "yaw": 0, "distance": 1300}]},
               {"version": 1, "shots": [{"name": "a", "mode": "hint", "hint": "tips", "yaw": 0, "distance": 1300}]},
               {"version": 1, "shots": [{"name": "a", "mode": "hint", "hint": "move", "distance": 1300}]},
               {"version": 1, "shots": [{"name": "a", "mode": "start", "build": "Wizard"}]},
               {"version": 1, "shots": [{"name": "a", "mode": "battle", "build": "wizard.evocation", "yaw": 0, "distance": 1300}]},
               {"version": 1, "settleFrames": 0, "shots": self.SHOTS}]
        for data in bad:
            path = self.root / "bad.json"
            path.write_text(json.dumps(data))
            with self.assertRaises(ValueError, msg=str(data)): load_poses(path)
        (self.root / "bad.json").write_text("not json")
        with self.assertRaises(ValueError): load_poses(self.root / "bad.json")
        with self.assertRaises(ValueError): load_poses(self.root / "missing.json")

    def test_gallery_refusals_name_the_remedy(self):
        dev = self.ready()
        poses = self.poses()
        with self.assertRaisesRegex(RuntimeError, "Fixture board-a is missing.*CaptureFixtures"): dev.gallery(["board-a"], ["1280x720"], poses=poses)
        self.fixture()
        with self.assertRaisesRegex(ValueError, "Resolution"): dev.gallery(["board-a"], ["wide"], poses=poses)
        with self.assertRaisesRegex(ValueError, "Fixture"): dev.gallery(["../board"], ["1280x720"], poses=poses)
        with self.assertRaisesRegex(ValueError, "Poses"): dev.gallery(["board-a"], ["1280x720"], poses=self.poses(shots=[], name="empty.json"))
        with self.assertRaisesRegex(ValueError, "timeout"): dev.gallery(["board-a"], ["1280x720"], poses=poses, timeout=5)
        (self.w.local / "build.json").write_text('{"stale": true}')
        with self.assertRaisesRegex(RuntimeError, "Build evidence"): dev.gallery(["board-a"], ["1280x720"], poses=poses)

    def test_gallery_refuses_while_the_editor_is_open(self):
        self.fixture()
        dev = Dev(self.w, host="linux")
        with patch.object(dev, "require_engine"), patch.object(dev, "processes", return_value=[{"pid": 5}]), patch.object(dev, "run_game") as run:
            with self.assertRaisesRegex(RuntimeError, "open in editor PID"): dev.gallery(["board-a"], ["1280x720"], poses=self.poses())
            run.assert_not_called()

    def test_gallery_runs_each_fixture_on_a_copy_and_writes_an_index(self):
        dev = self.ready()
        self.fixture()
        self.fixture("board-b")
        calls = []
        def run(args, log, timeout):
            calls.append([str(a) for a in args])
            return self.write_run(args, skip=("table",))
        with patch.object(dev, "run_game", side_effect=run):
            dev.gallery(["board-a", "board-b"], ["1280x720", "1920x1080"], primitives=True, poses=self.poses())
        self.assertEqual(len(calls), 4)
        first = calls[0]
        self.assertIn("-game", first)
        self.assertIn("-RenderOffscreen", first)
        self.assertIn("-ForceRes", first)
        self.assertIn("-ResX=1280", first)
        self.assertIn("-ExpeditionPrimitives", first)
        self.assertIn("-corelimit=2", first)
        save = next(a for a in first if a.startswith("-ExpeditionSaveRoot=")).split("=", 1)[1]
        self.assertTrue(save.startswith(str(self.w.local / "gallery")))
        self.assertNotIn("Saved/CaptureFixtures", save)
        index = json.loads(next((self.w.local / "gallery").glob("*/index.json")).read_text())
        self.assertEqual(index["commit"], "abc123")
        self.assertEqual([r["resolution"] for r in index["runs"]], ["1280x720", "1920x1080", "1280x720", "1920x1080"])
        self.assertTrue(all(r["primitives"] and r["seed"] == 7 for r in index["runs"]))
        self.assertEqual(sum(s["status"] == "skipped" for s in index["runs"][0]["shots"]), 1)
        self.assertEqual((self.root / "Saved/CaptureFixtures/board-a/current-v1.json").read_text(), "{}")

    def test_gallery_fails_on_bad_game_output(self):
        self.fixture()
        poses = self.poses()
        for label, kwargs, code, message in (("exit code", {}, 3, "exited 3"), ("size", {"size": (640, 360)}, 0, "640x360, not the requested 1280x720"),
                                             ("game failure", {"status": "failed"}, 0, "Capture failed"), ("all skipped", {"skip": ("battle-a", "table", "menu")}, 0, "Every shot was skipped")):
            dev = self.ready()
            def run(args, log, timeout, kwargs=kwargs, code=code):
                self.write_run(args, **kwargs)
                return code
            with patch.object(dev, "run_game", side_effect=run), self.assertRaisesRegex(RuntimeError, message, msg=label):
                dev.gallery(["board-a"], ["1280x720"], poses=poses)

    def test_gallery_fails_when_a_game_run_leaves_a_crash_report(self):
        self.fixture()
        dev = self.ready()
        def run(args, log, timeout):
            self.write_run(args)
            (self.root / "Saved/Crashes/crashinfo-Game").mkdir(parents=True)
            return 0
        with patch.object(dev, "run_game", side_effect=run), self.assertRaisesRegex(RuntimeError, "Failed lifecycle for board-a at 1280x720: new crash report.*crashinfo-Game"):
            dev.gallery(["board-a"], ["1280x720"], poses=self.poses())

    def test_verify_capture_requires_every_shot_and_nonempty_pictures(self):
        out = self.root / "out"
        out.mkdir()
        manifest = out / "manifest.json"
        with self.assertRaisesRegex(RuntimeError, "no manifest"): verify_capture(manifest, self.SHOTS, (1280, 720), out)
        manifest.write_text(json.dumps({"status": "ok", "shots": [{"name": "battle-a", "status": "captured", "file": "battle-a.png", "camera": {}}]}))
        with self.assertRaisesRegex(RuntimeError, "lacks shot"): verify_capture(manifest, self.SHOTS, (1280, 720), out)
        manifest.write_text(json.dumps({"status": "ok", "shots": [{"name": s["name"], "status": "captured", "file": s["name"] + ".png", "camera": {}} for s in self.SHOTS]}))
        with self.assertRaisesRegex(RuntimeError, "no picture"): verify_capture(manifest, self.SHOTS, (1280, 720), out)
        for shot in self.SHOTS: (out / (shot["name"] + ".png")).write_bytes(b"")
        with self.assertRaisesRegex(RuntimeError, "no picture"): verify_capture(manifest, self.SHOTS, (1280, 720), out)
        (out / "battle-a.png").write_bytes(b"not a png at all, but long enough to read")
        with self.assertRaisesRegex(ValueError, "not a PNG"): verify_capture(manifest, self.SHOTS, (1280, 720), out)

    def test_game_run_gets_one_sigterm_on_timeout_and_is_never_killed(self):
        dev = Dev(self.w, host="linux")
        process = Mock(pid=77)
        process.wait.side_effect = [subprocess.TimeoutExpired("game", 1), subprocess.TimeoutExpired("game", 30)]
        with patch.object(dev_module.subprocess, "Popen", return_value=process):
            with self.assertRaisesRegex(RuntimeError, "ignored one SIGTERM; left running"): dev.run_game(["/bin/game"], self.root / "game.log", 1)
        process.terminate.assert_called_once()
        process.kill.assert_not_called()
        process.wait.side_effect = [subprocess.TimeoutExpired("game", 1), 0]
        with patch.object(dev_module.subprocess, "Popen", return_value=process):
            with self.assertRaisesRegex(RuntimeError, "timed out after 1s"): dev.run_game(["/bin/game"], self.root / "game.log", 1)

    def test_gallery_takes_cores_and_the_operation_lock_like_other_commands(self):
        self.assertIn("gallery", dev_module.CORE_COMMANDS)
        source = Path(dev_module.__file__).read_text()
        self.assertIn('dev.gallery(args.fixture, args.res, args.primitives, args.poses, args.timeout)', source)


class EvidenceTests(unittest.TestCase):
    def test_packaged_world_requires_exact_map(self):
        self.assertTrue(entered_world("LogWorld: Bringing World /Game/Starter/Sandbox.Sandbox up for play", "/Game/Starter/Sandbox"))
        self.assertFalse(entered_world("Loading /Game/Starter/Sandbox\nLogWorld: Bringing World /Game/Other up for play", "/Game/Starter/Sandbox"))
        self.assertFalse(entered_world("LogWorld: Bringing World /Game/Starter/SandboxCopy.SandboxCopy up for play", "/Game/Starter/Sandbox"))

    def test_dirty_includes_unsaved_packages(self):
        require_clean({"dirty_packages":[], "pie_active":False})
        with self.assertRaisesRegex(RuntimeError, "Unsaved"):
            require_clean({"dirty_packages":["/Temp/Untitled"], "pie_active":False})

    def test_severity_not_message_words(self):
        result = log_events("LogAudio: Warning: Error initializing device\nLogPython: Display: Success - 0 error(s), 0 warning(s)\nLogTemp: Error: failed\nLogBlueprint: Display: Blueprint Runtime Error: None\n")
        self.assertEqual(len(result["errors"]), 2)
        self.assertEqual(len(result["warnings"]), 1)

    def test_fresh_log_offset(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "editor.log"
            log.write_text("LogTemp: Error: historical\n")
            offset = log.stat().st_size
            with log.open("a") as stream: stream.write("LogTemp: Warning: current\n")
            dev = object.__new__(Dev)
            dev.w = Mock(log=log)
            self.assertEqual(dev.new_log_events(offset)["errors"], [])
            log.write_text("")
            with self.assertRaises(RuntimeError): dev.new_log_events(offset)

    def test_bootstrap_requires_marker_and_clean_summary(self):
        valid = "STARTER_BOOTSTRAP_VALIDATED /Game/Starter\nSuccess - 0 error(s), 0 warning(s)"
        validate_bootstrap(valid)
        for invalid in ("Success - 0 error(s), 0 warning(s)", valid.replace("0 warning", "1 warning"), "STARTER_BOOTSTRAP_VALIDATED"):
            with self.assertRaises(RuntimeError): validate_bootstrap(invalid)

    def test_automation_requires_executed_passing_tests(self):
        self.assertEqual(automation_result({"tests":[{"state":"Success"}]})["passed"], 1)
        for report in ({"tests":[]}, {"tests":[{"state":"Fail"}]}, {"tests":[{"state":"NotRun"}]}, {"tests":[{"state":"Success"}], "failed":1}):
            with self.assertRaises(RuntimeError): automation_result(report)


class ProtocolTests(unittest.TestCase):
    def test_json_response_id(self):
        self.assertEqual(decode_response(Response('{"id":1,"result":{}}'),1)["result"], {})
        with self.assertRaises(RuntimeError): decode_response(Response('{"id":2}'),1)

    def test_sse_skips_notifications_and_other_ids(self):
        data = 'event: message\ndata: {"method":"progress"}\n\ndata: {"id":9}\n\ndata: {"id":1,\ndata: "result":{"ok":true}}\n\n'
        self.assertTrue(decode_response(Response(data,"text/event-stream"),1)["result"]["ok"])
        with self.assertRaises(RuntimeError): decode_response(Response('data: {"id":2}\n\n',"text/event-stream"),1)

    def test_tool_discovery_and_required_fields_precede_dispatch(self):
        client = MCP("http://127.0.0.1:18765/mcp")
        client.request = Mock(return_value={"content":[{"type":"text","text":json.dumps({"tools":[{"name":"Example.run","inputSchema":{"required":["path"]}}]})}]})
        with self.assertRaises(ValueError): client.call("Example", "run", {})
        self.assertEqual(client.request.call_args.args[1]["name"], "describe_toolset")
        self.assertEqual(client.request.call_count, 1)


if __name__ == "__main__": unittest.main()
