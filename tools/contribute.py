#!/usr/bin/env python3
"""Read-only contribution checks. Python 3.11+, Git, and local Git LFS."""
import sys
if sys.version_info < (3, 11):
    sys.exit("Python 3.11+ required.")

import argparse
import configparser
import json
import os
from pathlib import Path
import re
import subprocess
import tomllib
from urllib.parse import unquote, urlparse

ROOT = Path(__file__).resolve().parents[1]
try:
    REPOSITORY = json.loads((ROOT / "tools/project.json").read_text())["repository"]
except (OSError, ValueError, KeyError):
    sys.exit("tools/project.json needs a repository value such as owner/name.")
POINTER = re.compile(rb"version https://git-lfs.github.com/spec/v1\noid sha256:[0-9a-f]{64}\nsize (?:0|[1-9][0-9]*)\n")
GENERATED = {"Binaries", "Intermediate", "Saved", "DerivedDataCache", "Builds", ".local", ".venv", "__pycache__"}


def git(root, *args, data=None, required=True):
    result = subprocess.run(["git", "--no-optional-locks", "-C", str(root), *args],
                            input=data, capture_output=True)
    if required and result.returncode:
        # Git diagnostics can contain remote credentials; report the operation only.
        raise RuntimeError(f"Git {args[0]} failed; inspect this checkout's Git state.")
    return result


def text(root, *args, required=True):
    return git(root, *args, required=required).stdout.decode().strip()


def require_root(root):
    if Path(text(root, "rev-parse", "--show-toplevel")).resolve() != Path(root).resolve():
        raise RuntimeError("Run checks from this repository, not a directory inside another repository.")


def tracked(root):
    return [p.decode() for p in git(root, "ls-files", "-z").stdout.split(b"\0") if p]


def stored(root, name):
    return git(root, "show", ":" + name).stdout


def lfs_paths(root, files):
    if not files:
        return []
    result = git(root, "check-attr", "--cached", "-z", "--stdin", "filter",
                 data=b"\0".join(p.encode() for p in files) + b"\0").stdout.split(b"\0")
    return [result[i].decode() for i in range(0, len(result) - 2, 3) if result[i + 2] == b"lfs"]


def endpoint_problems(root):
    versions = json.loads(stored(root, "tools/versions.json"))
    url = versions["mcp_url"]
    endpoint = urlparse(url)
    codex = tomllib.loads(stored(root, ".codex/config.toml").decode())
    claude = json.loads(stored(root, ".mcp.json"))
    ini = configparser.ConfigParser(interpolation=None, strict=False)
    ini.read_string(stored(root, "Config/DefaultEditorPerProjectUserSettings.ini").decode())
    editor = ini["/Script/ModelContextProtocolEngine.ModelContextProtocolSettings"]
    if (endpoint.scheme != "http" or endpoint.hostname != "127.0.0.1" or not endpoint.port
            or endpoint.path != "/mcp" or endpoint.username or endpoint.query or endpoint.fragment
            or codex["mcp_servers"]["unreal"]["url"] != url
            or claude["mcpServers"]["unreal"]["url"] != url
            or editor.getint("ServerPortNumber") != endpoint.port
            or editor["ServerUrlPath"] != endpoint.path
            or editor.getboolean("bAutoStartServer")):
        return ["MCP endpoint configurations disagree or enable unsafe headless auto-start."]
    return []


def check_repository(root):
    """Check the Git index, including staged changes; no LFS download is needed."""
    require_root(root)
    if text(root, "ls-files", "--unmerged"):
        return ["Resolve the unmerged index before checking repository hygiene."]
    files = tracked(root)
    problems = []
    for name in files:
        path = Path(name)
        if GENERATED.intersection(path.parts) or name.startswith(("Build/Mac/", "Build/Linux/")) or path.suffix == ".pyc":
            problems.append(f"Generated output is tracked: {name}")
        if name.startswith("Config/") and re.search(rb"(?im)^\s*SecurityToken\s*=\s*[^\s\r\n]+", stored(root, name)):
            problems.append(f"Machine-generated SecurityToken is tracked: {name} (value omitted)")
    for name in lfs_paths(root, files):
        size = int(text(root, "cat-file", "-s", ":" + name))
        if size > 1024 or not POINTER.fullmatch(stored(root, name)):
            problems.append(f"Expected a Git LFS pointer in the index: {name}")
    try:
        problems.extend(endpoint_problems(root))
    except (KeyError, ValueError, configparser.Error, RuntimeError):
        problems.append("Cannot validate MCP endpoint configurations in the index.")
    file_set = set(files)
    directories = {str(parent) for p in files for parent in Path(p).parents}
    for name in files:
        if not name.endswith(".md"):
            continue
        content = stored(root, name).decode("utf-8")
        for link in re.findall(r"\[[^\]\n]*\]\(([^\s)]+)\)", content):
            parsed = urlparse(link.strip("<>"))
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            target = Path(os.path.normpath(str(Path(name).parent / unquote(parsed.path))))
            if str(target) not in file_set and str(target) not in directories:
                problems.append(f"Broken local link in {name}: {link}")
    return problems


