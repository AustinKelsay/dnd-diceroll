#!/usr/bin/env python3
"""Dependency-free Unreal workspace commands for the validated Linux host (Python 3.11+)."""
import sys
if sys.version_info < (3, 11):
    sys.exit(f"Python 3.11+ required; current interpreter: {sys.executable} ({sys.version.split()[0]}).")

import argparse
import base64
from contextlib import contextmanager
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import signal
import struct
import subprocess
import time
import urllib.parse

from mcp_client import MCP
from scaffold import BASES, new_class
from workspace import Workspace, automation_result, entered_world, log_events, project_argument, read_ini, require_clean, source_digest, validate_bootstrap, validate_runtime

ROOT = Path(__file__).resolve().parents[1]
APP = "EditorToolset.EditorAppToolset"
SCENE = "editor_toolset.toolsets.scene.SceneTools"
STATUS = "unreal_starter.status.StarterWorkspaceTools"


# Linux (PlebDev Ultra) is the validated host. The Mac layout is kept for reference only.
HOSTS = {
    "linux": {"label": "linux/x86_64", "platform": "Linux",
              "engine_root": Path.home() / "Applications/UnrealEngine-5.8.4",
              "editor": "Binaries/Linux/UnrealEditor", "cmd": "Binaries/Linux/UnrealEditor-Cmd",
              "build": "Build/BatchFiles/Linux/Build.sh", "proxy": "Bin/Linux/unreal_mcp_proxy",
              "process_token": "/Binaries/Linux/UnrealEditor"},
    "mac": {"label": "darwin/arm64", "platform": "Mac",
            "engine_root": Path("/Users/Shared/Epic Games/UE_5.8"),
            "editor": "Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor", "cmd": "Binaries/Mac/UnrealEditor-Cmd",
            "build": "Build/BatchFiles/Mac/Build.sh", "proxy": "Bin/Mac/unreal_mcp_proxy",
            "process_token": "/Binaries/Mac/UnrealEditor"},
}


def current_host():
    if sys.platform.startswith("linux") and platform.machine() in ("x86_64", "AMD64"):
        return "linux"
    if sys.platform == "darwin" and platform.machine() == "arm64":
        return "mac"
    return None


def stamp():
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n")


