"""Performance-gate helpers for `tools/dev.py perf` (#148): CSV profiler analysis, RSS sampling and reports.

Pure Python 3.11, no Unreal and no third-party packages, so every function is testable offline. The game's
autoplay mode (-ExpeditionAutoplay) records AUTOPLAY_* markers into the CSV profiler's EVENTS column; this module
slices a capture by them, takes the transition windows out of the steady state and checks the budgets."""
import csv
import io
import math
import os
from pathlib import Path
import re
import sys
import threading
import time

# The gate of #148, at 1920x1080 on a warm Linux Development package. The p95 limits apply to the steady state
# (every frame of the measured pass outside a transition window). 5 GB is 5120 MiB, checked against the larger of the
# sampled VmRSS maximum and the kernel's VmHWM high-water mark, so a spike between two samples still counts.
BUDGETS = {"frame_p95_ms": 16.7, "gpu_p95_ms": 12.0, "game_p95_ms": 4.0, "rss_mb": 5120.0}
BUDGET_LABELS = {"frame_p95_ms": "p95 FrameTime", "gpu_p95_ms": "p95 GPUTime", "game_p95_ms": "p95 GameThreadTime", "rss_mb": "max RSS"}
GATE_RESOLUTION = (1920, 1080)
# The four transitions the gate must report separately, in play order. `resume` comes from the second (fresh-process) pass.
REQUIRED_TRANSITIONS = ("start->route", "route->battle", "battle->reward", "resume")
MARKER = "AUTOPLAY_"
SAMPLE_SECONDS = 0.5
TRANSITION_PAD = 3
GPU_STAT_PREFIXES = ("GPU/", "Exclusive/GPU/")
ONE_MIB = 1024.0


class Frame:
    __slots__ = ("index", "frame", "game", "gpu", "events", "stats")

    def __init__(self, index, frame, game, gpu, events, stats=None):
        self.index, self.frame, self.game, self.gpu, self.events, self.stats = index, frame, game, gpu, events, stats or {}


def percentile(values, q):
    """The q-th percentile (0..100) with linear interpolation between closest ranks (numpy's default)."""
    if not values:
        raise ValueError("percentile of no values")
    if not 0 <= q <= 100:
        raise ValueError("percentile must be 0..100")
    ordered = sorted(values)
    position = (len(ordered) - 1) * q / 100.0
    low = math.floor(position)
    high = math.ceil(position)
    return ordered[low] + (ordered[high] - ordered[low]) * (position - low)


# ---------------------------------------------------------------------------------------------------------------
# CSV capture.

def parse_csv(text):
    """Frames of a CSV profiler capture. Needs FrameTime, GameThreadTime and GPUTime columns.

    The first line is the header (EVENTS, then stat names). Metadata rows ([Name],value) and a repeated header row at
    the end (HasHeaderRowAtEnd) are skipped. GPU pass columns (GPU/..., Exclusive/GPU/...) ride along in Frame.stats."""
    # The profiler writes its metadata (the command line, device names) as very long fields on a trailing row.
    try:
        csv.field_size_limit(sys.maxsize)
    except OverflowError:
        # Python 3.11 uses C long here: Windows long is 32-bit even with 64-bit Python.
        csv.field_size_limit((1 << 31) - 1)
    rows = csv.reader(io.StringIO(text))
    header = next(rows, None)
    if not header or header[0].strip() != "EVENTS":
        raise ValueError("Not a CSV profiler capture: the first column must be EVENTS")
    columns = {name.strip(): position for position, name in enumerate(header)}
    missing = [name for name in ("FrameTime", "GameThreadTime", "GPUTime") if name not in columns]
    if missing:
        raise ValueError("The capture has no " + ", ".join(missing) + " column")
    extras = {name: position for name, position in columns.items() if name.startswith(GPU_STAT_PREFIXES)}
    frames = []
    for row in rows:
        if not row or row[0].startswith("[") or row[0].strip() == "EVENTS":
            continue
        try:
            frame = float(row[columns["FrameTime"]])
            game = float(row[columns["GameThreadTime"]])
            gpu = float(row[columns["GPUTime"]])
        except (ValueError, IndexError):
            continue
        stats = {}
        for name, position in extras.items():
            try:
                stats[name] = float(row[position])
            except (ValueError, IndexError):
                pass
        events = [item.strip() for item in row[0].split(";") if item.strip()]
        frames.append(Frame(len(frames), frame, game, gpu, events, stats))
    if not frames:
        raise ValueError("The capture holds no frames")
    return frames


