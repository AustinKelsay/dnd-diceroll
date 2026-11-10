"""Offline tests for `tools/dev.py perf` (#148): the CSV analyzer, the RSS sampler, the report and the pass orchestration."""
import contextlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import time
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import dev as dev_module
import perf
from dev import Dev
from workspace import Workspace

HEADER = "EVENTS,FrameTime,GameThreadTime,RenderThreadTime,GPUTime,GPU/Basepass"


def row(frame, game=2.0, gpu=6.0, events=()):
    return f"{';'.join(events)},{frame},{game},1.5,{gpu},{gpu / 2}"


def capture(fight_ms=9.0, fight_frames=200, fight_gpu=6.0, fight_game=2.0, resume=False, hitches=None, drop=(), extra_rows=(), done="ok"):
    """A synthetic CSV capture of one autoplay pass: engine start-up, markers, transition hitches, a steady fight and a tail."""
    hitches = {"start->route": 120.0, "route->battle": 80.0, "battle->reward": 60.0, "resume": 150.0, **(hitches or {})}
    rows = [row(900.0, events=["unrelated event"]) for _ in range(3)]
    rows.append(row(10.0, events=[f"AUTOPLAY_BEGIN:watchtower-road:{'resume' if resume else 'fresh'}"]))

    def steady(count, ms=8.0):
        rows.extend(row(ms) for _ in range(count))

    def transition(name, frames=5):
        if name in drop:
            steady(frames)
            return
        rows.append(row(hitches.get(name, 40.0), gpu=11.0, game=9.0, events=[f"AUTOPLAY_TRANSITION:{name}:begin"]))
        rows.extend(row(14.0) for _ in range(frames - 2))
        rows.append(row(10.0, events=[f"AUTOPLAY_TRANSITION:{name}:end"]))
        rows.extend(row(30.0) for _ in range(perf.TRANSITION_PAD))  # inside the pad: still not steady state

    if resume:
        steady(8)
        transition("resume")
        steady(12)
        transition("reward->route")
    else:
        steady(10)
        transition("plan", 2)
        steady(6)
        transition("start->route")
        steady(10)
        transition("route->battle")
        rows.extend(row(fight_ms, game=fight_game, gpu=fight_gpu, events=["AUTOPLAY_BEAT:move"] if i == 0 else []) for i in range(fight_frames))
        transition("battle->reward")
        steady(6)
        transition("quit")
    rows.append(row(10.0, events=[f"AUTOPLAY_DONE:{done}"]))
    rows.extend(row(500.0) for _ in range(2))  # profiler flush after the run: outside the window
    rows.extend(extra_rows)
    return "\n".join([HEADER, *rows, HEADER, "[Platform],Linux", '[Commandline],"-ExpeditionAutoplay=/x.json"']) + "\n"


class PercentileTests(unittest.TestCase):
    def test_linear_interpolation(self):
        values = list(range(1, 101))
        self.assertAlmostEqual(perf.percentile(values, 50), 50.5)
        self.assertAlmostEqual(perf.percentile(values, 95), 95.05)
        self.assertEqual(perf.percentile([3.0], 95), 3.0)
        self.assertEqual(perf.percentile([5, 1, 3], 100), 5)
        with self.assertRaises(ValueError): perf.percentile([], 50)
        with self.assertRaises(ValueError): perf.percentile([1], 101)


