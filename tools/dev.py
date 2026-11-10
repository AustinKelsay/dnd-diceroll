#!/usr/bin/env python3
"""Dependency-free Unreal workspace commands for the validated Linux host (Python 3.11+)."""
import sys
if sys.version_info < (3, 11):
    sys.exit(f"Python 3.11+ required; current interpreter: {sys.executable} ({sys.version.split()[0]}).")

import argparse
import base64
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import signal
import socket
import struct
import subprocess
import time
import urllib.parse

from mcp_client import MCP
import perf as perf_tools
from scaffold import BASES, new_class
from workspace import Workspace, automation_result, entered_world, log_events, project_argument, read_ini, require_clean, source_digest, validate_bootstrap, validate_runtime

ROOT = Path(__file__).resolve().parents[1]
APP = "EditorToolset.EditorAppToolset"
SCENE = "editor_toolset.toolsets.scene.SceneTools"
STATUS = "unreal_starter.status.StarterWorkspaceTools"


# Linux (PlebDev Ultra) is the one validated host. Mac still runs through the same
# commands but is no longer validated; other hosts get guidance only.
HOSTS = {
    "linux": {"label": "linux/x86_64", "platform": "Linux",
              "engine_root": Path.home() / "Applications/UnrealEngine-5.8.4",
              "editor": "Binaries/Linux/UnrealEditor", "cmd": "Binaries/Linux/UnrealEditor-Cmd",
              "build": "Build/BatchFiles/Linux/Build.sh", "proxy": "Bin/Linux/unreal_mcp_proxy",
              "process_token": "/Binaries/Linux/UnrealEditor",
              # -nullrhi skips SDL setup (Slate then warns about Wayland); offscreen keeps the real RHI path.
              "headless": ["-RenderOffscreen"],
              # -TestExit force-exits 1 on Unix; SoftQuit exits normally with the test status.
              "automation": lambda prefix: [f"-ExecCmds=Automation RunTests {prefix};SoftQuit", "-RenderOffscreen"],
              "packaged": "**/Binaries/Linux/{target}",
              "tools": ["git", "ps", "lsof"],
              "quit": "quit_by_signal", "save_dialog": False,
              "version_tools": []},
    "mac": {"label": "darwin/arm64", "platform": "Mac",
            "engine_root": Path("/Users/Shared/Epic Games/UE_5.8"),
            "editor": "Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor", "cmd": "Binaries/Mac/UnrealEditor-Cmd",
            "build": "Build/BatchFiles/Mac/Build.sh", "proxy": "Bin/Mac/unreal_mcp_proxy",
            "process_token": "/Binaries/Mac/UnrealEditor",
            "headless": ["-nullrhi"],
            "automation": lambda prefix: [f"-ExecCmds=Automation RunTests {prefix}", "-TestExit=Automation Test Queue Empty", "-nullrhi"],
            "packaged": "**/*.app/Contents/MacOS/{target}",
            "tools": ["git", "ps", "lsof", "osascript"],
            "quit": "quit_by_app", "save_dialog": True,
            "version_tools": [("Xcode", ["xcodebuild", "-version"]), ("Metal", ["xcrun", "--find", "metal"])]},
}

TOOL_HINTS = {
    "lsof": "install lsof (Arch: `sudo pacman -S lsof`; Debian/Ubuntu: `sudo apt install lsof`)",
    "ps": "install procps (Arch: `sudo pacman -S procps-ng`; Debian/Ubuntu: `sudo apt install procps`)",
    "git": "install Git and Git LFS",
    "osascript": "part of macOS",
    "xcodebuild": "install Xcode", "xcrun": "install Xcode",
}

OFF_ULTRA = ("Not on PlebDev Ultra: Unreal results from this machine do not count as verification. "
             "Do source and docs work here, state your host in the PR, and the maintainer verifies "
             "Unreal work on Ultra before merge (CONTRIBUTING.md).")

# --cores reaches these commands; others refuse it so a cap is never silently dropped.
CORE_COMMANDS = ("build", "test", "bootstrap", "verify-skills", "editor", "relaunch", "package", "gallery", "perf")


# Capture gallery: fixtures written by `test --filter Project.Tools.CaptureFixtures`, rendered by the
# game's own capture mode (-ExpeditionCapture). Pose rules mirror ExpeditionCapture::ParsePoses.
GALLERY_FIXTURES = ("default-seed-battle", "default-seed-enemy-turn", "board-16x11", "board-18x12", "board-vigil-pillars", "board-vigil-walls", "board-root-r002", "default-seed-route")
GALLERY_RESOLUTIONS = ("1920x1080", "1280x720")
GALLERY_POSES = Path("tools/capture/poses.json")
CAPTURE_MODES = ("battle", "targeting", "overview", "start", "tooltip", "hint")
CAPTURE_ACTIONS = ("attack", "special1", "special2", "rescue", "potion")
CAPTURE_HINTS = ("move", "resources", "rescue", "endturn")
# Reference units: cm of the 110 cm r001 cell. The game scales them by the board metric (cell / 110).
CAMERA_DISTANCE = (900, 6400)
FIXTURE_PATTERN = re.compile(r"[a-z0-9][a-z0-9-]*")
FIXTURE_HINT = "run `python3 tools/dev.py --cores 2 test --filter Project.Tools.CaptureFixtures` to write them"

# Performance gate (#148): the game's autoplay mode plays one seeded fight (-ExpeditionAutoplay), the CSV profiler records it.
PERF_SCRIPT = Path("tools/autoplay/watchtower-road.json")
PERF_FRAMES = (1000, 2000000)
PERF_PASSES = ("warmup", "measured", "resume", "report")


def parse_resolution(text):
    match = re.fullmatch(r"(\d{3,5})x(\d{3,5})", str(text))
    if not match:
        raise ValueError(f"Resolution {text!r} must look like 1920x1080")
    width, height = int(match.group(1)), int(match.group(2))
    if not (640 <= width <= 7680 and 360 <= height <= 4320):
        raise ValueError(f"Resolution {text} is outside 640x360 to 7680x4320")
    return width, height


def resolution_argument(text):
    try:
        parse_resolution(text)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(str(exc)) from None
    return text