def parse_markers(frames):
    """Autoplay markers in frame order: (frame index, kind, name, edge). Kinds: BEGIN, BEAT, TRANSITION, DONE."""
    markers = []
    for frame in frames:
        for event in frame.events:
            if not event.startswith(MARKER):
                continue
            # The profiler appends "##<seconds>" to each event it records; the marker is what comes before it.
            event = event.split("##", 1)[0]
            kind, _, rest = event[len(MARKER):].partition(":")
            if kind == "TRANSITION":
                name, _, edge = rest.rpartition(":")
                markers.append((frame.index, kind, name, edge))
            elif kind == "BEGIN":
                name, _, edge = rest.rpartition(":")
                markers.append((frame.index, kind, name, edge))
            else:
                markers.append((frame.index, kind, rest, ""))
    return markers


def transition_windows(markers):
    """(name, first frame, last frame) for each begin/end pair, in order; raises if one is left open or nested."""
    open_at = {}
    windows = []
    for index, kind, name, edge in markers:
        if kind != "TRANSITION":
            continue
        if edge == "begin":
            if name in open_at:
                raise ValueError(f"Transition {name} began twice without ending")
            open_at[name] = index
        elif edge == "end":
            if name not in open_at:
                raise ValueError(f"Transition {name} ended without beginning")
            windows.append((name, open_at.pop(name), index))
    if open_at:
        raise ValueError("Transition(s) never ended: " + ", ".join(sorted(open_at)))
    return windows


def summarize(frames):
    """Count, p50, p95 and max of FrameTime, GPUTime and GameThreadTime (milliseconds) over some frames; None if empty."""
    if not frames:
        return None
    result = {"frames": len(frames), "duration_ms": round(sum(f.frame for f in frames), 3)}
    for label, attribute in (("frame", "frame"), ("gpu", "gpu"), ("game", "game")):
        values = [getattr(f, attribute) for f in frames]
        result[f"{label}_p50_ms"] = round(percentile(values, 50), 3)
        result[f"{label}_p95_ms"] = round(percentile(values, 95), 3)
        result[f"{label}_max_ms"] = round(max(values), 3)
    return result


def top_gpu_stats(frames, limit=8):
    """The most expensive GPU pass columns (-csvGpuStats) by p95 over the frames; empty when the capture has none."""
    names = sorted({name for f in frames for name in f.stats})
    ranked = []
    for name in names:
        values = [f.stats[name] for f in frames if name in f.stats]
        if values:
            ranked.append((percentile(values, 95), name, sum(values) / len(values)))
    ranked.sort(reverse=True)
    return [{"stat": name, "p95_ms": round(p95, 3), "mean_ms": round(mean, 3)} for p95, name, mean in ranked[:limit]]


def analyze_pass(frames, pad=TRANSITION_PAD):
    """Slice one pass by its autoplay markers.

    Returns the run window (BEGIN to DONE), every transition with its worst frame and duration, and the frames of the
    run window outside every transition window (each widened by `pad` frames after its end). Raises ValueError when
    the pass has no BEGIN, never reached DONE, or finished with DONE:failed."""
    markers = parse_markers(frames)
    begins = [m for m in markers if m[1] == "BEGIN"]
    dones = [m for m in markers if m[1] == "DONE"]
    if not begins:
        raise ValueError("No AUTOPLAY_BEGIN marker: the game did not run autoplay (or the capture started late)")
    if not dones:
        raise ValueError("No AUTOPLAY_DONE marker: the run did not finish inside -csvCaptureFrames (raise --csv-frames) or the game failed")
    if dones[-1][2] != "ok":
        raise ValueError("The game reported AUTOPLAY_DONE failed; read its log")
    first, last = begins[0][0], dones[-1][0]
    windows = transition_windows(markers)
    excluded = set()
    transitions = []
    for name, start, end in windows:
        span = frames[start:end + 1]
        worst = max(span, key=lambda f: f.frame)
        transitions.append({
            "name": name, "first_frame": start, "last_frame": end, "frames": len(span),
            "duration_ms": round(sum(f.frame for f in span), 3),
            "worst_frame_ms": round(worst.frame, 3), "worst_frame_index": worst.index,
            "worst_gpu_ms": round(max(f.gpu for f in span), 3), "worst_game_ms": round(max(f.game for f in span), 3),
        })
        excluded.update(range(start, min(end + pad, len(frames) - 1) + 1))
    run = frames[first:last + 1]
    steady = [f for f in run if f.index not in excluded]
    battle_begin = next((end for name, _, end in windows if name == "route->battle"), None)
    battle_end = next((start for name, start, _ in windows if name == "battle->reward"), None)
    fight = [f for f in steady if battle_begin is not None and battle_end is not None and battle_begin < f.index < battle_end]
    beats = [{"frame": index, "name": name} for index, kind, name, _ in markers if kind == "BEAT"]
    return {"first_frame": first, "last_frame": last, "run_frames": len(run), "transitions": transitions, "steady": steady, "fight": fight, "beats": beats}