class CaptureTests(unittest.TestCase):
    def test_parse_skips_metadata_and_repeated_header(self):
        frames = perf.parse_csv(capture())
        self.assertTrue(all(isinstance(f.frame, float) for f in frames))
        self.assertEqual(frames[0].frame, 900.0)
        self.assertEqual(frames[0].stats["GPU/Basepass"], 3.0)
        self.assertNotIn("[Platform]", " ".join(e for f in frames for e in f.events))

    def test_markers_ignore_the_profilers_time_suffix(self):
        # The CSV profiler records events as "<text>##<seconds>", several per frame separated by ";".
        frames = perf.parse_csv(HEADER + '\n"AUTOPLAY_BEGIN:watchtower:fresh##1.5",5,1,1,2,1\n'
                                + '"AUTOPLAY_TRANSITION:quit:end##69.56;AUTOPLAY_DONE:ok##69.56",5,1,1,2,1\n')
        markers = perf.parse_markers(frames)
        self.assertIn((1, "DONE", "ok", ""), markers)
        self.assertIn((0, "BEGIN", "watchtower", "fresh"), markers)

    def test_startup_sigfpe_on_a_headless_output_is_recognised(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        folder = Path(temp.name)
        log, console = folder / "game.log", folder / "console.log"
        console.write_text("Signal 8 caught.\n#3 handle_wl_output_done (libSDL3)\n")
        log.write_text("LogInit: Initializing SDL.\n")
        self.assertTrue(Dev.perf_startup_sigfpe(-8, log, console))
        self.assertTrue(Dev.perf_startup_sigfpe(136, log, console))
        self.assertFalse(Dev.perf_startup_sigfpe(1, log, console))
        log.write_text("AUTOPLAY_BEGIN watchtower fresh\n")
        self.assertFalse(Dev.perf_startup_sigfpe(-8, log, console))

    def test_parse_csv_accepts_a_long_metadata_field(self):
        # A real capture ends with metadata rows whose command-line field exceeds csv's default 128 KiB limit.
        long_row = "[Commandline]," + "x" * 200000
        frames = perf.parse_csv(HEADER + '\n,5,1,1,2,1\n' + long_row + "\n")
        self.assertEqual(len(frames), 1)

    def test_parse_rejects_other_files_and_missing_columns(self):
        with self.assertRaisesRegex(ValueError, "EVENTS"): perf.parse_csv("a,b\n1,2\n")
        with self.assertRaisesRegex(ValueError, "GPUTime"): perf.parse_csv("EVENTS,FrameTime,GameThreadTime\n,1,1\n")
        with self.assertRaisesRegex(ValueError, "no frames"): perf.parse_csv(HEADER + "\n")

    def test_events_split_on_semicolons(self):
        frames = perf.parse_csv(HEADER + '\n"AUTOPLAY_TRANSITION:a:begin;AUTOPLAY_TRANSITION:a:end",5,1,1,2,1\n')
        self.assertEqual(perf.parse_markers(frames), [(0, "TRANSITION", "a", "begin"), (0, "TRANSITION", "a", "end")])

    def test_analysis_takes_transition_windows_out_of_the_steady_state(self):
        frames = perf.parse_csv(capture())
        result = perf.analyze_pass(frames)
        names = [t["name"] for t in result["transitions"]]
        self.assertEqual(names, ["plan", "start->route", "route->battle", "battle->reward", "quit"])
        by_name = {t["name"]: t for t in result["transitions"]}
        self.assertEqual(by_name["start->route"]["worst_frame_ms"], 120.0)
        self.assertEqual(by_name["route->battle"]["worst_frame_ms"], 80.0)
        self.assertEqual(by_name["battle->reward"]["worst_frame_ms"], 60.0)
        self.assertEqual(by_name["start->route"]["frames"], 5)
        self.assertAlmostEqual(by_name["start->route"]["duration_ms"], 120.0 + 3 * 14.0 + 10.0)
        self.assertEqual(by_name["start->route"]["worst_gpu_ms"], 11.0)
        self.assertEqual(by_name["start->route"]["worst_game_ms"], 9.0)
        steady = perf.summarize(result["steady"])
        # No hitch, no padded frame and no start-up or flush frame leaks into the steady state.
        self.assertLess(steady["frame_max_ms"], 20.0)
        self.assertEqual(steady["frame_p50_ms"], 9.0)
        # The steady runs between the markers, plus the BEGIN and DONE frames themselves.
        self.assertEqual(steady["frames"], 10 + 6 + 10 + 200 + 6 + 2)
        self.assertEqual(len(result["fight"]), 200)
        self.assertEqual(result["beats"], [{"frame": result["fight"][0].index, "name": "move"}])

    def test_percentiles_of_the_steady_state(self):
        # 100 fight frames at 10 ms, ten of them made 40 ms: the p95 lands among the slow ones, the median stays at 10.
        result = perf.analyze_pass(perf.parse_csv(capture(fight_ms=10.0, fight_frames=100)))
        for f in result["fight"][:10]:
            f.frame = 40.0
        summary = perf.summarize(result["steady"])
        self.assertGreater(summary["frame_p95_ms"], 10.0)
        self.assertLessEqual(summary["frame_p95_ms"], 40.0)
        self.assertEqual(summary["frame_p50_ms"], 10.0)

    def test_broken_captures_are_named(self):
        with self.assertRaisesRegex(ValueError, "never ended"):
            perf.analyze_pass(perf.parse_csv(capture(drop=()).replace("AUTOPLAY_TRANSITION:route->battle:end", "x")))
        with self.assertRaisesRegex(ValueError, "AUTOPLAY_DONE"):
            perf.analyze_pass(perf.parse_csv(capture().replace("AUTOPLAY_DONE:ok", "x")))
        with self.assertRaisesRegex(ValueError, "failed"):
            perf.analyze_pass(perf.parse_csv(capture(done="failed")))
        with self.assertRaisesRegex(ValueError, "AUTOPLAY_BEGIN"):
            perf.analyze_pass(perf.parse_csv(capture().replace("AUTOPLAY_BEGIN", "x")))
        with self.assertRaisesRegex(ValueError, "began twice"):
            perf.transition_windows([(1, "TRANSITION", "a", "begin"), (2, "TRANSITION", "a", "begin")])
        with self.assertRaisesRegex(ValueError, "without beginning"):
            perf.transition_windows([(1, "TRANSITION", "a", "end")])

    def test_top_gpu_passes(self):
        result = perf.analyze_pass(perf.parse_csv(capture()))
        top = perf.top_gpu_stats(result["steady"])
        self.assertEqual(top[0]["stat"], "GPU/Basepass")
        self.assertEqual(top[0]["p95_ms"], 3.0)


class VerdictTests(unittest.TestCase):
    RSS = {"samples": 10, "interval_s": 0.5, "max_sampled_mb": 3000.0, "peak_hwm_mb": 3100.0, "max_mb": 3100.0}

    def report(self, fight_ms=9.0, fight_gpu=6.0, fight_game=2.0, rss=None, resolution=(1920, 1080), drop=(), resume=True):
        measured = perf.analyze_pass(perf.parse_csv(capture(fight_ms=fight_ms, fight_gpu=fight_gpu, fight_game=fight_game, drop=drop)))
        second = perf.analyze_pass(perf.parse_csv(capture(resume=True))) if resume else None
        return perf.build_report(measured, second, rss or self.RSS, resolution)

    def test_all_four_budgets_pass(self):
        report = self.report()
        self.assertTrue(report["passed"], report["failures"])
        self.assertEqual([c["name"] for c in report["checks"]], ["frame_p95_ms", "gpu_p95_ms", "game_p95_ms", "rss_mb"])
        self.assertEqual({t["name"] for t in report["transitions"]} >= set(perf.REQUIRED_TRANSITIONS), True)
        self.assertEqual({t["pass"] for t in report["transitions"]}, {"measured", "resume"})

    def test_each_budget_can_fail_alone(self):
        for label, kwargs in (("p95 FrameTime", {"fight_ms": 17.5}), ("p95 GPUTime", {"fight_gpu": 12.5}),
                              ("p95 GameThreadTime", {"fight_game": 4.5}), ("max RSS", {"rss": dict(self.RSS, max_mb=5200.0)})):
            report = self.report(**kwargs)
            self.assertFalse(report["passed"], label)
            failing = [c["label"] for c in report["checks"] if not c["passed"]]
            self.assertEqual(failing, [label])

    def test_limits_are_inclusive(self):
        report = self.report(fight_ms=16.7, fight_gpu=12.0, fight_game=4.0, rss=dict(self.RSS, max_mb=5120.0))
        self.assertTrue(report["passed"], report["failures"])

    def test_hitches_never_fail_the_steady_state(self):
        report = self.report()
        worst = max(t["worst_frame_ms"] for t in report["transitions"])
        self.assertGreater(worst, 100.0)
        self.assertTrue(report["passed"])

    def test_missing_evidence_fails(self):
        frames = perf.parse_csv(capture())
        for f in frames:
            f.gpu = 0.0  # a driver without GPU timestamps writes zeros
        no_gpu = perf.build_report(perf.analyze_pass(frames), perf.analyze_pass(perf.parse_csv(capture(resume=True))), self.RSS, (1920, 1080))
        self.assertIn("GPUTime", json.dumps(no_gpu["failures"]))
        self.assertFalse(no_gpu["passed"])
        self.assertFalse(self.report(rss=dict(self.RSS, max_mb=None))["passed"])
        self.assertFalse(self.report(drop=("route->battle",))["passed"])
        self.assertIn("route->battle", json.dumps(self.report(drop=("route->battle",))["failures"]))
        missing_resume = self.report(resume=False)
        self.assertFalse(missing_resume["passed"])
        self.assertIn("resume", json.dumps(missing_resume["failures"]))

    def test_other_resolutions_are_informational(self):
        report = self.report(resolution=(1280, 720))
        self.assertFalse(report["passed"])
        self.assertFalse(report["gate_resolution"])
        self.assertIn("gate resolution", json.dumps(report["failures"]))

    def test_markdown_names_the_verdict_and_every_transition(self):
        text = perf.markdown_report(dict(self.report(), mode="packaged", commit="abc", passes=[{"name": "measured", "exit_code": 0, "log": "m.log", "csv": "m.csv"}]))
        self.assertIn("Performance gate: PASS", text)
        for name in perf.REQUIRED_TRANSITIONS:
            self.assertIn(f"| {name} |", text)
        self.assertIn("p95 FrameTime", text)
        failed = perf.markdown_report(self.report(fight_ms=30.0))
        self.assertIn("Performance gate: FAIL", failed)
        self.assertIn("FAIL", failed)


class MemoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.proc = Path(self.temp.name)

    def status(self, pid, rss, hwm=None, name="DnDDiceRoll"):
        folder = self.proc / str(pid)
        folder.mkdir(exist_ok=True)
        lines = [f"Name:\t{name}", f"VmPeak:\t  999999 kB", f"VmHWM:\t  {hwm or rss} kB", f"VmRSS:\t  {rss} kB", "Threads:\t12"]
        (folder / "status").write_text("\n".join(lines) + "\n")
        return folder

    def test_status_parsing(self):
        text = "VmHWM:\t  2048 kB\nVmRSS:\t  1024 kB\nRssAnon:\t 5 kB\n"
        self.assertEqual(perf.parse_status_kb(text, "VmRSS"), 1024)
        self.assertEqual(perf.parse_status_kb(text, "VmHWM"), 2048)
        self.assertIsNone(perf.parse_status_kb(text, "VmSwap"))

    def test_samples_track_the_maximum_and_the_high_water_mark(self):
        sampler = perf.RssSampler(4242, proc_root=self.proc)
        for rss, hwm in ((1024 * 100, 1024 * 100), (1024 * 400, 1024 * 400), (1024 * 250, 1024 * 900)):
            self.status(4242, rss, hwm)
            self.assertTrue(sampler.sample())
        summary = sampler.summary()
        self.assertEqual(summary["samples"], 3)
        self.assertEqual(summary["max_sampled_mb"], 400.0)
        self.assertEqual(summary["peak_hwm_mb"], 900.0)
        self.assertEqual(summary["max_mb"], 900.0)  # the budget sees a spike between two samples

    def test_a_missing_process_ends_sampling(self):
        sampler = perf.RssSampler(31337, proc_root=self.proc)
        self.assertFalse(sampler.sample())
        self.assertEqual(sampler.summary()["samples"], 0)
        self.assertIsNone(sampler.summary()["max_mb"])

    def test_the_thread_samples_until_stopped_or_the_process_exits(self):
        folder = self.status(77, 1024 * 10)
        with perf.RssSampler(77, interval=0.01, proc_root=self.proc) as sampler:
            time.sleep(0.1)
            self.status(77, 1024 * 30)
            time.sleep(0.1)
        self.assertGreaterEqual(sampler.summary()["samples"], 3)
        self.assertEqual(sampler.summary()["max_sampled_mb"], 30.0)
        sampler = perf.RssSampler(77, interval=0.01, proc_root=self.proc)
        (folder / "status").unlink()
        folder.rmdir()
        with sampler:
            time.sleep(0.05)
        self.assertEqual(sampler.summary()["samples"], 0)

    def test_the_game_process_is_found_under_a_launcher(self):
        launcher = self.proc / "100"
        (launcher / "task" / "100").mkdir(parents=True)
        (launcher / "cmdline").write_text("/bin/sh\0run.sh\0")
        (launcher / "task" / "100" / "children").write_text("101 102 ")
        for pid, command in ((101, "/usr/bin/crashreporter\0"), (102, "/opt/game/Binaries/Linux/DnDDiceRoll\0-windowed\0")):
            (self.proc / str(pid)).mkdir()
            (self.proc / str(pid) / "cmdline").write_text(command)
        self.assertEqual(perf.resolve_game_pid(100, "DnDDiceRoll", proc_root=self.proc), 102)
        self.assertEqual(perf.resolve_game_pid(102, "DnDDiceRoll", proc_root=self.proc), 102)
        self.assertEqual(perf.resolve_game_pid(555, "DnDDiceRoll", proc_root=self.proc), 555)  # unknown: sample the pid itself


class LocatingTests(unittest.TestCase):
    def test_the_logged_path_wins_then_the_newest_recent_file(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            old, new, logged = root / "a" / "old.csv", root / "b" / "new.csv", root / "c" / "logged.csv"
            for path in (old, new, logged):
                path.parent.mkdir()
                path.write_text("x")
            os.utime(old, (1000, 1000))
            log = f"LogTemp: hello\nAUTOPLAY_CSV path={logged}\n"
            self.assertEqual(perf.find_csv(log, [root / "a", root / "b"], since=2000), logged)
            self.assertEqual(perf.find_csv("no path\n", [root / "a", root / "b"], since=2000), new)
            self.assertIsNone(perf.find_csv("no path\n", [root / "a"], since=2000))
            self.assertIsNone(perf.find_csv("", [root / "missing"], since=0))
            # A logged path that does not exist falls back to the search.
            self.assertEqual(perf.find_csv(f"AUTOPLAY_CSV path={root / 'gone.csv'}\n", [root / "b"], since=2000), new)

    def test_candidate_directories(self):
        with tempfile.TemporaryDirectory() as temp:
            package = Path(temp) / "pkg"
            staged = package / "Linux" / "DnDDiceRoll" / "Saved" / "Profiling" / "CSV"
            staged.mkdir(parents=True)
            found = perf.csv_directories("DnDDiceRoll", package=package, project_root="/proj", home="/home/x")
            self.assertIn(staged, found)
            self.assertIn(Path("/home/x/.config/Epic/DnDDiceRoll/Saved/Profiling/CSV"), found)
            self.assertIn(Path("/home/x/.config/Epic/UnrealEngine/5.8/Saved/Profiling/CSV"), found)
            self.assertIn(Path("/proj/Saved/Profiling/CSV"), found)


class PerfCommandTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="unreal workspace ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in ("Config", "tools", "Source/Game"):
            (self.root / name).mkdir(parents=True)
        (self.root / "Game.uproject").write_text(json.dumps({"Modules": [{"Name": "Game"}]}))
        (self.root / "tools/project.json").write_text(json.dumps({"module": "Game", "editor_target": "CustomEditor", "game_target": "CustomGame", "content_root": "/Game/Starter"}))
        (self.root / "tools/versions.json").write_text(json.dumps({"unreal_engine": "5.8.4", "unreal_changelist": 58902052, "mcp_url": "http://127.0.0.1:18765/mcp"}))
        (self.root / "Config/DefaultEngine.ini").write_text("[/Script/EngineSettings.GameMapsSettings]\nEditorStartupMap=/Game/Starter/Sandbox\n")
        for target in ("CustomEditor", "CustomGame"):
            (self.root / f"Source/{target}.Target.cs").touch()
        self.w = Workspace.discover(self.root)

    def prepare(self, package=True, **kwargs):
        dev = Dev(self.w, host="linux", cores=2)
        self.w.local.mkdir(exist_ok=True)
        (self.root / "tools/autoplay").mkdir(parents=True, exist_ok=True)
        (self.root / "tools/autoplay/watchtower-road.json").write_text(json.dumps({"version": 1, "party": ["a", "b", "c", "d"]}))
        if package:
            binary = self.root / "Builds/20260101T000000Z/Linux/CustomGame/Binaries/Linux/CustomGame"
            binary.parent.mkdir(parents=True)
            binary.write_text("#!/bin/sh\n")
            binary.chmod(0o755)
            (binary.parents[3] / "verified.json").parent.mkdir(exist_ok=True)
            (self.root / "Builds/20260101T000000Z/verified.json").write_text("{}")
        patches = [patch.object(dev, "require_engine"), patch.object(dev, "require_closed"), patch.object(dev, "perf_crash_dirs", return_value=[self.root]),
                   patch.object(dev, "execute", return_value=Mock(stdout="abc123\n")), patch.object(perf, "hardware_summary", return_value={"cpu": "test"})]
        for item in patches:
            item.start()
            self.addCleanup(item.stop)
        return dev

    def fake_game(self, calls, outcomes=None):
        """A stand-in for perf_run: records the command and writes the log and CSV a real pass would."""
        outcomes = outcomes or {}

        def run(args, log, timeout, cwd):
            args = [str(a) for a in args]
            folder = Path(next(a for a in args if a.startswith("-abslog=")).split("=", 1)[1]).parent
            name = folder.name
            calls.append({"name": name, "args": args, "timeout": timeout, "cwd": cwd})
            outcome = outcomes.get(name, {})
            save = Path(next(a for a in args if a.startswith("-ExpeditionSaveRoot=")).split("=", 1)[1])
            if "-ExpeditionAutoplayResume" not in args:
                (save / "current-v1.json").write_text('{"checkpoint": "after the fight"}')
            else:
                (save / "current-v1.json").write_text('{"checkpoint": "after the camp"}')
            csv_path = self.root / f"{name}.csv"
            csv_path.write_text(outcome.get("csv") or capture(resume="-ExpeditionAutoplayResume" in args, fight_ms=outcome.get("fight_ms", 9.0)))
            text = outcome.get("log", f"LogTemp: starting\nAUTOPLAY_DONE ok pass=x\nAUTOPLAY_CSV path={csv_path}\nLogExit: Exiting.\n")
            Path(next(a for a in args if a.startswith("-abslog=")).split("=", 1)[1]).write_text(text)
            if "crash" in outcome:
                (self.root / "Saved/Crashes" / outcome["crash"]).mkdir(parents=True)
            return outcome.get("code", 0), {"samples": 4, "interval_s": 0.5, "max_sampled_mb": 2000.0, "peak_hwm_mb": 2100.0, "max_mb": 2100.0}

        return run

    def test_runs_warmup_then_measured_then_a_fresh_process_resume(self):
        dev = self.prepare()
        calls = []
        with patch.object(dev, "perf_run", side_effect=self.fake_game(calls)):
            report = dev.perf()
        self.assertEqual([c["name"] for c in calls], ["warmup", "measured", "resume"])
        for call in calls:
            args = call["args"]
            self.assertTrue(args[0].endswith("Binaries/Linux/CustomGame"))
            for needed in ("-windowed", "-ForceRes", "-ResX=1920", "-ResY=1080", "-csvGpuStats", "-novsync", "-corelimit=2", "-unattended"):
                self.assertIn(needed, args)
            self.assertTrue(any(a.startswith("-csvCaptureFrames=200000") for a in args))
            self.assertTrue(any(a.startswith("-ExpeditionAutoplay=") and a.endswith("watchtower-road.json") for a in args))
            self.assertNotIn("-RenderOffscreen", args)
            self.assertNotIn("-nosound", args)
            self.assertEqual(call["timeout"], 1800)
        self.assertEqual(["-ExpeditionAutoplayResume" in c["args"] for c in calls], [False, False, True])
        saves = {c["name"]: Path(next(a for a in c["args"] if a.startswith("-ExpeditionSaveRoot=")).split("=", 1)[1]) for c in calls}
        self.assertEqual(len({str(p) for p in saves.values()}), 3)
        # The resume pass worked on a copy: the measured pass's checkpoint is still the one the fight left.
        self.assertIn("after the fight", (saves["measured"] / "current-v1.json").read_text())
        self.assertIn("after the camp", (saves["resume"] / "current-v1.json").read_text())
        self.assertTrue(report["passed"], report["failures"])
        output = next((self.w.local / "perf").iterdir())
        written = json.loads((output / "report.json").read_text())
        self.assertEqual(written["passed"], True)
        self.assertEqual([p["name"] for p in written["passes"]], ["warmup", "measured", "resume"])
        self.assertEqual(written["commit"], "abc123")
        self.assertEqual({t["name"] for t in written["transitions"]} >= set(perf.REQUIRED_TRANSITIONS), True)
        self.assertIn("Performance gate: PASS", (output / "report.md").read_text())
        self.assertTrue((output / "measured" / "measured.csv").is_file())
        self.assertTrue((output / "resume" / "resume.csv").is_file())
        self.assertFalse((output / "warmup" / "warmup.csv").exists())

    def test_a_slow_fight_fails_the_gate_but_still_writes_the_report(self):
        dev = self.prepare()
        calls = []
        with patch.object(dev, "perf_run", side_effect=self.fake_game(calls, {"measured": {"fight_ms": 25.0}})):
            report = dev.perf()
        self.assertFalse(report["passed"])
        self.assertTrue(any("FrameTime" in f for f in report["failures"]))
        self.assertTrue((next((self.w.local / "perf").iterdir()) / "report.md").is_file())

    def test_dry_run_flags(self):
        dev = self.prepare()
        calls = []
        with patch.object(dev, "perf_run", side_effect=self.fake_game(calls)):
            dev.perf(warmup=False, offscreen=True, vsync=True, sound=False, resolution="1280x720", frames=5000, timeout=120)
        self.assertEqual([c["name"] for c in calls], ["measured", "resume"])
        args = calls[0]["args"]
        self.assertIn("-RenderOffscreen", args)
        self.assertIn("-nosound", args)
        self.assertNotIn("-novsync", args)
        self.assertIn("-ResX=1280", args)
        self.assertIn("-csvCaptureFrames=5000", args)
        self.assertEqual(calls[0]["timeout"], 120)

    def test_the_editor_dry_run_never_passes_the_gate(self):
        dev = self.prepare(package=False)
        identity = {"fake": "identity"}
        (self.w.local / "build.json").write_text(json.dumps(identity))
        calls = []
        with patch.object(dev, "build_identity", return_value=identity), patch.object(dev, "perf_run", side_effect=self.fake_game(calls)):
            report = dev.perf(editor=True)
        self.assertEqual(calls[0]["args"][0], str(dev.editor))
        self.assertEqual(calls[0]["args"][1:3], [str(self.w.project), "-game"])
        self.assertFalse(report["passed"])
        self.assertIn("not a packaged", json.dumps(report["failures"]))
        (self.w.local / "build.json").write_text('{"stale": true}')
        with patch.object(dev, "build_identity", return_value=identity), self.assertRaisesRegex(RuntimeError, "stale"):
            dev.perf(editor=True)

    def test_a_pass_must_exit_cleanly_and_report_done(self):
        for outcome, message in (({"code": 1}, "exited 1"), ({"code": -11}, "signal 11"), ({"crash": "crashinfo-1"}, "crash report"),
                                 ({"log": "LogExit: Exiting.\n"}, "AUTOPLAY_DONE"),
                                 ({"log": "AUTOPLAY_FAIL stage=fight x\nAUTOPLAY_DONE ok\nLogExit: Exiting.\n"}, "AUTOPLAY_FAIL"),
                                 ({"log": "AUTOPLAY_DONE ok\n"}, "normal exit")):
            with self.subTest(message=message):
                dev = self.prepare()
                (self.root / "Saved/Crashes").mkdir(parents=True, exist_ok=True)
                with patch.object(dev, "perf_run", side_effect=self.fake_game([], {"warmup": outcome})):
                    with self.assertRaisesRegex(RuntimeError, message): dev.perf()
                for path in (self.root / "Builds", self.w.local / "perf", self.root / "Saved"):
                    if path.exists():
                        shutil.rmtree(path)

    def test_a_missing_capture_names_where_it_looked(self):
        dev = self.prepare()
        calls = []
        base = self.fake_game(calls)

        def run(args, log, timeout, cwd):
            result = base(args, log, timeout, cwd)
            Path(next(str(a) for a in args if str(a).startswith("-abslog=")).split("=", 1)[1]).write_text("AUTOPLAY_DONE ok\nLogExit: Exiting.\n")
            return result

        with patch.object(dev, "perf_run", side_effect=run), patch.object(perf, "csv_directories", return_value=[self.root / "nowhere"]):
            with self.assertRaisesRegex(RuntimeError, "no CSV capture"): dev.perf(warmup=False)

    def test_argument_handling(self):
        dev = self.prepare()
        with self.assertRaisesRegex(ValueError, "must look like"): dev.perf(resolution="big")
        with self.assertRaisesRegex(ValueError, "exclusive"): dev.perf(editor=True, package=str(self.root))
        with self.assertRaisesRegex(ValueError, "--csv-frames"): dev.perf(frames=10)
        with self.assertRaisesRegex(ValueError, "--csv-frames"): dev.perf(frames=10 ** 9)
        with self.assertRaisesRegex(ValueError, "--timeout"): dev.perf(timeout=5)
        with self.assertRaisesRegex(RuntimeError, "does not exist"): dev.perf(script="nope.json")
        (self.root / "bad.json").write_text("{}")
        with self.assertRaisesRegex(RuntimeError, "invalid"): dev.perf(script="bad.json")
        (self.root / "worse.json").write_text("not json")
        with self.assertRaisesRegex(RuntimeError, "invalid"): dev.perf(script="worse.json")
        with self.assertRaisesRegex(RuntimeError, "not a folder"): dev.perf(package=str(self.root / "missing"))
        shutil.rmtree(self.root / "Builds")
        with self.assertRaisesRegex(RuntimeError, "No finished package"): dev.perf()

    def test_the_newest_finished_package_is_chosen(self):
        dev = self.prepare()
        newer = self.root / "Builds/20270101T000000Z/Linux/CustomGame/Binaries/Linux/CustomGame"
        newer.parent.mkdir(parents=True)
        newer.write_text("x")
        newer.chmod(0o755)
        binary, folder = dev.perf_binary()
        self.assertIn("20260101", str(binary))  # the newer folder never finished (no verified.json)
        (self.root / "Builds/20270101T000000Z/verified.json").write_text("{}")
        binary, folder = dev.perf_binary()
        self.assertIn("20270101", str(binary))
        self.assertEqual(folder.name, "20270101T000000Z")
        explicit, _ = dev.perf_binary(str(self.root / "Builds/20260101T000000Z"))
        self.assertIn("20260101", str(explicit))

    def test_command_line_wiring(self):
        self.assertIn("perf", dev_module.CORE_COMMANDS)
        source = Path(dev_module.__file__).read_text()
        self.assertIn("dev.perf(args.package, args.editor, args.script, args.res, args.csv_frames, args.timeout", source)
        with patch.object(sys, "argv", ["dev.py", "--cores", "2", "perf", "--help"]), patch("sys.stdout"), self.assertRaises(SystemExit) as raised:
            dev_module.main()
        self.assertEqual(raised.exception.code, 0)

    def stamp_of(self, call, *args, **kwargs):
        """Run a perf call and return (its result, the PERF_RUN stamp it printed)."""
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            result = call(*args, **kwargs)
        found = re.search(r"PERF_RUN (\w+)", out.getvalue())
        return result, found.group(1) if found else None

    def test_a_rebuilt_binary_or_edited_script_at_the_same_path_is_refused(self):
        dev = self.prepare()
        binary = self.root / "Builds/20260101T000000Z/Linux/CustomGame/Binaries/Linux/CustomGame"
        with patch.object(dev, "perf_run", side_effect=self.fake_game([])):
            _, run = self.stamp_of(dev.perf, only="measured")
            binary.write_text("#!/bin/sh\n# rebuilt\n")
            with self.assertRaisesRegex(RuntimeError, "binary_sha256 differs"): dev.perf(only="resume", run=run)
            binary.write_text("#!/bin/sh\n")
            script = self.root / "tools/autoplay/watchtower-road.json"
            script.write_text(json.dumps({"version": 1, "party": ["a", "b", "c", "e"]}))
            with self.assertRaisesRegex(RuntimeError, "script_sha256 differs"): dev.perf(only="resume", run=run)

    def test_passes_out_of_order_fail_the_report(self):
        dev = self.prepare()
        with patch.object(dev, "perf_run", side_effect=self.fake_game([])):
            _, run = self.stamp_of(dev.perf, only="measured")
            dev.perf(only="warmup", run=run)
            dev.perf(only="resume", run=run)
            with self.assertRaisesRegex(RuntimeError, "measured started before warmup ended"): dev.perf(only="report", run=run)

    def test_the_package_commit_is_recorded_from_its_verified_json(self):
        dev = self.prepare()
        (self.root / "Builds/20260101T000000Z/verified.json").write_text(json.dumps({"commit": "pkg789"}))
        with patch.object(dev, "perf_run", side_effect=self.fake_game([])):
            _, run = self.stamp_of(dev.perf, only="measured")
        record = json.loads((self.w.local / "perf" / run / "measured" / "pass.json").read_text())
        self.assertEqual(record["package_commit"], "pkg789")
        self.assertEqual(len(record["binary_sha256"]), 64)

    def test_each_pass_runs_alone_and_the_report_is_built_from_the_run_folder(self):
        dev = self.prepare()
        calls = []
        with patch.object(dev, "perf_run", side_effect=self.fake_game(calls)):
            record, run = self.stamp_of(dev.perf, only="warmup")
            self.assertRegex(run, r"^\d{8}T\d+Z$")
            folder = self.w.local / "perf" / run
            self.assertEqual([c["name"] for c in calls], ["warmup"])
            warm = json.loads((folder / "warmup" / "pass.json").read_text())
            self.assertEqual((record["name"], warm["commit"], warm["dirty"], warm["exit_code"]), ("warmup", "abc123", True, 0))  # the mocked git status prints a line: dirty
            for key in ("log", "csv", "rss", "binary", "flags", "started_at", "ended_at", "started", "ended", "script", "cores"):
                self.assertIn(key, warm)
            self.assertEqual(warm["flags"]["resolution"], "1920x1080")
            self.assertFalse((folder / "measured").exists())
            self.assertFalse((folder / "report.json").exists())
            # The next two jobs name the run; each starts its own game process.
            _, again = self.stamp_of(dev.perf, only="measured", run=run)
            self.assertIsNone(again)  # a named run prints no new stamp
            self.assertTrue((folder / "measured" / "pass.json").is_file())
            self.assertTrue((folder / "measured" / "measured.csv").is_file())
            self.assertFalse((folder / "resume").exists())
            dev.perf(only="resume", run=run)
        self.assertEqual([c["name"] for c in calls], ["warmup", "measured", "resume"])
        self.assertEqual(["-ExpeditionAutoplayResume" in c["args"] for c in calls], [False, False, True])
        self.assertIn("after the fight", (folder / "measured" / "save" / "current-v1.json").read_text())
        self.assertIn("after the camp", (folder / "resume" / "save" / "current-v1.json").read_text())
        self.assertFalse((folder / "report.json").exists())
        # The report runs no Unreal: no engine check, no package lookup, no game.
        with patch.object(dev, "perf_run", side_effect=AssertionError("no game")), patch.object(dev, "require_engine", side_effect=AssertionError("no engine")), \
                patch.object(dev, "perf_binary", side_effect=AssertionError("no package")):
            report = dev.perf(only="report", run=run)
        self.assertTrue(report["passed"], report["failures"])
        written = json.loads((folder / "report.json").read_text())
        self.assertTrue(written["warmup"])
        self.assertEqual([p["name"] for p in written["passes"]], ["warmup", "measured", "resume"])
        self.assertEqual([(g["from"], g["to"]) for g in written["pass_gaps"]], [("warmup", "measured"), ("measured", "resume")])
        self.assertTrue(all(g["seconds"] >= 0 for g in written["pass_gaps"]))
        self.assertEqual(written["commit"], "abc123")
        self.assertEqual(written["hardware"], {"cpu": "test"})
        self.assertGreaterEqual({t["name"] for t in written["transitions"]}, set(perf.REQUIRED_TRANSITIONS))
        text = (folder / "report.md").read_text()
        self.assertIn("Warm-up pass: yes", text)
        self.assertIn("warmup to measured", text)

    def test_the_stepwise_report_equals_the_one_shot_report(self):
        dev = self.prepare()
        with patch.object(dev, "perf_run", side_effect=self.fake_game([])):
            one_shot = dev.perf()
            _, run = self.stamp_of(dev.perf, only="warmup")
            dev.perf(only="measured", run=run)
            dev.perf(only="resume", run=run)
            stepwise = dev.perf(only="report", run=run)
        for key in ("passed", "checks", "steady", "fight", "resume_steady", "transitions", "failures", "rss", "warmup", "commit", "binary", "flags", "resolution"):
            self.assertEqual(stepwise[key], one_shot[key], key)

    def test_a_measured_pass_without_a_run_starts_one_and_without_warmup_is_noted(self):
        dev = self.prepare()
        with patch.object(dev, "perf_run", side_effect=self.fake_game([])):
            _, run = self.stamp_of(dev.perf, only="measured")
            self.assertTrue((self.w.local / "perf" / run / "measured" / "pass.json").is_file())
            dev.perf(only="resume", run=run)
            report = dev.perf(only="report", run=run)
        self.assertFalse(report["warmup"])
        self.assertEqual([(g["from"], g["to"]) for g in report["pass_gaps"]], [("measured", "resume")])
        self.assertIn("NO (not a warm gate)", (self.w.local / "perf" / run / "report.md").read_text())

    def test_pass_error_cases(self):
        dev = self.prepare()
        with patch.object(dev, "perf_run", side_effect=self.fake_game([])):
            _, run = self.stamp_of(dev.perf, only="warmup")
            # Resume before the measured pass has run.
            with self.assertRaisesRegex(RuntimeError, "measured pass has not run"): dev.perf(only="resume", run=run)
            self.assertFalse((self.w.local / "perf" / run / "resume").exists())
            # A report needs both passes.
            with self.assertRaisesRegex(RuntimeError, "no measured or resume pass record"): dev.perf(only="report", run=run)
            dev.perf(only="measured", run=run)
            with self.assertRaisesRegex(RuntimeError, "no resume pass record"): dev.perf(only="report", run=run)
            # A pass does not run twice in one run folder.
            with self.assertRaisesRegex(RuntimeError, "already ran"): dev.perf(only="measured", run=run)
            # Unknown, malformed or missing run stamps and misplaced options.
            with self.assertRaisesRegex(RuntimeError, "No perf run"): dev.perf(only="report", run="20200101T000000Z")
            with self.assertRaisesRegex(ValueError, "--run takes the stamp"): dev.perf(only="report", run="../x")
            with self.assertRaisesRegex(ValueError, "needs --run"): dev.perf(only="resume")
            with self.assertRaisesRegex(ValueError, "needs --run"): dev.perf(only="report")
            with self.assertRaisesRegex(ValueError, "applies to --pass"): dev.perf(run=run)
            with self.assertRaisesRegex(ValueError, "--no-warmup"): dev.perf(only="warmup", warmup=False)
            with self.assertRaisesRegex(ValueError, "must be one of"): dev.perf(only="everything")
            with self.assertRaisesRegex(ValueError, "reads a run folder"): dev.perf(only="report", run=run, package="x")

    def test_passes_of_one_run_must_share_commit_binary_and_flags(self):
        dev = self.prepare()
        with patch.object(dev, "perf_run", side_effect=self.fake_game([])):
            _, run = self.stamp_of(dev.perf, only="measured")
            # Another commit between jobs: the resume refuses before launching the game.
            dev.execute.return_value = Mock(stdout="def456\n")
            with self.assertRaisesRegex(RuntimeError, r"commit differs.*abc123.*def456"): dev.perf(only="resume", run=run)
            self.assertFalse((self.w.local / "perf" / run / "resume").exists())
            dev.execute.return_value = Mock(stdout="abc123\n")
            # Other flags or another binary are refused too.
            with self.assertRaisesRegex(RuntimeError, "flags differs"): dev.perf(only="resume", run=run, vsync=True)
            with self.assertRaisesRegex(RuntimeError, "flags differs"): dev.perf(only="resume", run=run, resolution="1280x720")
            other = self.root / "Builds/20270101T000000Z/Linux/CustomGame/Binaries/Linux/CustomGame"
            other.parent.mkdir(parents=True)
            other.write_text("x")
            other.chmod(0o755)
            with self.assertRaisesRegex(RuntimeError, "binary differs"): dev.perf(only="resume", run=run, package=str(self.root / "Builds/20270101T000000Z"))
            dev.perf(only="resume", run=run)
            folder = self.w.local / "perf" / run
            # The report catches records that were made apart or edited.
            for key, value, message in (("commit", "zzz", "commit differs"), ("binary", "/else/Game", "binary differs"), ("flags", {"vsync": True}, "flags differs"), ("dirty", False, "dirty differs")):
                record = json.loads((folder / "resume" / "pass.json").read_text())
                changed = dict(record, **{key: value})
                (folder / "resume" / "pass.json").write_text(json.dumps(changed))
                with self.assertRaisesRegex(RuntimeError, message): dev.perf(only="report", run=run)
                (folder / "resume" / "pass.json").write_text(json.dumps(record))
            # This run skipped the warm-up, so the budgets are met but the gate is not: it wants a warm package.
            report = dev.perf(only="report", run=run)
            self.assertFalse(report["passed"])
            self.assertTrue(any("warm-up" in failure for failure in report["failures"]))

    def test_a_failed_pass_leaves_no_pass_record_and_the_default_stays_one_invocation(self):
        dev = self.prepare()
        with patch.object(dev, "perf_run", side_effect=self.fake_game([], {"measured": {"code": 1}})):
            _, run = self.stamp_of(dev.perf, only="warmup")
            with self.assertRaisesRegex(RuntimeError, "exited 1"): dev.perf(only="measured", run=run)
        self.assertFalse((self.w.local / "perf" / run / "measured" / "pass.json").exists())
        with patch.object(dev, "perf_run", side_effect=self.fake_game([])):
            report = dev.perf()
        runs = sorted((self.w.local / "perf").iterdir())
        full = runs[-1]
        self.assertTrue(all((full / n / "pass.json").is_file() for n in ("warmup", "measured", "resume")))
        self.assertTrue((full / "report.json").is_file() and report["passed"])

    def test_command_line_wiring_of_the_passes(self):
        source = Path(dev_module.__file__).read_text()
        self.assertIn("args.transition_pad, args.only, args.run)", source)
        self.assertEqual(dev_module.PERF_PASSES, ("warmup", "measured", "resume", "report"))
        out = io.StringIO()
        with patch.object(sys, "argv", ["dev.py", "perf", "--help"]), contextlib.redirect_stdout(out), self.assertRaises(SystemExit):
            dev_module.main()
        self.assertIn("--pass {warmup,measured,resume,report}", out.getvalue())
        self.assertIn("--run RUN", out.getvalue())

    def test_the_game_is_sampled_and_given_one_sigterm_on_timeout(self):
        dev = Dev(self.w, host="linux")
        process = Mock(pid=4321)
        process.wait.side_effect = [dev_module.subprocess.TimeoutExpired("game", 1), 0]
        sampler = Mock()
        sampler.__enter__ = Mock(return_value=sampler)
        sampler.__exit__ = Mock(return_value=False)
        sampler.summary.return_value = {}
        with tempfile.TemporaryDirectory() as temp, patch.object(dev_module.subprocess, "Popen", return_value=process), \
                patch.object(perf, "RssSampler", return_value=sampler) as made, patch.object(perf, "resolve_game_pid", return_value=4322):
            with self.assertRaisesRegex(RuntimeError, "asked to exit"):
                dev.perf_run(["/x/CustomGame"], Path(temp) / "game.log", 5, temp)
            made.assert_called_once_with(4322)
        process.terminate.assert_called_once()
        process.kill.assert_not_called()


if __name__ == "__main__":
    unittest.main()