def load_poses(path):
    """Validate a poses file the way the game does and return its shots."""
    try:
        data = json.loads(Path(path).read_text())
    except (OSError, ValueError) as exc:
        raise ValueError(f"Cannot read poses {path}: {exc}") from None
    if not isinstance(data, dict) or data.get("version") != 1 or isinstance(data.get("version"), bool):
        raise ValueError(f"Poses {path} needs an object with \"version\": 1")
    settle = data.get("settleFrames", 60)
    if isinstance(settle, bool) or not isinstance(settle, int) or not 1 <= settle <= 1200:
        raise ValueError("\"settleFrames\" must be a whole number from 1 to 1200")
    shots = data.get("shots")
    if not isinstance(shots, list) or not shots:
        raise ValueError("Poses need a non-empty \"shots\" list")
    names = set()
    for index, shot in enumerate(shots):
        if not isinstance(shot, dict):
            raise ValueError(f"Shot {index} is not an object")
        name = shot.get("name")
        if not isinstance(name, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", name):
            raise ValueError(f"Shot {index} needs a \"name\" of letters, digits, '-' and '_' (up to 64)")
        if name in names:
            raise ValueError(f"Shot name {name!r} is used twice")
        names.add(name)
        if shot.get("mode") not in CAPTURE_MODES:
            raise ValueError(f"{name} needs a \"mode\" of {', '.join(CAPTURE_MODES)}")
        if "build" in shot and (shot["mode"] != "start" or not isinstance(shot["build"], str) or not re.fullmatch(r"[a-z0-9.-]{1,64}", shot["build"])):
            raise ValueError(f"{name} may give a \"build\" of lowercase letters, digits, '.' and '-' on a start shot only")
        if shot["mode"] == "tooltip" and shot.get("action") not in CAPTURE_ACTIONS:
            raise ValueError(f"{name} needs an \"action\" of {', '.join(CAPTURE_ACTIONS)}")
        if shot["mode"] == "hint" and shot.get("hint") not in CAPTURE_HINTS:
            raise ValueError(f"{name} needs a \"hint\" of {', '.join(CAPTURE_HINTS)}")
        if shot["mode"] in ("battle", "targeting", "tooltip", "hint"):
            yaw, distance = shot.get("yaw"), shot.get("distance")
            if isinstance(yaw, bool) or not isinstance(yaw, (int, float)) or not -360 <= yaw <= 360:
                raise ValueError(f"{name} needs a \"yaw\" from -360 to 360")
            ok = distance in ("default", "far") or (isinstance(distance, (int, float)) and not isinstance(distance, bool) and CAMERA_DISTANCE[0] <= distance <= CAMERA_DISTANCE[1])
            if not ok:
                raise ValueError(f"{name} needs a \"distance\" of \"default\", \"far\" or a number from {CAMERA_DISTANCE[0]} to {CAMERA_DISTANCE[1]}")
    return shots


def png_size(path):
    data = Path(path).read_bytes()[:24]
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path} is not a PNG")
    return struct.unpack(">II", data[16:24])


def verify_capture(manifest_path, shots, resolution, directory):
    """Check one game run's manifest and PNGs against the poses and requested size; return the manifest."""
    if not manifest_path.is_file():
        raise RuntimeError(f"The game wrote no manifest ({manifest_path}); inspect its log")
    manifest = json.loads(manifest_path.read_text())
    if manifest.get("status") != "ok":
        raise RuntimeError(f"Capture failed in the game: {manifest.get('error', manifest.get('status'))}")
    wanted = [shot["name"] for shot in shots]
    found = {shot.get("name"): shot for shot in manifest.get("shots", [])}
    missing = [name for name in wanted if name not in found]
    if missing:
        raise RuntimeError(f"Manifest lacks shot(s) {missing}")
    captured = 0
    for name in wanted:
        entry = found[name]
        if entry.get("status") == "skipped":
            continue
        picture = directory / str(entry.get("file", ""))
        if entry.get("status") != "captured" or not picture.is_file() or picture.stat().st_size == 0:
            raise RuntimeError(f"Shot {name} has no picture ({picture})")
        size = png_size(picture)
        if size != resolution:
            raise RuntimeError(f"Shot {name} is {size[0]}x{size[1]}, not the requested {resolution[0]}x{resolution[1]}")
        if "camera" not in entry:
            raise RuntimeError(f"Shot {name} has no camera transform")
        captured += 1
    if not captured:
        raise RuntimeError("Every shot was skipped; the fixture does not suit these poses")
    return manifest