# ---------------------------------------------------------------------------------------------------------------
# Resident memory.

def parse_status_kb(text, key):
    """A kB field of /proc/<pid>/status (VmRSS, VmHWM) as an int, or None."""
    match = re.search(rf"^{re.escape(key)}:\s+(\d+)\s*kB", text, re.MULTILINE)
    return int(match.group(1)) if match else None


def resolve_game_pid(pid, binary_name, proc_root="/proc"):
    """The process to sample: `pid` itself when its command line names the game binary, else the first descendant that does
    (a launcher script would start the real game as a child). Falls back to `pid`."""
    root = Path(proc_root)

    def names_game(candidate):
        try:
            return binary_name in (root / str(candidate) / "cmdline").read_text(errors="replace").split("\0")[0]
        except OSError:
            return False

    queue = [pid]
    seen = set()
    while queue:
        current = queue.pop(0)
        if current in seen:
            continue
        seen.add(current)
        if names_game(current):
            return current
        try:
            for task in (root / str(current) / "task").iterdir():
                children = (task / "children").read_text().split()
                queue.extend(int(child) for child in children)
        except (OSError, ValueError):
            pass
    return pid


class RssSampler:
    """Samples VmRSS from /proc/<pid>/status every `interval` seconds on a thread until stopped or the process is gone.

    `peak_kb` is the kernel's VmHWM high-water mark as of the last successful read; `max_kb` is the largest sampled VmRSS."""

    def __init__(self, pid, interval=SAMPLE_SECONDS, proc_root="/proc", clock=time.monotonic):
        self.pid, self.interval, self.root, self.clock = pid, interval, Path(proc_root), clock
        self.samples = []
        self.peak_kb = None
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._run, name="rss-sampler", daemon=True)
        self._start = None

    def sample(self):
        """One read; False when the process is gone."""
        try:
            text = (self.root / str(self.pid) / "status").read_text()
        except OSError:
            return False
        rss = parse_status_kb(text, "VmRSS")
        if rss is None:
            return False
        if self._start is None:
            self._start = self.clock()
        self.samples.append((round(self.clock() - self._start, 3), rss))
        peak = parse_status_kb(text, "VmHWM")
        if peak is not None:
            self.peak_kb = peak
        return True

    def _run(self):
        while not self._stop.is_set():
            if not self.sample():
                # The process may not have a status file for a moment at start; stop only after it has appeared and gone.
                if self.samples:
                    return
            self._stop.wait(self.interval)

    def __enter__(self):
        self._thread.start()
        return self

    def __exit__(self, *exc):
        self.stop()

    def stop(self):
        self._stop.set()
        if self._thread.is_alive():
            self._thread.join(timeout=5)

    @property
    def max_kb(self):
        return max((kb for _, kb in self.samples), default=None)

    def summary(self):
        peak = max([v for v in (self.max_kb, self.peak_kb) if v is not None], default=None)
        return {
            "samples": len(self.samples), "interval_s": self.interval,
            "max_sampled_mb": None if self.max_kb is None else round(self.max_kb / ONE_MIB, 1),
            "peak_hwm_mb": None if self.peak_kb is None else round(self.peak_kb / ONE_MIB, 1),
            "max_mb": None if peak is None else round(peak / ONE_MIB, 1),
        }


# ---------------------------------------------------------------------------------------------------------------
# Locating the capture.

def csv_from_log(text):
    """The path the game logged with `AUTOPLAY_CSV path=...` (the last one), or None."""
    found = re.findall(r"AUTOPLAY_CSV path=(.+?)\s*$", text, re.MULTILINE)
    return Path(found[-1]) if found else None


def find_csv(log_text, directories, since):
    """The capture a pass wrote: the path its log names if that file exists, else the newest *.csv under the given
    directories modified at or after `since` (epoch seconds). Returns None when there is none."""
    named = csv_from_log(log_text)
    if named and named.is_file():
        return named
    newest = None
    for directory in directories:
        directory = Path(directory)
        if not directory.is_dir():
            continue
        for path in directory.glob("*.csv"):
            try:
                modified = path.stat().st_mtime
            except OSError:
                continue
            if modified >= since and (newest is None or modified > newest[0]):
                newest = (modified, path)
    return newest[1] if newest else None


