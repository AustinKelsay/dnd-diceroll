"""Project selection and evidence parsers, independent of Unreal and MCP."""
from dataclasses import dataclass
import configparser
import hashlib
import json
from pathlib import Path
import re


def read_ini(path):
    result = configparser.ConfigParser(interpolation=None, strict=False)
    result.optionxform = str
    result.read(path)
    return result


@dataclass
class Workspace:
    root: Path
    project: Path
    settings: dict
    versions: dict
    startup_map: str

    @classmethod
    def discover(cls, root, selection=None):
        root = Path(root).resolve()
        projects = sorted(root.glob("*.uproject"))
        if selection:
            project = (root / selection).resolve()
            if project not in projects:
                raise ValueError("--project must select a root .uproject file")
        elif len(projects) == 1:
            project = projects[0]
        else:
            raise ValueError(f"Expected one root .uproject, found {len(projects)}; use --project")
        settings = json.loads((root / "tools/project.json").read_text())
        descriptor = json.loads(project.read_text())
        modules = {m["Name"] for m in descriptor.get("Modules", [])}
        if settings["module"] not in modules:
            raise ValueError("Configured module is absent from the .uproject")
        for key in ("editor_target", "game_target"):
            if not (root / "Source" / f"{settings[key]}.Target.cs").is_file():
                raise ValueError(f"Missing configured {key} Target.cs")
        startup = read_ini(root / "Config/DefaultEngine.ini")["/Script/EngineSettings.GameMapsSettings"]["EditorStartupMap"]
        if not startup.startswith(settings["content_root"] + "/"):
            raise ValueError("Startup map is outside the configured content root")
        return cls(root, project, settings, json.loads((root / "tools/versions.json").read_text()), startup)

    @property
    def local(self):
        return self.root / ".local"

    @property
    def log(self):
        return self.root / "Saved/Logs" / f"{self.project.stem}.log"

    @property
    def game_map(self):
        return read_ini(self.root / "Config/DefaultEngine.ini")["/Script/EngineSettings.GameMapsSettings"]["GameDefaultMap"]


def project_argument(command, project, windows=False):
    """ps joins argv without quoting; require a complete path at argument boundaries."""
    path = str(Path(project).resolve())
    if windows:
        command = command.replace("\\", "/")
        path = path.replace("\\", "/")
        # A quoted flag value (e.g. -abslog="project") is not a project argument.
        return bool(re.search(r'(?:^|\s)["\']?' + re.escape(path) + r'["\']?(?=$|\s)', command, flags=re.IGNORECASE))
    path = re.escape(path)
    return bool(re.search(r'(?:^|\s|[\"\'])' + path + r'(?=$|\s|[\"\'])', command, flags=re.IGNORECASE if windows else 0))


def source_digest(workspace):
    digest = hashlib.sha256()
    paths = [workspace.project, *(workspace.root / "Source").rglob("*")]
    for path in sorted(p for p in paths if p.is_file()):
        digest.update(str(path.relative_to(workspace.root)).encode() + b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def validate_runtime(status, workspace, pid):
    if Path(status["project_file"]).resolve() != workspace.project:
        raise RuntimeError("MCP editor belongs to a different project or checkout")
    if status["pid"] != pid:
        raise RuntimeError("MCP PID does not own the verified listener")
    expected = f"{workspace.versions['unreal_engine']}-{workspace.versions['unreal_changelist']}"
    if not status["engine_version"].startswith(expected + "+"):
        raise RuntimeError(f"Unexpected running engine: {status['engine_version']}")


def require_clean(status):
    if status["pie_active"]:
        raise RuntimeError("PIE is active; stop it before closing the editor")
    if status["dirty_packages"]:
        raise RuntimeError("Unsaved packages; save or discard intentionally in Unreal: " + ", ".join(status["dirty_packages"]))


def log_events(text):
    """Match Unreal severity fields, not words inside messages or success summaries."""
    events = {"errors": [], "warnings": []}
    for line in text.splitlines():
        match = re.search(r'\b(?:Log\w+|LoadErrors): (Error|Fatal|Warning):', line)
        if match:
            events["warnings" if match[1] == "Warning" else "errors"].append(line)
        elif "Ensure condition failed:" in line or "Blueprint Runtime Error:" in line:
            events["errors"].append(line)
    return events


def validate_bootstrap(text):
    summaries = re.findall(r'Success - (\d+) error\(s\), (\d+) warning\(s\)', text)
    if not summaries or summaries[-1] != ("0", "0"):
        raise RuntimeError("Bootstrap lacks a clean commandlet summary; inspect its fresh log")
    if "STARTER_BOOTSTRAP_VALIDATED" not in text:
        raise RuntimeError("Bootstrap did not validate the sandbox recipe")


def automation_result(report):
    tests = report.get("tests", [])
    if not tests:
        raise RuntimeError("Automation ran zero tests")
    failed = [t.get("fullTestPath", t.get("testDisplayName", "unknown")) for t in tests if t.get("state") not in ("Success", "SuccessWithWarnings")]
    if failed or report.get("failed", 0) or report.get("notRun", 0) or report.get("inProcess", 0):
        raise RuntimeError(f"Automation did not pass: {failed or 'incomplete/failed report'}")
    return {"tests": len(tests), "passed": len(tests), "warnings": report.get("succeededWithWarnings", 0)}


def entered_world(text, map_path):
    package = map_path.split(".", 1)[0]
    return bool(re.search(r'Bringing World ' + re.escape(package) + r'(?:\.\S+)? up for play', text))
