import csv
import io
import json
import os
import shutil
import subprocess
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import balance_report

RUN_FIELDS = ("party,archetype,builds,seed,route_pattern,outcome,stall_reason,encounters_won,rounds,deaths,survivors,damage,damage_taken,"
              "healing,protection,specials_used,potions_used,commands,policy_errors,illegal_proposals,passive_ends,protection_mismatches").split(",")
ENCOUNTER_FIELDS = ("party,seed,stage,route,recipe,width,height,enemies,outcome,rounds,conscious_start,hp_start,hp_low,hp_end,hp_max,damage,"
                    "damage_taken,healing,protection,downs,deaths,revives,specials_used").split(",")
HERO_FIELDS = ("party,archetype,seed,outcome,slot,build,class,survived,death_stage,death_cause,level,turns,idle_turns,damage,kills,healing,"
               "protection,damage_taken,downs,revives,potions_used,s1_command,s1_available_turns,s1_uses,s1_blockers,"
               "s2_command,s2_available_turns,s2_uses,s2_blockers,kit_left,kit_cap,slot_left,slot_cap,wind_left,wind_cap,"
               "maneuver_left,maneuver_cap,rage_left,rage_cap,potions_left").split(",")


def write(path, fields, rows):
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "" if field.endswith("_blockers") else 0) for field in fields})


class BalanceReportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        # A stand-in checkout: results must live under its .local/.
        self.local = Path(self.temp.name) / ".local"
        self.root = self.local / "balance" / "run"
        self.root.mkdir(parents=True)

    def group(self, name, runs):
        """runs: list of (party, seed, outcome, fights, hero kits with (survived, damage))."""
        folder = self.root / name
        folder.mkdir()
        run_rows, encounter_rows, hero_rows = [], [], []
        for party, seed, outcome, fights, heroes in runs:
            won = fights if outcome == "Victory" else fights - 1
            run_rows.append({"party": party, "archetype": "rotation", "builds": ";".join(k for k, _, _ in heroes), "seed": seed, "outcome": outcome,
                             "stall_reason": "fight 2 reached round 60" if outcome == "Stall" else "", "encounters_won": won, "rounds": 4 * fights,
                             "deaths": sum(1 for _, alive, _ in heroes if not alive), "damage": 100})
            for stage in range(1, fights + 1):
                last = stage == fights
                encounter_rows.append({"party": party, "seed": seed, "stage": stage, "route": "guarded" if stage % 2 else "dangerous",
                                       "outcome": outcome if last and outcome != "Victory" else "Victory", "rounds": 4 + (20 if outcome == "Stall" and last else 0),
                                       "hp_start": 80, "hp_low": 30, "hp_end": 40, "hp_max": 100, "conscious_start": 4})
            for slot, (kit, alive, damage) in enumerate(heroes, 1):
                hero_rows.append({"party": party, "archetype": "rotation", "seed": seed, "outcome": outcome, "slot": slot, "build": kit,
                                  "class": kit.split(".")[0], "survived": 1 if alive else 0, "death_stage": 0 if alive else fights,
                                  "death_cause": "" if alive else "wipe", "turns": 10, "damage": damage, "s1_command": "Special1",
                                  "s1_available_turns": 5, "s1_uses": 1 if kit != "monk.idle" else 0, "s1_blockers": "ResourceShort:5",
                                  "s2_command": "", "kit_left": 1, "kit_cap": 2, "slot_cap": 0, "wind_cap": 0,
                                  "maneuver_cap": 0, "rage_cap": 0, "potions_left": 2})
        write(folder / "runs.csv", RUN_FIELDS, run_rows)
        write(folder / "encounters.csv", ENCOUNTER_FIELDS, encounter_rows)
        write(folder / "heroes.csv", HERO_FIELDS, hero_rows)
        (folder / "group.json").write_text(json.dumps({"group": name, "policy_version": 1, "seconds": 1.5}), encoding="utf-8")

    def test_wilson_interval(self):
        low, high = balance_report.wilson(8, 10)
        self.assertAlmostEqual(low, 0.4902, places=3)
        self.assertAlmostEqual(high, 0.9433, places=3)
        self.assertEqual(balance_report.wilson(0, 0), (0.0, 1.0))
        self.assertEqual(balance_report.wilson(0, 5)[0], 0.0)

    def test_robust_z(self):
        z = balance_report.robust_z([1, 2, 3, 4, 100])
        self.assertAlmostEqual(z[2], 0.0)
        self.assertGreater(z[4], 30)
        self.assertEqual(balance_report.robust_z([5, 5, 5]), [0.0, 0.0, 0.0])
        self.assertGreater(balance_report.robust_z([5, 5, 5, 5, 9])[4], 3)  # MAD of zero falls back to the mean deviation

    def test_merges_groups_and_rates(self):
        heroes = [("fighter.champion", True, 40), ("monk.idle", True, 10), ("cleric.life", True, 5), ("wizard.evocation", True, 30)]
        self.group("A", [("p1", 1, "Victory", 5, heroes), ("p1", 2, "Defeat", 3, [(k, False, d) for k, _, d in heroes])])
        self.group("B", [("p2", 1, "Stall", 2, heroes)])
        summary = balance_report.summarize(balance_report.load(self.root))
        self.assertEqual(summary["groups"], ["A", "B"])
        self.assertEqual(summary["overall"]["n"], 3)
        self.assertEqual(summary["overall"]["wins"], 1)
        self.assertEqual(summary["overall"]["stalls"], 1)
        self.assertEqual(summary["seconds"], 3.0)
        fighter = summary["kits"]["fighter.champion"]
        self.assertEqual(fighter["n"], 3)
        # 40 damage in each of 5 + 3 + 2 fights present.
        self.assertAlmostEqual(fighter["damage_per_fight"], 120 / 10)
        self.assertAlmostEqual(fighter["death_rate"], 1 / 3)
        self.assertEqual(fighter["left_at_victory"], {"kit": {"mean": 1.0, "cap": 2}})
        self.assertEqual(summary["stalls"], [{"party": "p2", "seed": "1", "reason": "fight 2 reached round 60"}])
        self.assertIn("1-guarded", summary["encounters"])
        self.assertAlmostEqual(summary["encounters"]["1-guarded"]["hp_lost_fraction"], 0.4)
        self.assertAlmostEqual(summary["encounters"]["1-guarded"]["hp_low_min"], 0.3)
        # p1 seeds 1 and 2 play 20 + 12 rounds over 8 fights; p2 plays 8 rounds over 2.
        self.assertAlmostEqual(fighter["party_rounds_per_fight"], 40 / 10)
        flags = {(s["kit"], s["flag"]) for s in summary["signatures"]}
        self.assertIn(("monk.idle", "never used"), flags)
        self.assertEqual(summary["long_fights"]["p99"], 24)
        self.assertEqual(summary["deaths"]["count"], 4)
        self.assertEqual(summary["deaths"]["causes"], {"wipe": 4})
        self.assertEqual(summary["deaths"]["list"][0]["seed"], "2")

    def test_writes_summary_files(self):
        self.group("A", [("p1", 1, "Victory", 5, [("fighter.champion", True, 40)] * 4)])
        out = io.StringIO()
        with redirect_stdout(out):
            self.assertEqual(balance_report.main([str(self.root)], local_root=self.local), 0)
        self.assertIn("1 runs", out.getvalue())
        self.assertIn("# Balance baseline summary", (self.root / "summary.md").read_text(encoding="utf-8"))
        self.assertEqual(json.loads((self.root / "summary.json").read_text(encoding="utf-8"))["overall"]["wins"], 1)

    def test_blockers_sum_across_runs_before_ranking(self):
        # Summing only each row's top would give ResourceShort 20 against OutOfRange 6; the full counts give 20 against 22.
        heroes = [("cleric.war", True, 10)] * 4
        self.group("A", [("p1", seed, "Victory", 5, heroes) for seed in (1, 2)])
        with (self.root / "A" / "heroes.csv").open(encoding="utf-8") as stream:
            rows = list(csv.DictReader(stream))
        for row in rows:
            row["s1_blockers"] = {"1": "ResourceShort:5;OutOfRange:4", "2": "ResourceShort:5;OutOfRange:4", "3": "OutOfRange:3"}.get(row["slot"], "")
            row["s1_uses"] = "0"
        write(self.root / "A" / "heroes.csv", HERO_FIELDS, rows)
        special = balance_report.summarize(balance_report.load(self.root))["kits"]["cleric.war"]["specials"][0]
        self.assertEqual(special["blockers"], {"OutOfRange": 22, "ResourceShort": 20})
        self.assertEqual(special["top_blocker"], "OutOfRange")
        self.assertEqual(balance_report.parse_blockers(""), {})
        with self.assertRaises(ValueError):
            balance_report.parse_blockers("ResourceShort")

    def test_refuses_folders_outside_local(self):
        self.group("A", [("p1", 1, "Victory", 5, [("fighter.champion", True, 40)] * 4)])
        outside = Path(self.temp.name) / "elsewhere"
        outside.mkdir()
        for folder in (str(outside), str(self.local), str(self.local / ".." / "elsewhere"), "../escape", "Saved/balance"):
            err = io.StringIO()
            with redirect_stderr(err):
                self.assertEqual(balance_report.main([folder], local_root=self.local), 2, folder)
            self.assertIn("must be inside", err.getvalue())
        self.assertFalse((outside / "summary.md").exists())
        # A relative folder is taken from the checkout root (the parent of .local/).
        self.assertEqual(balance_report.resolve_folder(".local/balance/run", self.local), self.root.resolve())
        with redirect_stdout(io.StringIO()):
            self.assertEqual(balance_report.main([".local/balance/run"], local_root=self.local), 0)

    @unittest.skipUnless(hasattr(os, "symlink"), "needs symlinks")
    def test_refuses_directory_links_out_of_local(self):
        self.group("A", [("p1", 1, "Victory", 5, [("fighter.champion", True, 40)] * 4)])
        outside = Path(self.temp.name) / "outside"
        shutil.copytree(self.root, outside / "run")
        def directory_link(link, target):
            if os.name == "nt":
                # Directory junctions exercise the same escape without requiring symlink privileges.
                script = Path(self.temp.name) / "junction.ps1"
                script.write_text("param([string]$Link, [string]$Target)\n$ErrorActionPreference='Stop'\nNew-Item -ItemType Junction -Path $Link -Target $Target | Out-Null\n", encoding="utf-8")
                subprocess.run(["powershell.exe", "-NoProfile", "-File", str(script), str(link), str(target)], check=True, capture_output=True)
            else:
                link.symlink_to(target, target_is_directory=True)

        missing = Path(self.temp.name) / "missing"
        missing.mkdir()
        directory_link(self.local / "link-out", outside)
        directory_link(self.local / "link-dangling", missing)
        missing.rmdir()
        directory_link(self.local / "link-in", self.local / "balance")
        for folder in (".local/link-out/run", ".local/link-dangling/run", str(self.local / "link-out" / "run")):
            err = io.StringIO()
            with redirect_stderr(err):
                self.assertEqual(balance_report.main([folder], local_root=self.local), 2, folder)
            self.assertIn("must be inside", err.getvalue())
        self.assertFalse((outside / "run" / "summary.md").exists())
        with redirect_stdout(io.StringIO()):
            self.assertEqual(balance_report.main([".local/link-in/run"], local_root=self.local), 0)
        self.assertTrue((self.root / "summary.md").exists())

    def test_refuses_empty_or_incomplete_folder(self):
        err = io.StringIO()
        with redirect_stderr(err):
            self.assertEqual(balance_report.main([str(self.root)], local_root=self.local), 2)
        self.assertIn("No balance groups", err.getvalue())
        (self.root / "X").mkdir()
        (self.root / "X" / "group.json").write_text("{}", encoding="utf-8")
        with redirect_stderr(err):
            self.assertEqual(balance_report.main([str(self.root)], local_root=self.local), 2)
        self.assertIn("missing runs.csv", err.getvalue())


if __name__ == "__main__":
    unittest.main()