def csv_directories(project_name, package=None, project_root=None, home=None, engine_version="5.8"):
    """Where a Development game's CSV profiler writes (Saved/Profiling/CSV): beside a staged package, under the user
    config directory of the project (a packaged Linux game keeps its Saved folder there), the engine's shared one, or
    the project's own Saved folder when running the editor."""
    home = Path(home or os.path.expanduser("~"))
    found = []
    if package:
        found.extend(sorted(Path(package).glob("**/Saved/Profiling/CSV")))
    found.append(home / ".config" / "Epic" / project_name / "Saved" / "Profiling" / "CSV")
    found.append(home / ".config" / "Epic" / "UnrealEngine" / engine_version / "Saved" / "Profiling" / "CSV")
    if project_root:
        found.append(Path(project_root) / "Saved" / "Profiling" / "CSV")
    return found


# ---------------------------------------------------------------------------------------------------------------
# Verdict and report.

def check_budgets(steady, rss, resolution, budgets=BUDGETS):
    """Each budget as {name, label, value, limit, passed}; a budget with no measurement fails (the gate has no evidence)."""
    values = {
        "frame_p95_ms": None if steady is None else steady["frame_p95_ms"],
        "gpu_p95_ms": None if steady is None or steady["gpu_max_ms"] <= 0 else steady["gpu_p95_ms"],
        "game_p95_ms": None if steady is None else steady["game_p95_ms"],
        "rss_mb": None if rss is None else rss.get("max_mb"),
    }
    checks = []
    for name, limit in budgets.items():
        value = values[name]
        note = ""
        if value is None:
            note = "no measurement" if name != "gpu_p95_ms" else "no GPUTime measurement (the capture's GPUTime column is zero or absent)"
        checks.append({"name": name, "label": BUDGET_LABELS[name], "value": value, "limit": limit, "passed": value is not None and value <= limit, "note": note})
    return checks


def build_report(measured, resume, rss, resolution, budgets=BUDGETS, extra=None):
    """Verdict from the measured pass (`analyze_pass` result), the fresh-process resume pass and the RSS summary of the
    whole run. The steady state is the measured pass outside its transition windows; the resume pass contributes the
    `resume` transition and its own (informational) steady state."""
    steady = summarize(measured["steady"])
    fight = summarize(measured["fight"])
    resume_steady = summarize(resume["steady"]) if resume else None
    transitions = [dict(t, **{"pass": "measured"}) for t in measured["transitions"]]
    if resume:
        transitions += [dict(t, **{"pass": "resume"}) for t in resume["transitions"]]
    failures = []
    for name in REQUIRED_TRANSITIONS:
        if not any(t["name"] == name for t in transitions):
            failures.append(f"The transition {name} was not found in the capture(s)")
    checks = check_budgets(steady, rss, resolution, budgets)
    for check in checks:
        if not check["passed"]:
            detail = check["note"] or f"{check['value']} > {check['limit']}"
            failures.append(f"{check['label']}: {detail}")
    gate = tuple(resolution) == GATE_RESOLUTION
    report = {
        "version": 1, "resolution": f"{resolution[0]}x{resolution[1]}", "gate_resolution": gate,
        "budgets": budgets, "checks": checks, "steady": steady, "fight": fight, "resume_steady": resume_steady,
        "rss": rss, "transitions": transitions, "beats": measured["beats"], "failures": failures,
        "passed": not failures and gate,
    }
    if not gate:
        report["failures"].append(f"Resolution {resolution[0]}x{resolution[1]} is not the gate resolution 1920x1080; the numbers are informational")
    if extra:
        report.update(extra)
    return report


def number(value, digits=2, unit=""):
    return "n/a" if value is None else f"{value:.{digits}f}{unit}"