def current_host():
    if sys.platform.startswith("linux") and platform.machine() == "x86_64":
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
        if cores is not None and cores < 1:
            raise ValueError("--cores must be 1 or more")
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
        import fcntl  # Unix only; imported here so doctor can still guide other hosts.
        with (self.w.local / "operation.lock").open("a") as lock:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError as exc:
                raise RuntimeError("Another helper operation owns this workspace") from exc
            yield

    def tool(self, name):
        path = shutil.which(name)
        if not path:
            raise RuntimeError(f"`{name}` is not installed or not on PATH; {TOOL_HINTS.get(name, 'install it')}")
        return path

    def execute(self, args, **kwargs):
        args = [str(a) for a in args]
        if "/" not in args[0]:
            args[0] = self.tool(args[0])
        return subprocess.run(args, cwd=self.w.root, check=True, **kwargs)

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
                opened = subprocess.run([self.tool("lsof"), "-a", "-p", str(pid), "-Fn", str(self.w.log)], capture_output=True, text=True)
                owns = f"n{self.w.log}\n" in opened.stdout
            if owns:
                processes.append({"pid": pid, "command": command})
        return processes

    def listener(self):
        result = subprocess.run([self.tool("lsof"), "-nP", "-a", "-iTCP:" + str(self.endpoint.port), "-sTCP:LISTEN", "-Fp"], capture_output=True, text=True)
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

    def wait_for_free_port(self, timeout=90):
        """Wait out sockets still holding the MCP port (e.g. TIME_WAIT after the last session).

        Unreal's listener binds without address reuse, so an editor started too soon after
        the previous one runs without MCP. A plain test bind sees the same conflict."""
        deadline = time.monotonic() + timeout
        while True:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
                try:
                    probe.bind(("127.0.0.1", self.endpoint.port))
                    return
                except OSError:
                    pass
            if time.monotonic() >= deadline:
                raise RuntimeError(f"Port {self.endpoint.port} is still held (e.g. TIME_WAIT); wait a minute and retry")
            time.sleep(1)

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
        if self.host != "linux" or not self.supported:
            print("NOTE " + OFF_ULTRA)
        if not self.supported:
            print(f"SKIP Unreal checks: {sys.platform}/{platform.machine()} has no helper support (Windows is #51)")
            return
        check("Host", self.check_host)
        for name in self.layout["tools"]:
            check(f"Tool {name}", lambda name=name: self.tool(name))
        check("Unreal", self.require_engine)
        check("Editor plugins", self.check_plugins)
        tools = [("Git LFS", ["git", "lfs", "version"]), *self.layout["version_tools"]]
        if self.host == "linux":
            check("Clang (engine toolchain)", self.check_clang)
        for label, command in tools:
            check(label, lambda command=command: self.execute(command, capture_output=True, text=True).stdout.strip().replace("\n", "; "))
        check("MCP configurations", self.check_configs)
        if self.host == "linux":
            try:
                gpu = subprocess.run(["nvidia-smi", "--query-gpu=name,driver_version", "--format=csv,noheader"], capture_output=True, text=True, timeout=15) if shutil.which("nvidia-smi") else None
            except (OSError, subprocess.TimeoutExpired):
                gpu = None  # informational only; never fail doctor on the GPU probe
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
        return f"{self.layout['label']} {platform.release()}" + ("" if self.host == "linux" else " (not validated)")

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
        return {p: p.read_bytes() for p in sorted((self.w.root / "Config").rglob("*")) if p.is_file()}

    @contextmanager
    def config_guard(self):
        """Report any engine write under Config/, even when the engine run itself failed."""
        before = self.config_snapshot()
        try:
            yield
        finally:
            after = self.config_snapshot()
            changed = [str(p.relative_to(self.w.root)) for p in sorted(set(before) | set(after)) if before.get(p) != after.get(p)]
            if changed:
                original = sys.exc_info()[1]
                message = f"The engine changed tracked config {changed}; inspect `git diff` and restore it before committing. The helper does not revert it."
                if original:
                    message += f" The run also failed: {original}"
                raise RuntimeError(message) from original

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
        log = self.w.local / "build" / f"{stamp()}.log"
        log.parent.mkdir(parents=True, exist_ok=True)
        log.touch()
        with self.config_guard():
            code = self.run_logged([self.engine / self.layout["build"], self.w.settings["editor_target"], self.platform, "Development", self.w.project, "-WaitMutex", "-NoHotReloadFromIDE", *self.native_limits(build=True)], log)
        warnings = build_warnings(log.read_text(errors="replace"))
        if code != 0:
            raise RuntimeError(f"Build failed with exit {code}; inspect {log}")
        if warnings:
            raise RuntimeError(f"{len(warnings)} build warning line(s); a clean build has none. First: {warnings[0]} Inspect {log}")
        if source_digest(self.w) != snapshot:
            raise RuntimeError("Source changed during the build; rerun build before launching")
        write_json(self.w.local / "build.json", self.build_identity(snapshot))
        print(f"Build passed with 0 warnings; log: {log}")

    def run_logged(self, args, log):
        """Stream a child's output to the console and a log file; return its exit code."""
        with log.open("w") as stream:
            process = subprocess.Popen([str(a) for a in args], cwd=self.w.root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
            for line in process.stdout:
                sys.stdout.write(line)
                stream.write(line)
            return process.wait()

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
        with self.config_guard():
            code = self.run_bounded([self.cmd, self.w.project, "-run=pythonscript", f"-script={self.w.root / 'Content/Python/bootstrap_sandbox.py'}", f"-abslog={log}", "-unattended", *self.layout["headless"], "-nosplash", *self.native_limits()], log.with_suffix(".console.log"), timeout=300)
        if code != 0:
            raise RuntimeError(f"Bootstrap commandlet exited {code}; inspect {log}")
        validate_bootstrap(log.read_text(errors="replace"))
        print(f"Sandbox validated; commandlet has zero errors/warnings. Log: {log}")

    def verify_skills(self):
        self.require_engine()
        self.require_closed()
        output = self.w.local / "skills" / stamp()
        output.mkdir(parents=True)
        log = output / "commandlet.log"
        script = self.w.root / "Content/Python/verify_project_skill.py"
        with self.config_guard():
            code = self.run_bounded([self.cmd, self.w.project, "-run=pythonscript", f"-script={script}",
                                     f"-abslog={log}", "-unattended", *self.layout["headless"], "-nosplash", *self.native_limits()],
                                    output / "console.log", timeout=300)
        content = log.read_text(errors="replace")
        matches = re.findall(r"PROJECT_SKILL_VERIFIED (\{[^\r\n]+\})", content)
        events = log_events(content)
        if code != 0 or len(matches) != 1 or events["errors"]:
            raise RuntimeError(f"Project skill readback failed; inspect {log}")
        report = json.loads(matches[0])
        expected_engine = f"{self.w.versions['unreal_engine']}-{self.w.versions['unreal_changelist']}+"
        source = self.w.root / "docs/agents/editor-authoring.md"
        if (Path(report["project_file"]).resolve() != self.w.project
                or not report["engine_version"].startswith(expected_engine)
                or report["instructions_sha256"] != hashlib.sha256(source.read_text(encoding="utf-8").encode("utf-8")).hexdigest()):
            raise RuntimeError(f"Project skill readback differs from this checkout/source/engine; inspect {log}")
        write_json(output / "verified.json", {**report, "warnings": events["warnings"]})
        if events["warnings"]:
            raise RuntimeError(f"{len(events['warnings'])} warning line(s) in the skill check; first: {events['warnings'][0]} Inspect {log}")
        print(f"Project skill discovery/readback verified with 0 warnings; live MCP not exercised. Evidence: {output}")

    def launch(self, wait=True):
        self.require_engine()
        self.check_configs()
        self.require_closed()
        if self.listener():
            raise RuntimeError("MCP port is occupied by another process")
        self.wait_for_free_port()
        receipt = self.w.local / "build.json"
        if not receipt.is_file() or json.loads(receipt.read_text()) != self.build_identity():
            raise RuntimeError("Editor build evidence is missing or stale; run build first")
        started = time.time()
        with (self.w.local / "editor-launch.log").open("ab") as log:
            process = subprocess.Popen([str(self.editor), str(self.w.project), "-ModelContextProtocolStartServer", f"-ModelContextProtocolPort={self.endpoint.port}", f"-abslog={self.w.log}", "-nosplash", *self.native_limits()], cwd=self.w.root, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        write_json(self.w.local / "editor.json", {"pid": process.pid, "project_file": str(self.w.project), "mcp_url": self.w.versions["mcp_url"], "launched_at": stamp()})
        if wait:
            deadline = time.monotonic() + 180
            last_error = "Editor not ready"
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError(f"Editor exited: inspect {self.w.log}")
                if self.w.log.is_file() and self.w.log.stat().st_mtime >= started and "HttpListener unable to bind" in self.w.log.read_text(errors="replace"):
                    raise RuntimeError(f"Editor PID {process.pid} could not open MCP port {self.endpoint.port} and is running without MCP. "
                                       "The helper cannot check it for unsaved work; close it from its window.")
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
        if request_quit and not self.layout["save_dialog"]:
            raise RuntimeError("--request-quit relies on a save dialog, which this host's quit path does not show; use close")
        pid = self.os_identity()
        if not request_quit:
            with self.connected() as (client, state):
                require_clean(state)
                # Re-read immediately before requesting a normal application quit.
                require_clean(self.runtime(client, pid))
        if self.os_identity() != pid:
            raise RuntimeError("Editor identity changed before quit")
        # Known gap: a package could become dirty between the last check and the quit
        # request. On Linux there is no save dialog to catch that, so do not edit while closing.
        getattr(self, self.layout["quit"])(pid)
        deadline = time.monotonic() + 120
        while time.monotonic() < deadline:
            if not pid_exists(pid) and not any(p["pid"] == pid for p in self.processes()) and self.listener() != pid:
                print(f"Editor PID {pid} exited normally")
                return
            time.sleep(0.5)
        raise RuntimeError("Editor did not exit in time. Check it in Unreal; do not signal it again. No force quit attempted.")

    def quit_by_signal(self, pid):
        # Unreal's Unix handler turns the first SIGTERM into a normal engine exit request
        # (exit 143). A second signal forces exit, so send exactly one. Never SIGKILL.
        os.kill(pid, signal.SIGTERM)

    def quit_by_app(self, pid):
        # NSRunningApplication.terminate sends a normal quit request, honoring
        # Unreal's dirty/save dialogs. Never use forceTerminate, SIGTERM or QUIT_EDITOR.
        script = 'ObjC.import("AppKit"); $.NSRunningApplication.runningApplicationWithProcessIdentifier(' + str(pid) + ').terminate;'
        result = self.execute(["osascript", "-l", "JavaScript", "-e", script], capture_output=True, text=True)
        if result.stdout.strip() != "true":
            raise RuntimeError("macOS declined the normal quit request")

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
        return [self.cmd, self.w.project, f"-ReportExportPath={output}", f"-abslog={log}", "-unattended", "-nosound", "-nosplash",
                *self.layout["automation"](pattern), *self.native_limits()]

    def test(self, pattern, allow_warnings=False):
        self.require_engine()
        self.require_closed()
        if not re.fullmatch(r"[A-Za-z0-9_.]+", pattern):
            raise ValueError("Use one explicit automation prefix consisting of letters, digits, dots and underscores")
        output = self.w.local / "automation" / stamp()
        output.mkdir(parents=True)
        log = output / "automation.log"
        crashes_before = crash_folders(self.w.root)
        with self.config_guard():
            code = self.run_bounded(self.automation_command(pattern, output, log), output / "console.log", timeout=300)
        # Check the process lifecycle first, so a crash is reported even when tests also fail (#157).
        lifecycle = lifecycle_problems(code, crash_folders(self.w.root) - crashes_before, self.w.root)
        failed_lifecycle = f" Failed lifecycle: {'; '.join(lifecycle)}." if lifecycle else ""
        report_path = output / "index.json"
        if not report_path.is_file():
            raise RuntimeError(f"Automation produced no test report (possibly zero matched tests); inspect {log}.{failed_lifecycle}")
        try:
            report = automation_result(json.loads(report_path.read_text(encoding="utf-8-sig")))
        except RuntimeError as error:
            raise RuntimeError(f"{error}.{failed_lifecycle} Inspect {log}") from None
        events = log_events(log.read_text(errors="replace"))
        if events["errors"]:
            raise RuntimeError(f"Automation logged errors: {events['errors']}.{failed_lifecycle}")
        if lifecycle:
            raise RuntimeError(f"Tests passed but the run is a failed lifecycle: {'; '.join(lifecycle)} after the tests; inspect {log}")
        write_json(output / "verified.json", {**report, "filter": pattern, "project": str(self.w.project), "exit_code": code, "warnings": events["warnings"]})
        if events["warnings"] and not allow_warnings:
            raise RuntimeError(f"{len(events['warnings'])} warning line(s); the gate is zero. First: {events['warnings'][0]} Inspect {log}")
        print(f"Automation passed: {report}; warning lines: {len(events['warnings'])}; evidence: {output}")

    def run_game(self, args, log, timeout):
        """Run one owned game process. On timeout it gets one SIGTERM (a normal exit request); it is never killed."""
        with log.open("ab") as stream:
            process = subprocess.Popen([str(a) for a in args], cwd=self.w.root, stdout=stream, stderr=subprocess.STDOUT)
            try:
                return process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                process.terminate()
                try:
                    process.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    raise RuntimeError(f"Game PID {process.pid} timed out after {timeout}s and ignored one SIGTERM; left running. Inspect {log}") from None
                raise RuntimeError(f"Game timed out after {timeout}s and was asked to exit; inspect {log}") from None

    def gallery(self, fixtures=None, resolutions=None, primitives=False, poses=None, timeout=300):
        fixtures = list(dict.fromkeys(fixtures or GALLERY_FIXTURES))
        resolutions = list(dict.fromkeys(resolutions or GALLERY_RESOLUTIONS))
        sizes = {text: parse_resolution(text) for text in resolutions}
        for name in fixtures:
            if not FIXTURE_PATTERN.fullmatch(name):
                raise ValueError(f"Fixture {name!r} must be lowercase letters, digits and hyphens")
        poses_path = Path(poses or GALLERY_POSES)
        poses_path = (poses_path if poses_path.is_absolute() else self.w.root / poses_path).resolve()
        shots = load_poses(poses_path)
        if timeout < 30:
            raise ValueError("--timeout must be at least 30 seconds")
        fixture_root = self.w.root / "Saved" / "CaptureFixtures"
        for name in fixtures:
            if not (fixture_root / name / "current-v1.json").is_file():
                raise RuntimeError(f"Fixture {name} is missing from {fixture_root}; {FIXTURE_HINT}")
        self.require_engine()
        self.require_closed()
        receipt = self.w.local / "build.json"
        if not receipt.is_file() or json.loads(receipt.read_text()) != self.build_identity():
            raise RuntimeError("Build evidence is missing or stale; run build first")
        output = self.w.local / "gallery" / stamp()
        output.mkdir(parents=True)
        head = self.execute(["git", "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
        dirty = bool(self.execute(["git", "status", "--porcelain", "--untracked-files=no"], capture_output=True, text=True).stdout.strip())
        runs = []
        with self.config_guard():
            for name in fixtures:
                sidecar = fixture_root / name / "fixture.json"
                info = json.loads(sidecar.read_text()) if sidecar.is_file() else {}
                for text in resolutions:
                    width, height = sizes[text]
                    target = output / name / text
                    save = output / "save" / name / text
                    target.mkdir(parents=True)
                    save.mkdir(parents=True)
                    # The game only reads a copy; the real fixture and any real save stay untouched.
                    shutil.copy2(fixture_root / name / "current-v1.json", save / "current-v1.json")
                    log = target / "game.log"
                    command = [self.editor, self.w.project, "-game", "-windowed", "-ForceRes", f"-ResX={width}", f"-ResY={height}", *self.layout["headless"],
                               "-unattended", "-nosound", "-nosplash", "-UseFixedTimeStep", "-FPS=30", f"-abslog={log}",
                               f"-ExpeditionSaveRoot={save}", f"-ExpeditionCapture={poses_path}", f"-ExpeditionCaptureOut={target}",
                               f"-ExpeditionCaptureFixture={name}", *(["-ExpeditionPrimitives"] if primitives else []), *self.native_limits()]
                    print(f"Capturing {name} at {text}{' (primitives)' if primitives else ''}")
                    crashes_before = crash_folders(self.w.root)
                    code = self.run_game(command, target / "console.log", timeout)
                    lifecycle = lifecycle_problems(code, crash_folders(self.w.root) - crashes_before, self.w.root, who="Game")
                    if lifecycle:
                        raise RuntimeError(f"Failed lifecycle for {name} at {text}: {'; '.join(lifecycle)}; inspect {log}")
                    manifest = verify_capture(target / "manifest.json", shots, (width, height), target)
                    runs.append({"fixture": name, "seed": manifest.get("seed", info.get("seed")), "board": [manifest.get("boardWidth"), manifest.get("boardHeight")],
                                 "fixture_info": info, "resolution": text, "primitives": primitives, "themed": manifest.get("themed"),
                                 "directory": str(target.relative_to(output)), "shots": manifest["shots"]})
        write_json(output / "index.json", {"commit": head, "dirty": dirty, "engine": self.w.versions["unreal_engine"], "changelist": self.w.versions["unreal_changelist"],
                                          "poses": str(poses_path), "settle_args": ["-UseFixedTimeStep", "-FPS=30"], "runs": runs})
        pictures = sum(1 for run in runs for shot in run["shots"] if shot.get("status") == "captured")
        skipped = sum(1 for run in runs for shot in run["shots"] if shot.get("status") == "skipped")
        print(f"Gallery: {len(runs)} run(s), {pictures} picture(s), {skipped} skipped as not applicable; index: {output / 'index.json'}")

    def perf_binary(self, package=None):
        """(game binary, package folder) to measure: the given folder, else the newest Builds/<stamp> whose package run finished (verified.json)."""
        if package:
            folder = Path(package).expanduser()
            folder = (folder if folder.is_absolute() else self.w.root / folder).resolve()
            if not folder.is_dir():
                raise RuntimeError(f"--package {folder} is not a folder")
            return self.packaged_binary(folder), folder
        builds = self.w.root / "Builds"
        finished = sorted(p for p in builds.glob("*") if (p / "verified.json").is_file()) if builds.is_dir() else []
        if not finished:
            raise RuntimeError("No finished package under Builds/; run `package` first, or pass --package DIR (or --editor for a dry run)")
        return self.packaged_binary(finished[-1]), finished[-1]

    def perf_command(self, binary, editor, script, size, frames, log, save, resume, vsync, sound, offscreen):
        width, height = size
        command = [binary, *([self.w.project, "-game"] if editor else []), "-windowed", "-ForceRes", f"-ResX={width}", f"-ResY={height}",
                   "-unattended", "-nosplash", f"-abslog={log}", f"-csvCaptureFrames={frames}", "-csvGpuStats",
                   f"-ExpeditionSaveRoot={save}", f"-ExpeditionAutoplay={script}"]
        if resume:
            command.append("-ExpeditionAutoplayResume")
        if not vsync:
            command.append("-novsync")
        if not sound:
            command.append("-nosound")
        if offscreen:
            command += self.layout["headless"]
        return [*command, *self.native_limits()]

    def perf_run(self, args, log, timeout, cwd):
        """Run one game process, sampling its resident memory. On timeout it gets one SIGTERM (a normal exit request) and is never killed."""
        with log.open("ab") as stream:
            process = subprocess.Popen([str(a) for a in args], cwd=cwd, stdout=stream, stderr=subprocess.STDOUT)
            pid = perf_tools.resolve_game_pid(process.pid, Path(str(args[0])).name)
            with perf_tools.RssSampler(pid) as sampler:
                try:
                    code = process.wait(timeout=timeout)
                except subprocess.TimeoutExpired:
                    process.terminate()
                    try:
                        process.wait(timeout=30)
                    except subprocess.TimeoutExpired:
                        raise RuntimeError(f"Game PID {process.pid} timed out after {timeout}s and ignored one SIGTERM; left running. Inspect {log}") from None
                    raise RuntimeError(f"Game timed out after {timeout}s and was asked to exit; inspect {log}") from None
        return code, sampler.summary()

    def perf_crash_dirs(self, binary):
        """Folders whose Saved/Crashes a run may write to: this checkout, the packaged game's user directory and its staged folder."""
        roots = [self.w.root, Path.home() / ".config" / "Epic" / self.w.settings["game_target"]]
        if len(binary.parents) > 2 and binary.parent.name == "Linux":
            roots.append(binary.parents[2])
        return roots

    def perf_context(self, package, editor, script, resolution, frames, timeout, offscreen, vsync, sound):
        """Validate the arguments and resolve what a pass launches. Shared by every pass of a run."""
        size = parse_resolution(resolution)
        if editor and package:
            raise ValueError("--editor and --package are exclusive")
        if not PERF_FRAMES[0] <= frames <= PERF_FRAMES[1]:
            raise ValueError(f"--csv-frames must be {PERF_FRAMES[0]} to {PERF_FRAMES[1]}")
        if timeout < 60:
            raise ValueError("--timeout must be at least 60 seconds")
        script_path = Path(script or PERF_SCRIPT)
        script_path = (script_path if script_path.is_absolute() else self.w.root / script_path).resolve()
        if not script_path.is_file():
            raise RuntimeError(f"Autoplay script {script_path} does not exist")
        try:
            scripted = json.loads(script_path.read_text())
            if scripted.get("version") != 1 or len(scripted.get("party", [])) != 4:
                raise ValueError("needs \"version\": 1 and four party builds")
        except ValueError as exc:
            raise RuntimeError(f"Autoplay script {script_path} is invalid: {exc}") from None
        self.require_engine()
        self.require_closed()
        if editor:
            receipt = self.w.local / "build.json"
            if not receipt.is_file() or json.loads(receipt.read_text()) != self.build_identity():
                raise RuntimeError("Build evidence is missing or stale; run build first")
            binary, cwd, folder = self.editor, self.w.root, None
        else:
            binary, folder = self.perf_binary(package)
            cwd = binary.parent
        head = self.execute(["git", "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
        dirty = bool(self.execute(["git", "status", "--porcelain", "--untracked-files=no"], capture_output=True, text=True).stdout.strip())
        return {"size": size, "editor": editor, "script": script_path, "binary": binary, "cwd": cwd, "folder": folder, "frames": frames, "timeout": timeout,
                "offscreen": offscreen, "vsync": vsync, "sound": sound, "commit": head, "dirty": dirty,
                "directories": perf_tools.csv_directories(self.w.settings["game_target"], package=folder, project_root=self.w.root)}

    @staticmethod
    def perf_identity(record):
        """What every pass of one run must agree on: the build under test and how it was launched."""
        return {key: record.get(key) for key in ("commit", "dirty", "binary", "mode", "script", "cores", "flags")}

    def perf_mismatch(self, base, other, label):
        """Problems when `other` (a pass.json or the current context record) was made from a different build or with different flags."""
        left, right = self.perf_identity(base), self.perf_identity(other)
        return [f"{label}: {key} differs ({left[key]!r} in {base.get('name', 'the earlier pass')} vs {right[key]!r})" for key in left if left[key] != right[key]]

    def perf_run_folder(self, stamp_name, create):
        """The run folder under .local/perf/: new (create) or named by --run."""
        root = self.w.local / "perf"
        if create:
            folder = root / stamp()
            folder.mkdir(parents=True)
            print(f"PERF_RUN {folder.name}")
            return folder
        if not stamp_name or not re.fullmatch(r"[A-Za-z0-9]+", stamp_name):
            raise ValueError("--run takes the stamp a first pass printed (PERF_RUN <stamp>)")
        folder = root / stamp_name
        if not folder.is_dir():
            raise RuntimeError(f"No perf run {stamp_name} under {root}; start one with `perf --pass warmup` or `perf --pass measured`")
        return folder

    def perf_pass(self, ctx, output, name, pad):
        """Run one game pass in the run folder and write <pass>/pass.json. Returns that record."""
        resuming = name == "resume"
        folder = output / name
        record_path = folder / "pass.json"
        if folder.exists():
            raise RuntimeError(f"The {name} pass already ran in {output.name}; start a new run")
        mode = "editor (-game): a dry run, not the gate" if ctx["editor"] else "packaged Development"
        flags = {"vsync": ctx["vsync"], "sound": ctx["sound"], "offscreen": ctx["offscreen"], "csv_frames": ctx["frames"], "resolution": f"{ctx['size'][0]}x{ctx['size'][1]}"}
        current = {"name": name, "commit": ctx["commit"], "dirty": ctx["dirty"], "binary": str(ctx["binary"]), "mode": mode, "script": str(ctx["script"]),
                   "cores": self.cores, "flags": flags}
        save = folder / "save"
        if resuming:
            measured = output / "measured" / "pass.json"
            if not measured.is_file() or not (output / "measured" / "save").is_dir():
                raise RuntimeError(f"The measured pass has not run in {output.name}; run `perf --pass measured --run {output.name}` first")
            problems = self.perf_mismatch(json.loads(measured.read_text()), current, "resume")
            if problems:
                raise RuntimeError("The resume pass must use the build and flags of the measured pass: " + "; ".join(problems))
        folder.mkdir()
        if resuming:
            # The fresh process resumes the checkpoint the measured pass left; a copy keeps that evidence unchanged.
            shutil.copytree(output / "measured" / "save", save)
        else:
            save.mkdir()
        log = folder / "game.log"
        crash_roots = self.perf_crash_dirs(ctx["binary"])
        crashes_before = {str(r / "Saved" / "Crashes" / n) for r in crash_roots for n in crash_folders(r)}
        started = time.time()
        size = ctx["size"]
        print(f"Perf pass {name}: {'resuming the checkpoint' if resuming else 'playing the fight'} at {size[0]}x{size[1]}")
        with self.config_guard():
            code, memory = self.perf_run(self.perf_command(ctx["binary"], ctx["editor"], ctx["script"], size, ctx["frames"], log, save, resuming, ctx["vsync"], ctx["sound"], ctx["offscreen"]),
                                         folder / "console.log", ctx["timeout"], ctx["cwd"])
        ended = time.time()
        crashes = {str(r / "Saved" / "Crashes" / n) for r in crash_roots for n in crash_folders(r)} - crashes_before
        problems = lifecycle_problems(code, set(), self.w.root, who="Game")
        if crashes:
            problems.append("new crash report(s): " + ", ".join(sorted(crashes)))
        text = log.read_text(errors="replace") if log.is_file() else ""
        if "AUTOPLAY_FAIL" in text or not re.search(r"AUTOPLAY_DONE ok\b", text):
            problems.append("the game did not log AUTOPLAY_DONE ok" + ("; it logged AUTOPLAY_FAIL" if "AUTOPLAY_FAIL" in text else ""))
        if "LogExit: Exiting." not in text:
            problems.append("the game did not log a normal exit")
        if problems:
            raise RuntimeError(f"Failed lifecycle for the {name} pass: {'; '.join(problems)}; inspect {log}")
        record = {**current, "exit_code": code, "log": str(log.relative_to(output)), "csv": None, "rss": memory,
                  "started": started, "ended": ended,
                  "started_at": datetime.fromtimestamp(started, timezone.utc).isoformat(), "ended_at": datetime.fromtimestamp(ended, timezone.utc).isoformat()}
        if name != "warmup":
            found = perf_tools.find_csv(text, ctx["directories"], started - 2)
            if not found:
                raise RuntimeError(f"The {name} pass left no CSV capture; looked in {[str(d) for d in ctx['directories']]}; inspect {log}")
            kept = folder / f"{name}.csv"
            shutil.copy2(found, kept)
            record["csv"] = str(kept.relative_to(output))
            record["csv_source"] = str(found)
            # Fail now, while the machine is ours, if the capture cannot be sliced (no markers, run cut short).
            perf_tools.analyze_pass(perf_tools.parse_csv(kept.read_text(errors="replace")), pad)
            record["hardware"] = perf_tools.hardware_summary(run=lambda cmd: subprocess.run(cmd, capture_output=True, text=True, timeout=10, check=True).stdout)
        write_json(record_path, record)
        return record

    def perf_report(self, output, pad):
        """Build report.json and report.md from the passes recorded in a run folder; runs no Unreal."""
        records = {}
        for name in ("warmup", "measured", "resume"):
            path = output / name / "pass.json"
            if path.is_file():
                records[name] = json.loads(path.read_text())
        missing = [name for name in ("measured", "resume") if name not in records]
        if missing:
            raise RuntimeError(f"Run {output.name} has no {' or '.join(missing)} pass record; run the missing pass(es) with `perf --pass ... --run {output.name}`")
        problems = []
        for name in ("warmup", "resume"):
            if name in records:
                problems += self.perf_mismatch(records["measured"], records[name], name)
        if problems:
            raise RuntimeError("The passes of one run must share a commit, binary and flags: " + "; ".join(problems))
        captures, rss = {}, {}
        for name in ("measured", "resume"):
            kept = output / records[name]["csv"]
            captures[name] = perf_tools.analyze_pass(perf_tools.parse_csv(kept.read_text(errors="replace")), pad)
            rss[name] = records[name]["rss"]
        gaps, order = [], [n for n in ("warmup", "measured", "resume") if n in records]
        for before, after in zip(order, order[1:]):
            gaps.append({"from": before, "to": after, "seconds": round(records[after]["started"] - records[before]["ended"], 1)})
        measured = records["measured"]
        peaks = [m for m in rss.values() if m.get("max_mb") is not None]
        combined = {"samples": sum(m["samples"] for m in rss.values()), "interval_s": perf_tools.SAMPLE_SECONDS,
                    "max_sampled_mb": max((m["max_sampled_mb"] for m in peaks if m.get("max_sampled_mb") is not None), default=None),
                    "peak_hwm_mb": max((m["peak_hwm_mb"] for m in peaks if m.get("peak_hwm_mb") is not None), default=None),
                    "max_mb": max((m["max_mb"] for m in peaks), default=None), "per_pass": rss}
        flags = dict(measured["flags"], warmup="warmup" in records, transition_pad=pad)
        width, height = (int(v) for v in measured["flags"]["resolution"].split("x"))
        report = perf_tools.build_report(captures["measured"], captures["resume"], combined, (width, height), extra={
            "stamp": output.name, "commit": measured["commit"], "dirty": measured["dirty"], "engine": self.w.versions["unreal_engine"], "changelist": self.w.versions["unreal_changelist"],
            "mode": measured["mode"], "binary": measured["binary"], "script": measured["script"], "cores": measured["cores"], "flags": flags,
            "warmup": "warmup" in records, "pass_gaps": gaps, "passes": [records[n] for n in order],
            "top_gpu_stats": perf_tools.top_gpu_stats(captures["measured"]["steady"]),
            "hardware": measured.get("hardware") or perf_tools.hardware_summary()})
        if measured["mode"].startswith("editor"):
            report["passed"] = False
            report["failures"].append("Measured in the editor (-game): not a packaged Development build, so it cannot pass the gate")
        if "warmup" not in records:
            # #148 asks for a warm package: the first run after packaging pays shader, PSO and file caches.
            report["passed"] = False
            report["failures"].append("No warm-up pass: the gate is measured on a warm package (run perf --pass warmup first)")
        write_json(output / "report.json", report)
        (output / "report.md").write_text(perf_tools.markdown_report(report))
        for check in report["checks"]:
            print(f"  {check['label']}: {perf_tools.number(check['value'])} (limit {check['limit']}) {'ok' if check['passed'] else 'FAIL'}")
        for item in report["transitions"]:
            print(f"  transition {item['name']} [{item['pass']}]: worst frame {item['worst_frame_ms']:.1f} ms, {item['duration_ms']:.0f} ms over {item['frames']} frames")
        for gap in gaps:
            print(f"  gap {gap['from']} -> {gap['to']}: {gap['seconds']:.0f} s")
        print(f"Performance gate {'PASSED' if report['passed'] else 'FAILED'}; report: {output / 'report.md'}")
        for failure in report["failures"]:
            print(f"  - {failure}")
        return report

    def perf(self, package=None, editor=False, script=None, resolution="1920x1080", frames=200000, timeout=1800, warmup=True,
             offscreen=False, vsync=False, sound=True, pad=perf_tools.TRANSITION_PAD, only=None, run=None):
        """Measure an autoplay fight against the #148 budgets (docs/development.md).

        Without `only` it warms up, measures, resumes in a fresh process and reports in one go. With `only` (warmup, measured,
        resume or report) it does just that step in the run folder `run`, so each locked job can be short; `warmup` and
        `measured` start a new run when `run` is omitted. Returns the report, or the pass record for a single pass."""
        if only is not None and only not in PERF_PASSES:
            raise ValueError(f"--pass must be one of {', '.join(PERF_PASSES)}")
        if only is None and run:
            raise ValueError("--run applies to --pass")
        if only in ("resume", "report") and not run:
            raise ValueError(f"--pass {only} needs --run <stamp> (printed as PERF_RUN by the first pass)")
        if only and not warmup:
            raise ValueError("--no-warmup applies to a run without --pass")
        if only == "report":
            if package or editor or script:
                raise ValueError("--pass report reads a run folder; it takes no --package, --editor or --script")
            return self.perf_report(self.perf_run_folder(run, create=False), pad)
        ctx = self.perf_context(package, editor, script, resolution, frames, timeout, offscreen, vsync, sound)
        if only:
            output = self.perf_run_folder(run, create=run is None)
            if run is None and only == "resume":
                raise ValueError("--pass resume needs --run")
            return self.perf_pass(ctx, output, only, pad)
        output = self.perf_run_folder(None, create=True)
        for name in (["warmup"] if warmup else []) + ["measured", "resume"]:
            self.perf_pass(ctx, output, name, pad)
        return self.perf_report(output, pad)

    def packaged_binary(self, output):
        target = self.w.settings["game_target"]
        found = [p for p in output.glob(self.layout["packaged"].format(target=target)) if p.is_file() and os.access(p, os.X_OK)]
        if len(found) != 1:
            raise RuntimeError(f"Cannot identify the packaged {target} executable in {output}: {found}")
        return found[0]

    def package(self):
        self.require_engine()
        self.require_closed()
        output = self.w.root / "Builds" / stamp()
        output.mkdir(parents=True)
        log = output / "package.log"
        with self.config_guard(), log.open("w") as stream:
            self.execute([self.engine / "Build/BatchFiles/RunUAT.sh", "BuildCookRun", f"-project={self.w.project}", f"-platform={self.platform}", "-clientconfig=Development", f"-target={self.w.settings['game_target']}", "-build", "-cook", "-stage", "-pak", "-package", "-archive", f"-archivedirectory={output}", "-unattended", "-nop4", "-utf8output", *self.package_limits()], stdout=stream, stderr=subprocess.STDOUT)
        package_events = log_events(log.read_text(errors="replace"))
        binary = self.packaged_binary(output)
        launch_log = output / "launch.log"
        with launch_log.open("w") as stream:
            process = subprocess.Popen([str(binary), "-unattended", *self.layout["headless"], "-nosound", "-stdout", "-FullStdOutLogOutput", *self.native_limits()], cwd=binary.parent, stdout=stream, stderr=subprocess.STDOUT)
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
                original = sys.exc_info()[1]
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=30)
                    except subprocess.TimeoutExpired:
                        message = f"Packaged game PID {process.pid} did not exit after one SIGTERM; left running"
                        if original:
                            message += f". Earlier failure: {original}"
                        raise RuntimeError(message) from original
        text = launch_log.read_text(errors="replace")
        events = log_events(text)
        if events["errors"]:
            raise RuntimeError(f"Packaged startup logged errors: {events['errors']}")
        exited = "LogExit: Exiting." in text
        if not exited:
            raise RuntimeError(f"Packaged game did not log a normal exit; inspect {launch_log}")
        write_json(output / "verified.json", {"binary": str(binary), "startup_map": self.w.game_map, "launched_at": stamp(), "exit_code": process.returncode, "normal_exit_logged": exited, **events,
                                              "package_warnings": package_events["warnings"], "package_errors": package_events["errors"]})
        print(f"Packaged game entered {self.w.game_map} and exited normally (code {process.returncode}); warning lines: launch {len(events['warnings'])}, package {len(package_events['warnings'])}; package error lines: {len(package_events['errors'])}; evidence: {output}")

def crash_folders(root):
    """Names of Unreal crash reports under the project's Saved/Crashes (crashinfo-*)."""
    folder = Path(root) / "Saved" / "Crashes"
    return {p.name for p in folder.glob("crashinfo-*") if p.is_dir()} if folder.is_dir() else set()


def lifecycle_problems(code, new_crashes, root, who="Engine"):
    """Why a native run's process lifecycle failed (#157): abnormal exit, signal or a new crash report; empty when clean."""
    problems = []
    if code is not None and code < 0:
        problems.append(f"{who} was killed by signal {-code}")
    elif code:
        problems.append(f"{who} exited {code}")
    if new_crashes:
        problems.append("new crash report(s): " + ", ".join(str(Path(root) / "Saved" / "Crashes" / name) for name in sorted(new_crashes)))
    return problems


def pid_exists(pid):
    """True until the process is fully gone; an exiting editor can drop out of `ps` matching first."""
    try:
        os.kill(pid, 0)  # Signal 0 only checks existence; nothing is delivered.
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def build_warnings(text):
    """Compiler (clang/MSVC) and UnrealBuildTool warning lines."""
    return [line for line in text.splitlines() if re.search(r"(?i)(?:^|[\s:])warning(?: [A-Z]+\d+)?:", line)]


def core_count(value):
    try:
        cores = int(value)
    except ValueError:
        raise argparse.ArgumentTypeError("--cores takes a whole number") from None
    if cores < 1:
        raise argparse.ArgumentTypeError("--cores must be 1 or more")
    return cores


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", help="Select a root .uproject if the workspace has more than one")
    parser.add_argument("--cores", type=core_count, help="Cap build actions and engine cores so jobs share the machine (e.g. 2); applies to " + ", ".join(CORE_COMMANDS))
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
    gallery = commands.add_parser("gallery", help="Render the fixed-pose capture gallery under .local/gallery/")
    gallery.add_argument("--fixture", action="append", help="Fixture folder under Saved/CaptureFixtures (repeatable); default: " + ", ".join(GALLERY_FIXTURES))
    gallery.add_argument("--res", action="append", type=resolution_argument, help="Window size WIDTHxHEIGHT (repeatable); default: " + ", ".join(GALLERY_RESOLUTIONS))
    gallery.add_argument("--primitives", action="store_true", help="Render the engine-primitive presentation instead of the theme")
    gallery.add_argument("--poses", help="Poses file; default " + str(GALLERY_POSES))
    gallery.add_argument("--timeout", type=int, default=300, help="Seconds allowed for each game run")
    perf = commands.add_parser("perf", help="Measure one autoplay fight against the #148 performance budgets; evidence under .local/perf/")
    perf.add_argument("--package", help="Packaged build folder (default: the newest Builds/<stamp> that finished packaging)")
    perf.add_argument("--editor", action="store_true", help="Run the editor in -game instead of a package: a quick dry run that cannot pass the gate")
    perf.add_argument("--script", help="Autoplay script; default " + str(PERF_SCRIPT))
    perf.add_argument("--res", type=resolution_argument, default="1920x1080", help="Window size WIDTHxHEIGHT; the budgets apply at 1920x1080")
    perf.add_argument("--csv-frames", type=int, default=200000, help="Upper bound for -csvCaptureFrames (the game ends the capture itself when the run is done)")
    perf.add_argument("--timeout", type=int, default=1800, help="Seconds allowed for each pass")
    perf.add_argument("--no-warmup", action="store_true", help="Skip the discarded warm-up pass (a dry run; the gate wants a warm one)")
    perf.add_argument("--offscreen", action="store_true", help="Add -RenderOffscreen (no window; frames are not presented)")
    perf.add_argument("--vsync", action="store_true", help="Leave vertical sync on (default: -novsync, so FrameTime is not pinned to the refresh rate)")
    perf.add_argument("--no-sound", action="store_true", help="Add -nosound (default: sound on, so the synthesized cues are part of the game-thread cost)")
    perf.add_argument("--pass", dest="only", choices=PERF_PASSES, help="Run one step of a run (each in its own locked job): warmup, measured, resume or report; default: all of them in one go")
    perf.add_argument("--run", help="The run folder stamp under .local/perf/ (printed as PERF_RUN <stamp> by the first pass); required for resume and report")
    perf.add_argument("--transition-pad", type=int, default=perf_tools.TRANSITION_PAD, help="Frames after a transition ends that still count as the transition")
    scaffold = commands.add_parser("new-class")
    scaffold.add_argument("kind", choices=BASES)
    scaffold.add_argument("name", help="PascalCase class name without Unreal's A/U prefix")
    raw = commands.add_parser("mcp")
    raw.add_argument("--method", default="tools/list")
    raw.add_argument("--params", default="{}")
    args = parser.parse_args()
    if args.cores and args.command not in CORE_COMMANDS:
        parser.error(f"--cores does not apply to {args.command}; it applies to " + ", ".join(CORE_COMMANDS))
    dev = Dev(Workspace.discover(ROOT, args.project), cores=args.cores)
    if args.command == "doctor":  # Read-only; runs without the Unix lock so any host gets guidance.
        dev.doctor()
        return 0
    with dev.operation():
        if args.command == "editor":
            dev.launch(wait=not args.no_wait)
        elif args.command == "close":
            dev.close(args.request_quit)
        elif args.command == "schemas":
            dev.schemas(args.toolset)
        elif args.command == "test":
            dev.test(args.filter or dev.w.settings["automation_filter"], args.allow_warnings)
        elif args.command == "gallery":
            dev.gallery(args.fixture, args.res, args.primitives, args.poses, args.timeout)
        elif args.command == "perf":
            report = dev.perf(args.package, args.editor, args.script, args.res, args.csv_frames, args.timeout, not args.no_warmup,
                              args.offscreen, args.vsync, not args.no_sound, args.transition_pad, args.only, args.run)
            return 0 if args.only in PERF_PASSES[:3] or report["passed"] else 1
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