class Dev:
    def __init__(self, workspace, host=None, cores=None):
        self.w = workspace
        self.supported = (host or current_host()) is not None
        self.host = host or current_host() or "linux"
        self.layout = HOSTS[self.host]
        self.platform = self.layout["platform"]
        self.cores = cores
        self.engine = Path(os.environ.get("UE_ROOT", self.layout["engine_root"])).resolve() / "Engine"
        self.editor = self.engine / self.layout["editor"]
        self.cmd = self.engine / self.layout["cmd"]
        self.endpoint = urllib.parse.urlparse(self.w.versions["mcp_url"])
        if self.endpoint.scheme != "http" or self.endpoint.hostname != "127.0.0.1" or not self.endpoint.port:
            raise ValueError("Unreal MCP endpoint must be http://127.0.0.1:<port>/mcp")

    def require_engine(self):
        build = json.loads((self.engine / "Build/Build.version").read_text())
        actual = ".".join(str(build[k]) for k in ("MajorVersion", "MinorVersion", "PatchVersion"))
        if actual != self.w.versions["unreal_engine"] or build["Changelist"] != self.w.versions["unreal_changelist"] or not self.editor.is_file():
            raise RuntimeError("Engine build differs from tools/versions.json; validate an upgrade first")
        return actual

    @contextmanager
    def operation(self):
        self.w.local.mkdir(exist_ok=True)
        with (self.w.local / "operation.lock").open("a") as lock:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError as exc:
                raise RuntimeError("Another helper operation owns this workspace") from exc
            yield

    def execute(self, args, **kwargs):
        return subprocess.run([str(a) for a in args], cwd=self.w.root, check=True, **kwargs)

    def processes(self):
        result = self.execute(["ps", "-axww", "-o", "pid=,command="], capture_output=True, text=True)
        processes = []
        for line in result.stdout.splitlines():
            parts = line.strip().split(None, 1)
            if len(parts) != 2 or self.layout["process_token"] not in parts[1]:
                continue
            # Only actual editor executables, not shell commands containing a path.
            command = parts[1]
            if not command.startswith("/") or command.startswith(("/bin/", "/usr/bin/")):
                continue
            pid = int(parts[0])
            owns = project_argument(command, self.w.project)
            if not owns:
                # Finder launches may omit the project argument; an open exact log
                # establishes workspace association but runtime identity is still required.
                opened = subprocess.run(["lsof", "-a", "-p", str(pid), "-Fn", str(self.w.log)], capture_output=True, text=True)
                owns = f"n{self.w.log}\n" in opened.stdout
            if owns:
                processes.append({"pid": pid, "command": command})
        return processes

    def listener(self):
        result = subprocess.run(["lsof", "-nP", "-a", "-iTCP:" + str(self.endpoint.port), "-sTCP:LISTEN", "-Fp"], capture_output=True, text=True)
        if result.returncode not in (0, 1):
            raise RuntimeError(f"Cannot inspect listener: {result.stderr.strip()}")
        pids = {int(x[1:]) for x in result.stdout.splitlines() if x.startswith("p")}
        if len(pids) > 1:
            raise RuntimeError("Multiple listener PIDs; editor ownership is ambiguous")
        return next(iter(pids), None)

    def os_identity(self):
        pid = self.listener()
        matches = [p for p in self.processes() if p["pid"] == pid]
        if len(matches) != 1 or not matches[0]["command"].startswith(str(self.editor) + " "):
            raise RuntimeError("MCP listener is not a verified instance of this project and pinned editor")
        return pid

    def require_closed(self):
        running = self.processes()
        if running:
            raise RuntimeError(f"This project is open in editor PID(s) {[p['pid'] for p in running]}; close it before this operation")

    def runtime(self, client, pid):
        status = json.loads(client.call(STATUS, "get_status"))
        validate_runtime(status, self.w, pid)
        return status

    @contextmanager
    def connected(self):
        self.require_engine()
        pid = self.os_identity()
        with MCP(self.w.versions["mcp_url"]) as client:
            status = self.runtime(client, pid)
            yield client, status

    def doctor(self):
        failures = []
        def check(label, action):
            try:
                value = action()
                print(f"OK   {label}: {value}")
            except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError, KeyError) as exc:
                failures.append(label)
                print(f"FAIL {label}: {exc}")
        print(f"INFO Python {platform.python_version()} at {sys.executable}")
        check("Host", self.check_host)
        check("Unreal", self.require_engine)
        check("Editor plugins", self.check_plugins)
        tools = [("Git LFS", ["git", "lfs", "version"])]
        if self.host == "linux":
            check("Clang (engine toolchain)", self.check_clang)
        else:
            tools += [("Xcode", ["xcodebuild", "-version"]), ("Metal", ["xcrun", "--find", "metal"])]
        for label, command in tools:
            check(label, lambda command=command: self.execute(command, capture_output=True, text=True).stdout.strip().replace("\n", "; "))
        check("MCP configurations", self.check_configs)
        if self.host == "linux":
            gpu = subprocess.run(["nvidia-smi", "--query-gpu=name,driver_version", "--format=csv,noheader"], capture_output=True, text=True) if shutil.which("nvidia-smi") else None
            print("INFO GPU: " + (gpu.stdout.strip() if gpu and gpu.returncode == 0 else "nvidia-smi unavailable; Vulkan GPU not inspected"))
            print("INFO Display: " + (os.environ.get("WAYLAND_DISPLAY") or os.environ.get("DISPLAY") or "none; `editor` needs a desktop session"))
        proxy = self.engine / "Plugins/Experimental/ModelContextProtocol/Extras/Proxy" / self.layout["proxy"]
        print("INFO Optional recovery proxy: " + (f"present, not validated: {proxy}" if proxy.is_file() else "not bundled; use direct HTTP recovery in docs/agents/unreal-tools.md"))
        print(f"INFO Project {self.w.project}; free space {shutil.disk_usage(self.w.root).free // 1024**3} GiB")
        if failures:
            raise RuntimeError("Doctor failed: " + ", ".join(failures))

    def check_host(self):
        if not self.supported:
            raise RuntimeError("This helper supports Linux x86_64 (validated) and Apple Silicon macOS (unvalidated)")
        if self.host != "linux":
            print("WARN Mac is no longer a validated host; verify on Linux (docs/development.md)")
        return f"{self.layout['label']} {platform.release()}"

    def check_clang(self):
        expected = self.w.versions.get("host_clang", "")
        found = sorted(self.engine.glob("Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/*/x86_64-unknown-linux-gnu/bin/clang++"))
        if len(found) != 1:
            raise RuntimeError("Expected exactly one bundled Linux clang toolchain in the engine")
        version = self.execute([found[0], "--version"], capture_output=True, text=True).stdout.splitlines()[0]
        if expected and expected not in version:
            raise RuntimeError(f"Bundled clang differs from tools/versions.json ({expected}): {version}")
        return version

    def native_limits(self, build=False):
        """Optional --cores cap so native jobs share a busy machine."""
        if not self.cores:
            return []
        return [f"-MaxParallelActions={self.cores}"] if build else [f"-corelimit={self.cores}"]

    def package_limits(self):
        """Carry --cores into UAT's compile (UBT actions), cook and IoStore (engine cores) children."""
        if not self.cores:
            return []
        return [f"-ubtargs=-MaxParallelActions={self.cores}", f"-AdditionalCookerOptions=-corelimit={self.cores}", f"-AdditionalIoStoreOptions=-corelimit={self.cores}"]

    def config_snapshot(self):
        return {p: p.read_bytes() for p in sorted((self.w.root / "Config").glob("*.ini"))}

    def require_config_unchanged(self, before):
        changed = [str(p.relative_to(self.w.root)) for p in sorted(set(before) | set(self.config_snapshot())) if not p.is_file() or before.get(p) != p.read_bytes()]
        if changed:
            raise RuntimeError(f"The engine rewrote tracked config {changed}; inspect `git diff` and restore before committing. The helper does not revert it.")

    def check_plugins(self):
        paths = {
            "ModelContextProtocol": "Experimental/ModelContextProtocol",
            "AllToolsets": "Experimental/Toolsets/AllToolsets",
            "PythonScriptPlugin": "Experimental/PythonScriptPlugin",
            "EditorScriptingUtilities": "Editor/EditorScriptingUtilities",
        }
        configured = {p["Name"]: p for p in json.loads(self.w.project.read_text()).get("Plugins", [])}
        for name, folder in paths.items():
            if not (self.engine / "Plugins" / folder / (name + ".uplugin")).is_file():
                raise RuntimeError(f"Bundled plugin missing: {name}")
            plugin = configured.get(name, {})
            if not plugin.get("Enabled") or plugin.get("TargetAllowList") != ["Editor"]:
                raise RuntimeError(f"Expected editor-only enabled plugin: {name}")
        # Its editor settings write a SecurityToken into tracked DefaultEngine.ini on every run.
        if configured.get("AndroidFileServer", {}).get("Enabled") is not False:
            raise RuntimeError("AndroidFileServer must stay disabled in the .uproject")
        return ", ".join(paths) + "; AndroidFileServer disabled"

    def check_configs(self):
        import tomllib
        url = self.w.versions["mcp_url"]
        codex = tomllib.loads((self.w.root / ".codex/config.toml").read_text())
        claude = json.loads((self.w.root / ".mcp.json").read_text())
        editor = read_ini(self.w.root / "Config/DefaultEditorPerProjectUserSettings.ini")["/Script/ModelContextProtocolEngine.ModelContextProtocolSettings"]
        if codex["mcp_servers"]["unreal"]["url"] != url or claude["mcpServers"]["unreal"]["url"] != url or int(editor["ServerPortNumber"]) != self.endpoint.port or editor["ServerUrlPath"] != self.endpoint.path:
            raise RuntimeError("MCP endpoint configurations disagree")
        if editor["bAutoStartServer"].lower() != "false":
            raise RuntimeError("MCP auto-start must be off for headless test isolation")
        return url

    def status(self):
        report = {"observed_at": stamp(), "project_file": str(self.w.project), "processes": self.processes(), "listener_pid": self.listener(), "verified": False}
        if report["listener_pid"]:
            try:
                with self.connected() as (client, state):
                    report.update(state, verified=True, current_level=client.call(SCENE, "get_current_level"))
            except (OSError, ValueError, RuntimeError, KeyError) as exc:
                report["reason"] = str(exc)
        write_json(self.w.local / "status.json", report)
        print(json.dumps(report, indent=2))
        if report["listener_pid"] and not report["verified"]:
            raise RuntimeError(report["reason"])

    def build(self):
        self.require_engine()
        self.require_closed()
        snapshot = source_digest(self.w)
        self.execute([self.engine / self.layout["build"], self.w.settings["editor_target"], self.platform, "Development", self.w.project, "-WaitMutex", "-NoHotReloadFromIDE", *self.native_limits(build=True)])
        if source_digest(self.w) != snapshot:
            raise RuntimeError("Source changed during the build; rerun build before launching")
        write_json(self.w.local / "build.json", self.build_identity(snapshot))

    def build_identity(self, snapshot=None):
        manifest = self.w.root / "Binaries" / self.platform / "UnrealEditor.modules"
        module_file = json.loads(manifest.read_text())["Modules"][self.w.settings["module"]]
        binary = manifest.parent / module_file
        return {"project_file": str(self.w.project), "source_digest": snapshot or source_digest(self.w),
                "binary_digest": hashlib.sha256(binary.read_bytes()).hexdigest(),
                "manifest_digest": hashlib.sha256(manifest.read_bytes()).hexdigest(),
                "engine": str(self.engine), "version": self.w.versions["unreal_engine"],
                "changelist": self.w.versions["unreal_changelist"], "target": self.w.settings["editor_target"]}

    def bootstrap(self):
        self.require_engine()
        self.require_closed()
        log = self.w.local / "bootstrap" / f"{stamp()}.log"
        log.parent.mkdir(exist_ok=True)
        config = self.config_snapshot()
        self.execute([self.cmd, self.w.project, "-run=pythonscript", f"-script={self.w.root / 'Content/Python/bootstrap_sandbox.py'}", f"-abslog={log}", "-unattended", "-nullrhi", "-nosplash", *self.native_limits()], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        self.require_config_unchanged(config)
        validate_bootstrap(log.read_text(errors="replace"))
        print(f"Sandbox validated; commandlet has zero errors/warnings. Log: {log}")

    def verify_skills(self):
        self.require_engine()
        self.require_closed()
        output = self.w.local / "skills" / stamp()
        output.mkdir(parents=True)
        log = output / "commandlet.log"
        script = self.w.root / "Content/Python/verify_project_skill.py"
        config = self.config_snapshot()
        self.execute([self.cmd, self.w.project, "-run=pythonscript", f"-script={script}",
                      f"-abslog={log}", "-unattended", "-nullrhi", "-nosplash", *self.native_limits()],
                     stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        self.require_config_unchanged(config)
        content = log.read_text(errors="replace")
        matches = re.findall(r"PROJECT_SKILL_VERIFIED (\{[^\r\n]+\})", content)
        if len(matches) != 1 or log_events(content)["errors"]:
            raise RuntimeError(f"Project skill readback failed; inspect {log}")
        report = json.loads(matches[0])
        expected_engine = f"{self.w.versions['unreal_engine']}-{self.w.versions['unreal_changelist']}+"
        source = self.w.root / "docs/agents/editor-authoring.md"
        if (Path(report["project_file"]).resolve() != self.w.project
                or not report["engine_version"].startswith(expected_engine)
                or report["instructions_sha256"] != hashlib.sha256(source.read_text(encoding="utf-8").encode("utf-8")).hexdigest()):
            raise RuntimeError(f"Project skill readback differs from this checkout/source/engine; inspect {log}")
        write_json(output / "verified.json", report)
        print(f"Project skill discovery/readback verified; live MCP not exercised. Evidence: {output}")

    def launch(self, wait=True):
        self.require_engine()
        self.check_configs()
        self.require_closed()
        if self.listener():
            raise RuntimeError("MCP port is occupied by another process")
        receipt = self.w.local / "build.json"
        if not receipt.is_file() or json.loads(receipt.read_text()) != self.build_identity():
            raise RuntimeError("Editor build evidence is missing or stale; run build first")
        with (self.w.local / "editor-launch.log").open("ab") as log:
            process = subprocess.Popen([str(self.editor), str(self.w.project), "-ModelContextProtocolStartServer", f"-ModelContextProtocolPort={self.endpoint.port}", f"-abslog={self.w.log}", "-nosplash", *self.native_limits()], cwd=self.w.root, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        write_json(self.w.local / "editor.json", {"pid": process.pid, "project_file": str(self.w.project), "mcp_url": self.w.versions["mcp_url"], "launched_at": stamp()})
        if wait:
            deadline = time.monotonic() + 180
            last_error = "Editor not ready"
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError(f"Editor exited: inspect {self.w.log}")
                try:
                    with self.connected() as (client, state):
                        current = client.call(SCENE, "get_current_level")
                        if current != self.w.startup_map:
                            raise RuntimeError(f"Expected startup map {self.w.startup_map}; got {current}")
                        print(f"Editor ready: PID {state['pid']}, {current}")
                        return
                except (OSError, ValueError, RuntimeError, KeyError) as exc:
                    last_error = str(exc)
                    time.sleep(1)
            raise RuntimeError(f"Editor readiness timed out; editor left running. {last_error}")
        print(f"Started PID {process.pid}; readiness not checked")

    def close(self, request_quit=False):
        self.require_engine()
        if request_quit and self.host == "linux":
            raise RuntimeError("--request-quit needs a save dialog, which the Linux quit path does not show; use close")
        pid = self.os_identity()
        if not request_quit:
            with self.connected() as (client, state):
                require_clean(state)
                # Re-read immediately before requesting a normal application quit.
                require_clean(self.runtime(client, pid))
        if self.os_identity() != pid:
            raise RuntimeError("Editor identity changed before quit")
        if self.host == "linux":
            # Unreal's Unix handler turns the first SIGTERM into a normal engine exit
            # request. A second signal forces exit, so send exactly one and only after the
            # PIE/dirty gate above: this path shows no save dialog. Never SIGKILL.
            os.kill(pid, signal.SIGTERM)
        else:
            # NSRunningApplication.terminate sends a normal quit request, honoring
            # Unreal's dirty/save dialogs. Never use forceTerminate, SIGTERM or QUIT_EDITOR.
            script = 'ObjC.import("AppKit"); $.NSRunningApplication.runningApplicationWithProcessIdentifier(' + str(pid) + ').terminate;'
            result = self.execute(["osascript", "-l", "JavaScript", "-e", script], capture_output=True, text=True)
            if result.stdout.strip() != "true":
                raise RuntimeError("macOS declined the normal quit request")
        deadline = time.monotonic() + 120
        while time.monotonic() < deadline:
            if not any(p["pid"] == pid for p in self.processes()) and self.listener() != pid:
                print(f"Editor PID {pid} exited normally")
                return
            time.sleep(0.5)
        raise RuntimeError("Editor did not exit in time. Check it in Unreal; do not signal it again. No force quit attempted.")

    def smoke(self):
        report_path = self.w.local / "smoke-result.json"
        report_path.unlink(missing_ok=True)
        offset = self.w.log.stat().st_size
        with self.connected() as (client, state):
            require_clean(state)
            level = client.call(SCENE, "get_current_level")
            if level != self.w.startup_map:
                raise RuntimeError(f"Expected {self.w.startup_map}, found {level}")
            actors = client.call(SCENE, "find_actors", {"name": "SandboxFloor", "tag": "", "collision_channels": []})
            exact = [a for a in actors if client.call("editor_toolset.toolsets.actor.ActorTools", "get_label", {"actor": a}) == "SandboxFloor"]
            if len(exact) != 1:
                raise RuntimeError("Expected exactly one actor labeled SandboxFloor")
            original = None
            try:
                client.call(APP, "StartPIE", {"options": {"bSimulate": False, "playMode": "PlayMode_InViewPort", "warmupSeconds": 1}})
                if client.call(APP, "IsPIERunning") is not True:
                    raise RuntimeError("PIE did not reach running state")
            except Exception as exc:
                original = exc
            cleanup = None
            try:
                if client.call(APP, "IsPIERunning"):
                    client.call(APP, "StopPIE")
                if client.call(APP, "IsPIERunning"):
                    raise RuntimeError("PIE did not stop")
            except Exception as exc:
                cleanup = exc
            if original or cleanup:
                write_json(self.w.local / "smoke-failure.json", {
                    "verified_at": stamp(), **state, "level": level,
                    "failure": str(original) if original else None,
                    "cleanup_failure": str(cleanup) if cleanup else None,
                    "pie_state": "unknown" if cleanup else "stopped",
                })
                raise RuntimeError(f"PIE failure: {original or 'none'}; cleanup: {cleanup or 'restored edit mode'}" + ("; PIE state unknown" if cleanup else "")) from original
            events = self.new_log_events(offset)
            report = {"verified_at": stamp(), **state, "level": level, "pie_started": True, "pie_stopped": True, **events}
            if events["errors"]:
                write_json(self.w.local / "smoke-failure.json", report)
                raise RuntimeError(f"New PIE errors; see {self.w.local / 'smoke-failure.json'}")
            write_json(report_path, report)
            print(json.dumps(report, indent=2))

    def new_log_events(self, offset):
        with self.w.log.open("rb") as stream:
            if self.w.log.stat().st_size < offset:
                raise RuntimeError("Editor log rotated during operation; evidence boundary is invalid")
            stream.seek(offset)
            return log_events(stream.read().decode(errors="replace"))

    def schemas(self, toolset):
        with self.connected() as (client, state):
            output = self.w.local / "mcp-schemas" / self.w.versions["unreal_engine"]
            write_json(output / "catalog.json", client.catalog)
            write_json(output / (toolset + ".json"), client.describe(toolset))
            print(f"Schemas saved in {output}")

    def capture(self):
        with self.connected() as (client, state):
            if state["pie_active"]:
                raise RuntimeError("Return to edit mode before capture")
            transform = client.call(APP, "GetCameraTransform")
            result = client.call(APP, "CaptureViewport", {"captureTransform": transform, "annotations": {
                "gridSpacing": 0, "gridExtent": 0, "gridHeight": 0, "maxLabelDistance": 0,
                "classFilter": {"refPath": "/Script/Engine.Actor"}, "maxLabels": 0}, "bShowUI": False})
            data = base64.b64decode(result["image"]["data"], validate=True)
            if data[:8] != b"\x89PNG\r\n\x1a\n" or len(data) < 24:
                raise RuntimeError("Viewport capture did not return a PNG")
            width, height = struct.unpack(">II", data[16:24])
            if not width or not height:
                raise RuntimeError("Viewport capture is empty")
            output = self.w.local / "captures" / f"{stamp()}.png"
            output.parent.mkdir(exist_ok=True)
            output.write_bytes(data)
            print(f"Captured {width}x{height}: {output}")

    def run_bounded(self, args, log, timeout):
        """Run an owned headless engine process; on timeout leave it running and report it."""
        with log.open("ab") as stream:
            process = subprocess.Popen([str(a) for a in args], cwd=self.w.root, stdout=stream, stderr=subprocess.STDOUT)
            try:
                return process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                raise RuntimeError(f"PID {process.pid} still running after {timeout}s; left running for a normal shutdown, not killed. Inspect {log}") from None

    def automation_command(self, pattern, output, log):
        common = [self.cmd, self.w.project, f"-ReportExportPath={output}", f"-abslog={log}", "-unattended", "-nosound", "-nosplash"]
        if self.host == "linux":
            # -nullrhi skips SDL init (Slate then warns about Wayland), and -TestExit
            # force-exits 1 on Unix. Offscreen rendering plus SoftQuit exits normally.
            return [*common, f"-ExecCmds=Automation RunTests {pattern};SoftQuit", "-RenderOffscreen", *self.native_limits()]
        return [*common, f"-ExecCmds=Automation RunTests {pattern}", "-TestExit=Automation Test Queue Empty", "-nullrhi"]

    def test(self, pattern, allow_warnings=False):
        self.require_engine()
        self.require_closed()
        if not re.fullmatch(r"[A-Za-z0-9_.]+", pattern):
            raise ValueError("Use one explicit automation prefix consisting of letters, digits, dots and underscores")
        output = self.w.local / "automation" / stamp()
        output.mkdir(parents=True)
        log = output / "automation.log"
        config = self.config_snapshot()
        code = self.run_bounded(self.automation_command(pattern, output, log), output / "console.log", timeout=300)
        self.require_config_unchanged(config)
        report_path = output / "index.json"
        if not report_path.is_file():
            raise RuntimeError(f"Automation produced no test report (possibly zero matched tests); inspect {log}")
        report = automation_result(json.loads(report_path.read_text(encoding="utf-8-sig")))
        events = log_events(log.read_text(errors="replace"))
        if events["errors"]:
            raise RuntimeError(f"Automation logged errors: {events['errors']}")
        if code != 0:
            raise RuntimeError(f"Engine exited {code} after the tests; inspect {log}")
        write_json(output / "verified.json", {**report, "filter": pattern, "project": str(self.w.project), "exit_code": code, "warnings": events["warnings"]})
        if events["warnings"] and not allow_warnings:
            raise RuntimeError(f"{len(events['warnings'])} warning line(s); the gate is zero. First: {events['warnings'][0]} Inspect {log}")
        print(f"Automation passed: {report}; warning lines: {len(events['warnings'])}; evidence: {output}")

    def packaged_binary(self, output):
        target = self.w.settings["game_target"]
        if self.host == "linux":
            found = [p for p in output.glob(f"**/Binaries/Linux/{target}") if p.is_file() and os.access(p, os.X_OK)]
        else:
            found = [p for p in output.glob(f"**/*.app/Contents/MacOS/{target}") if p.is_file()]
        if len(found) != 1:
            raise RuntimeError(f"Cannot identify the packaged {target} executable in {output}: {found}")
        return found[0]

    def package(self):
        self.require_engine()
        self.require_closed()
        output = self.w.root / "Builds" / stamp()
        output.mkdir(parents=True)
        log = output / "package.log"
        config = self.config_snapshot()
        with log.open("w") as stream:
            self.execute([self.engine / "Build/BatchFiles/RunUAT.sh", "BuildCookRun", f"-project={self.w.project}", f"-platform={self.platform}", "-clientconfig=Development", f"-target={self.w.settings['game_target']}", "-build", "-cook", "-stage", "-pak", "-package", "-archive", f"-archivedirectory={output}", "-unattended", "-nop4", "-utf8output", *self.package_limits()], stdout=stream, stderr=subprocess.STDOUT)
        self.require_config_unchanged(config)
        binary = self.packaged_binary(output)
        # Offscreen keeps the real RHI path; Linux -nullrhi skips SDL and adds a warning.
        render = ["-RenderOffscreen"] if self.host == "linux" else ["-nullrhi"]
        launch_log = output / "launch.log"
        with launch_log.open("w") as stream:
            process = subprocess.Popen([str(binary), "-unattended", *render, "-nosound", "-stdout", "-FullStdOutLogOutput", *self.native_limits()], cwd=binary.parent, stdout=stream, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 90
                loaded = False
                while time.monotonic() < deadline:
                    if process.poll() is not None:
                        raise RuntimeError(f"Packaged game exited early: {process.returncode}")
                    text = launch_log.read_text(errors="replace") if launch_log.exists() else ""
                    if log_events(text)["errors"]:
                        raise RuntimeError(f"Packaged startup logged errors; inspect {launch_log}")
                    if entered_world(text, self.w.game_map):
                        loaded = True
                        break
                    time.sleep(0.5)
                if not loaded:
                    raise RuntimeError(f"Packaged game did not enter the startup map; inspect {launch_log}")
            finally:
                # This is the packaged process created above, never an editor. One SIGTERM
                # is Unreal's normal exit request on Linux and macOS; it is not escalated.
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=30)
                    except subprocess.TimeoutExpired:
                        raise RuntimeError(f"Packaged game PID {process.pid} did not exit after one SIGTERM; left running") from None
        text = launch_log.read_text(errors="replace")
        events = log_events(text)
        if events["errors"]:
            raise RuntimeError(f"Packaged startup logged errors: {events['errors']}")
        exited = "LogExit: Exiting." in text
        if not exited:
            raise RuntimeError(f"Packaged game did not log a normal exit; inspect {launch_log}")
        write_json(output / "verified.json", {"binary": str(binary), "startup_map": self.w.game_map, "launched_at": stamp(), "exit_code": process.returncode, "normal_exit_logged": exited, **events})
        print(f"Packaged game entered {self.w.game_map} and exited normally (code {process.returncode}); warning lines: {len(events['warnings'])}; evidence: {output}")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", help="Select a root .uproject if the workspace has more than one")
    parser.add_argument("--cores", type=int, help="Cap build actions / engine cores so native jobs share the machine (e.g. 2)")
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("doctor", "status", "build", "bootstrap", "smoke", "capture", "package", "relaunch", "verify-skills"):
        commands.add_parser(name)
    launch = commands.add_parser("editor")
    launch.add_argument("--no-wait", action="store_true", help="Skip readiness checks")
    close = commands.add_parser("close")
    close.add_argument("--request-quit", action="store_true", help="Mac only: normal UI quit without the status-tool dirty gate; honors save dialogs")
    schemas = commands.add_parser("schemas")
    schemas.add_argument("toolset")
    test = commands.add_parser("test")
    test.add_argument("--filter", default=None)
    test.add_argument("--allow-warnings", action="store_true", help="Report warning lines without failing (the merge gate is zero)")
    scaffold = commands.add_parser("new-class")
    scaffold.add_argument("kind", choices=BASES)
    scaffold.add_argument("name", help="PascalCase class name without Unreal's A/U prefix")
    raw = commands.add_parser("mcp")
    raw.add_argument("--method", default="tools/list")
    raw.add_argument("--params", default="{}")
    args = parser.parse_args()
    dev = Dev(Workspace.discover(ROOT, args.project), cores=args.cores)
    with dev.operation():
        if args.command == "editor":
            dev.launch(wait=not args.no_wait)
        elif args.command == "close":
            dev.close(args.request_quit)
        elif args.command == "schemas":
            dev.schemas(args.toolset)
        elif args.command == "test":
            dev.test(args.filter or dev.w.settings["automation_filter"], args.allow_warnings)
        elif args.command == "new-class":
            for path in new_class(dev.w, args.kind, args.name):
                print(path)
            print("Run close, build, then editor to compile and load the class")
        elif args.command == "relaunch":
            if dev.processes():
                dev.close()
            dev.build()
            dev.launch()
        elif args.command == "verify-skills":
            dev.verify_skills()
        elif args.command == "mcp":
            with dev.connected() as (client, state):
                print(json.dumps({"identity": state, "result": client.request(args.method, json.loads(args.params))}, indent=2))
        else:
            getattr(dev, args.command)()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError, KeyError, subprocess.SubprocessError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)