def markdown_report(report):
    """The human-readable report: verdict, budgets, steady state, transitions and where the evidence lives."""
    lines = [f"# Performance gate: {'PASS' if report['passed'] else 'FAIL'}", ""]
    lines.append(f"Resolution {report['resolution']}"
                 + ("" if report["gate_resolution"] else " (not the gate resolution)")
                 + f"; mode {report.get('mode', '?')}; commit {report.get('commit', '?')}{' (dirty)' if report.get('dirty') else ''}; engine {report.get('engine', '?')}.")
    lines += ["", "## Budgets", "", "| Budget | Measured | Limit | Result |", "| --- | --- | --- | --- |"]
    for check in report["checks"]:
        unit = " MiB" if check["name"] == "rss_mb" else " ms"
        lines.append(f"| {check['label']} | {number(check['value'], 1 if check['name'] == 'rss_mb' else 2, unit)} | {number(check['limit'], 1, unit)} | {'pass' if check['passed'] else 'FAIL'} |")
    if report["failures"]:
        lines += ["", "Failures:", ""] + [f"- {failure}" for failure in report["failures"]]
    lines += ["", "## Steady state (transition windows excluded)", "", "| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |", "| --- | --- | --- | --- | --- | --- | --- |"]
    for label, key in (("measured pass, gate", "steady"), ("measured pass, fight only", "fight"), ("resume pass", "resume_steady")):
        row = report.get(key)
        if row:
            lines.append(f"| {row['frames']} | {label} | {row['frame_p50_ms']:.2f} ms | {row['frame_p95_ms']:.2f} ms | {row['frame_max_ms']:.2f} ms | {row['gpu_p95_ms']:.2f} ms | {row['game_p95_ms']:.2f} ms |")
    lines += ["", "## Transition hitches (reported separately; not in the steady state)", "",
              "| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |", "| --- | --- | --- | --- | --- | --- | --- |"]
    for t in report["transitions"]:
        lines.append(f"| {t['name']} | {t['pass']} | {t['worst_frame_ms']:.1f} ms (frame {t['worst_frame_index']}) | {t['duration_ms']:.0f} ms | {t['frames']} | {t['worst_gpu_ms']:.1f} ms | {t['worst_game_ms']:.1f} ms |")
    rss = report.get("rss") or {}
    lines += ["", "## Memory", "",
              f"VmRSS sampled every {rss.get('interval_s', SAMPLE_SECONDS)} s ({rss.get('samples', 0)} samples): max {number(rss.get('max_sampled_mb'), 1, ' MiB')}; "
              f"kernel high-water mark (VmHWM) {number(rss.get('peak_hwm_mb'), 1, ' MiB')}. The budget uses the larger."]
    if report.get("top_gpu_stats"):
        lines += ["", "## Most expensive GPU passes (steady state, -csvGpuStats)", "", "| Pass | p95 | mean |", "| --- | --- | --- |"]
        lines += [f"| {s['stat']} | {s['p95_ms']:.2f} ms | {s['mean_ms']:.2f} ms |" for s in report["top_gpu_stats"]]
    if "warmup" in report:
        gaps = ", ".join(f"{g['from']} to {g['to']}: {g['seconds']:.0f} s" for g in report.get("pass_gaps", [])) or "none"
        lines += ["", "## Run", "", f"Warm-up pass: {'yes' if report['warmup'] else 'NO (not a warm gate)'}. Idle time between passes (other jobs may have run): {gaps}."]
    if report.get("passes"):
        lines += ["", "## Passes", "", "| Pass | Exit | Log | CSV |", "| --- | --- | --- | --- |"]
        lines += [f"| {p['name']} | {p.get('exit_code')} | {p.get('log')} | {p.get('csv') or 'n/a'} |" for p in report["passes"]]
    if report.get("hardware"):
        lines += ["", "## Host", ""] + [f"- {key}: {value}" for key, value in report["hardware"].items()]
    lines.append("")
    return "\n".join(lines)


def hardware_summary(proc_root="/proc", run=None):
    """Best-effort host facts for the record (CPU model, memory, kernel, GPU); never raises."""
    facts = {}
    try:
        for line in (Path(proc_root) / "cpuinfo").read_text().splitlines():
            if line.startswith("model name"):
                facts["cpu"] = line.split(":", 1)[1].strip()
                break
        facts["logical_cpus"] = os.cpu_count()
        meminfo = (Path(proc_root) / "meminfo").read_text()
        total = parse_status_kb(meminfo, "MemTotal")
        if total:
            facts["memory_total_mb"] = round(total / ONE_MIB)
        facts["kernel"] = (Path(proc_root) / "sys" / "kernel" / "osrelease").read_text().strip()
    except OSError:
        pass
    if run:
        try:
            gpu = run(["nvidia-smi", "--query-gpu=name,driver_version,memory.total", "--format=csv,noheader"])
            if gpu:
                facts["gpu"] = gpu.strip().splitlines()[0]
        except Exception:  # Best effort: a missing nvidia-smi, a timeout or a non-zero exit all just leave the GPU out.
            pass
    return facts