def correct_remote(url):
    if url.startswith("git@github.com:"):
        path = url.split(":", 1)[1]
    else:
        parsed = urlparse(url)
        if parsed.scheme not in ("https", "ssh") or parsed.hostname != "github.com":
            return False
        path = parsed.path.lstrip("/")
    return path.removesuffix(".git").rstrip("/").lower() == REPOSITORY.lower()


def editor_problems(root):
    # Import the Unreal inspector only here; `check` stays host-neutral.
    from dev import Dev, current_host
    if current_host() is None:
        return ["Editor state is unverified: local Unreal inspection supports Linux x86_64 (validated) and Apple Silicon macOS."]
    from workspace import Workspace
    inspector = Dev(Workspace.discover(root))
    inspector.require_closed()
    if inspector.listener():
        return ["Configured MCP port is occupied; coordinate its owner before changing this workspace."]
    return []


def preflight(root, inspect_editor=editor_problems):
    """Report local readiness using cached remote refs; never fetch or update files."""
    require_root(root)
    report = {"branch": text(root, "symbolic-ref", "--short", "-q", "HEAD", required=False),
              "base": "origin/staging", "ahead": None, "behind": None, "problems": []}
    problems = report["problems"]
    if not report["branch"]:
        problems.append("Detached HEAD: select an intentional branch before continuing.")
    for direction, args in (("fetch", []), ("push", ["--push"])):
        urls = text(root, "remote", "get-url", *args, "--all", "origin", required=False).splitlines()
        if not urls or not all(correct_remote(url) for url in urls):
            problems.append(f"origin {direction} must target {REPOSITORY} (tools/project.json); review remotes (URLs omitted).")
    # LFS clean filters can install hooks during status. Suppress installation
    # for this subprocess only; inspect the real configured hook below.
    dirty = text(root, "-c", f"core.hooksPath={os.devnull}", "status", "--porcelain=v1", "--untracked-files=all")
    if dirty:
        problems.append("Uncommitted work; preserve and inspect it before workspace updates:\n" + dirty)
    for marker in ("MERGE_HEAD", "CHERRY_PICK_HEAD", "REVERT_HEAD", "rebase-merge", "rebase-apply", "sequencer"):
        location = Path(text(root, "rev-parse", "--git-path", marker))
        if not location.is_absolute():
            location = Path(root) / location
        if location.exists():
            problems.append(f"Unfinished Git operation: {marker}; resolve it deliberately.")
    comparison = git(root, "rev-list", "--left-right", "--count", "origin/staging...HEAD", required=False)
    if comparison.returncode:
        problems.append("Missing origin/staging reference; inspect remotes and fetch explicitly.")
    else:
        report["behind"], report["ahead"] = map(int, comparison.stdout.split())
    if git(root, "lfs", "version", required=False).returncode:
        problems.append("Git LFS is unavailable; install it before working with game assets.")
    if text(root, "config", "--bool", "--get", "filter.lfs.required", required=False) != "true":
        problems.append("Git LFS filters are not required locally; inspect `git lfs install --local` setup.")
    hook = Path(text(root, "rev-parse", "--git-path", "hooks/pre-push"))
    if not hook.is_absolute():
        hook = Path(root) / hook
    if not hook.is_file() or not os.access(hook, os.X_OK) or not re.search(rb"git(?:-|\s+)lfs\s+pre-push", hook.read_bytes()):
        problems.append("Cannot verify the executable LFS pre-push hook; inspect existing hooks before installation.")
    for name in lfs_paths(root, tracked(root)):
        path = Path(root) / name
        if not path.is_file():
            problems.append(f"Missing LFS working asset: {name}; download assets with the editor closed.")
        elif path.stat().st_size <= 1024 and POINTER.fullmatch(path.read_bytes()):
            problems.append(f"LFS working asset needs download: {name}; use `git lfs pull` with the editor closed.")
    try:
        problems.extend(inspect_editor(root))
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError, KeyError) as exc:
        problems.append(f"Editor inspection failed: {exc}")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("check", "preflight"))
    args = parser.parse_args()
    try:
        if args.command == "check":
            problems = check_repository(ROOT)
            print("Checking the Git index (stage intended changes to include them).")
        else:
            report = preflight(ROOT)
            problems = report.pop("problems")
            print(json.dumps(report, indent=2))
            print("Remote comparison uses the last fetch; this command does not fetch or authorize Git mutations.")
            if report["branch"] in ("main", "staging"):
                print("Integration branch: create a feature branch before implementation.")
        for problem in problems:
            print("FAIL " + problem)
        if not problems:
            print("PASS " + args.command)
        return bool(problems)
    except (OSError, ValueError, RuntimeError, configparser.Error) as exc:
        print("FAIL " + str(exc), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
