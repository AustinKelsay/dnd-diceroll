import csv
import io
import json
import math
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
                    "damage_taken,healing,protection,downs,deaths,revives,specials_used,melee_damage").split(",")
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

    def test_layouts_stage_kinds_and_risk_words(self):
        """#188: per-layout, per-stage and risk-word tables, with the melee share of damage and the risk ordering check."""
        folder = self.root / "G"
        folder.mkdir()
        rows = []
        # (stage, route, recipe, outcome, hp lost of 100, damage, melee damage, rounds)
        fights = [(1, "guarded", 0, "Victory", 10, 50, 10, 4), (2, "guarded", 1, "Victory", 12, 50, 40, 6),
                  (3, "guarded", 3, "Victory", 20, 60, 30, 5), (1, "dangerous", 2, "Victory", 30, 80, 20, 5),
                  (5, "guarded", 6, "Defeat", 40, 70, 35, 9), (3, "dangerous", 4, "Defeat", 60, 90, 45, 8), (5, "dangerous", 6, "Stall", 99, 1, 1, 60)]
        for i, (stage, route, recipe, outcome, lost, damage, melee, rounds) in enumerate(fights):
            rows.append({"party": "p", "seed": i, "stage": stage, "route": route, "recipe": recipe, "outcome": outcome, "rounds": rounds,
                         "hp_start": 100, "hp_end": 100 - lost, "hp_max": 100, "damage": damage, "melee_damage": melee, "damage_taken": lost + 10})
        write(folder / "runs.csv", RUN_FIELDS, [{"party": "p", "archetype": "rotation", "builds": "fighter.champion", "seed": 0, "outcome": "Victory"}])
        write(folder / "encounters.csv", ENCOUNTER_FIELDS, rows)
        write(folder / "heroes.csv", HERO_FIELDS, [])
        (folder / "group.json").write_text(json.dumps({"group": "G", "policy_version": 1, "seconds": 1, "catalog_revision": 9}), encoding="utf-8")
        summary = balance_report.summarize(balance_report.load(self.root))
        self.assertEqual(summary["catalog_revisions"], [9])
        colonnade = summary["layouts"]["3-guarded"]
        self.assertEqual((colonnade["layout"], colonnade["n"], colonnade["stages"]), ("fallen colonnade", 1, [3]))
        self.assertAlmostEqual(colonnade["melee_share"], 0.5)
        self.assertAlmostEqual(summary["layouts"]["1-guarded"]["melee_share"], 0.8)
        # The stall is not a finished fight.
        self.assertEqual(summary["layouts"]["6-dangerous"]["n"], 0)
        self.assertAlmostEqual(summary["stage_kinds"]["approach-guarded"]["hp_lost_fraction"], 0.11)
        self.assertAlmostEqual(summary["stage_kinds"]["approach-guarded"]["rounds"], 5)
        self.assertEqual(summary["stage_kinds"]["finale-guarded"]["rate"], 1.0)
        tiers = summary["risk"]["tiers"]
        self.assertEqual(list(tiers), ["low", "moderate", "high", "severe"])
        # high: approach dangerous (30, won) and finale guarded (40, lost).
        self.assertEqual((tiers["high"]["n"], tiers["high"]["wins"]), (2, 1))
        self.assertAlmostEqual(tiers["high"]["hp_lost_fraction"], 0.35)
        # moderate (pressure guarded) never lost, as low never did: that pair is not ordered.
        self.assertFalse(summary["risk"]["ordered"])
        failing = [(p["lower"], p["higher"]) for p in summary["risk"]["pairs"] if not p["loses_more"]]
        self.assertEqual(failing, [("low", "moderate")])
        text = balance_report.markdown(summary)
        self.assertIn("| 3 | fallen colonnade | guarded | 1 (3) | 0.0% | 5.0 | 20% | 30% | 50% |", text)
        self.assertIn("Every higher word costs more HP (net) and loses more often: no, moderate vs low loses no more often.", text)
        self.assertIn("Every higher word takes more HP (gross) and loses more often: no, moderate vs low loses no more often.", text)
        self.assertAlmostEqual(colonnade["hp_taken_fraction"], 0.3)
        self.assertTrue(all(p["takes_more"] for p in summary["risk"]["pairs"]))
        self.assertIn("Catalog revision 9.", text)

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


class ProgressionStatisticsTests(unittest.TestCase):
    """#218, contract 13 section 6: compare, the paired seed bootstrap, verdicts and the holds."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.local = Path(self.temp.name) / ".local"
        self.local.mkdir()

    def measurement(self, name, appended=True, cards="G", edit=None, strategy="gear", revision=9):
        """Two parties on two seeds, one group; appended adds the #218 hero columns."""
        folder = self.local / name / "Fixed"
        folder.mkdir(parents=True)
        heroes = HERO_FIELDS + (["reward_cards", "reserve", "honed"] if appended else [])
        runs, encounters, hero_rows = [], [], []
        for party in ("p1", "p2"):
            for seed in ("1", "2"):
                runs.append({"party": party, "archetype": "rotation", "builds": "a.x", "seed": seed, "outcome": "Victory", "stall_reason": "", "encounters_won": 1})
                encounters.append({"party": party, "seed": seed, "stage": 1, "route": "guarded", "outcome": "Victory", "hp_start": 10, "hp_end": 5, "hp_max": 10})
                row = {"party": party, "archetype": "rotation", "seed": seed, "outcome": "Victory", "slot": 1, "build": "a.x", "class": "a", "s1_blockers": "", "s2_blockers": "", "kit_cap": 2}
                if appended:
                    row.update({"reward_cards": cards, "reserve": 1 if "R" in cards else 0, "honed": 1 if "H" in cards else 0, "kit_cap": 3 if "R" in cards else 2})
                hero_rows.append(row)
        if edit:
            edit(runs, encounters, hero_rows)
        write(folder / "runs.csv", RUN_FIELDS, runs)
        write(folder / "encounters.csv", ENCOUNTER_FIELDS, encounters)
        write(folder / "heroes.csv", heroes, hero_rows)
        meta = {"group": "Fixed", "policy_version": 2 if appended else 1, "seconds": 1.0, "catalog_revision": revision, "seeds": 2, "runs": 4, "parties": ["p1", "p2"]}
        if appended:
            meta["reward_strategy"] = strategy
        (folder / "group.json").write_text(json.dumps(meta), encoding="utf-8")
        return self.local / name

    def test_compare_c1_match(self):
        self.assertEqual(balance_report.compare(self.measurement("old", appended=False), self.measurement("new"), "c1"), [])

    def test_compare_c1_mismatch_missing_and_extra_rows(self):
        base = self.measurement("old", appended=False)
        changed = self.measurement("changed", edit=lambda r, e, h: r[0].update({"rounds": 99}))
        self.assertTrue(any("rounds" in p for p in balance_report.compare(base, changed, "c1")))
        missing = self.measurement("missing", edit=lambda r, e, h: e.pop())
        self.assertTrue(any("missing" in p for p in balance_report.compare(base, missing, "c1")))
        extra = self.measurement("extra", edit=lambda r, e, h: e.append(dict(e[0], stage=2)))
        self.assertTrue(any("extra row" in p for p in balance_report.compare(base, extra, "c1")))
        upgraded = self.measurement("upgraded", cards="R")
        self.assertTrue(any("not gear-only" in p for p in balance_report.compare(base, upgraded, "c1")))
        unclaimed = self.measurement("unclaimed", cards="")
        self.assertTrue(any("claims for" in p for p in balance_report.compare(base, unclaimed, "c1")))

    def test_compare_c2_every_column_and_group_json(self):
        before = self.measurement("before")
        self.assertEqual(balance_report.compare(before, self.measurement("gear", revision=10), "c2"), [])
        self.assertTrue(any("reward_cards" in p for p in balance_report.compare(before, self.measurement("cards", revision=10, cards="G;G"), "c2")))
        self.assertTrue(any("reward_strategy" in p for p in balance_report.compare(before, self.measurement("mixed", revision=10, strategy="mixed"), "c2")))
        self.assertTrue(any("catalog revisions" in p for p in balance_report.compare(before, self.measurement("same", revision=9), "c2")))
        self.assertTrue(any("headers differ" in p for p in balance_report.compare(before, self.measurement("old", appended=False, revision=10), "c2")))

    def test_check_cards(self):
        gear = self.measurement("gear")
        self.assertEqual(balance_report.check_cards(self.measurement("reserve", cards="R"), gear), [])
        bad = self.measurement("bad", edit=lambda r, e, h: h[0].update({"reward_cards": "R;R", "reserve": 1, "kit_cap": 3}))
        self.assertTrue(any("cards" in p for p in balance_report.check_cards(bad, gear)))
        caps = self.measurement("caps", cards="R", edit=lambda r, e, h: h[0].update({"kit_cap": 2}))
        self.assertTrue(any("kit_cap" in p for p in balance_report.check_cards(caps, gear)))

    def test_bootstrap_is_fixed_by_its_seed(self):
        a = {("p", str(s)): 1 if s % 3 else 0 for s in range(32)}
        b = {("p", str(s)): 1 if s % 4 else 0 for s in range(32)}
        draws = balance_report.resamples([str(s) for s in range(32)], count=500)
        first = balance_report.paired_difference(a, b, list(a), draws)
        second = balance_report.paired_difference(a, b, list(a), balance_report.resamples([str(s) for s in range(32)], count=500))
        self.assertEqual(first, second)
        self.assertLess(first["low"], first["d"])
        self.assertGreater(first["high"], first["d"])
        self.assertNotEqual(balance_report.resamples([str(s) for s in range(32)], count=5, seed=1), balance_report.resamples([str(s) for s in range(32)], count=5))

    def test_verdicts(self):
        self.assertEqual(balance_report.verdict({"d": 7.0, "low": 2.0, "high": 12.0}), "clear")
        self.assertEqual(balance_report.verdict({"d": -6.0, "low": -9.0, "high": -1.0}), "clear")
        self.assertEqual(balance_report.verdict({"d": 3.0, "low": 1.0, "high": 5.0}), "small")
        self.assertEqual(balance_report.verdict({"d": 4.0, "low": -1.0, "high": 9.0}), "tie")
        self.assertEqual(balance_report.threshold((2.0, 7.0), 8.0, above_is_met=False), "met")
        self.assertEqual(balance_report.threshold((6.0, 9.0), 8.0, above_is_met=False), "inconclusive")
        self.assertEqual(balance_report.threshold((9.0, 12.0), 8.0, above_is_met=False), "missed")
        self.assertEqual(balance_report.threshold((1.6, 2.0), 1.5, above_is_met=True), "met")
        self.assertEqual(balance_report.threshold((1.2, 1.4), 1.5, above_is_met=True), "missed")

    def test_best_for_class_with_ties(self):
        # Each strategy best (clear) for two classes: met. A tie that could go either way: possible but not best.
        self.assertTrue(balance_report.best_assignment({"a": ["gear"], "b": ["gear"], "c": ["reserve"], "d": ["reserve"], "e": ["honed"], "f": ["honed"]}))
        self.assertFalse(balance_report.best_assignment({"a": ["gear"], "b": ["gear"], "c": ["reserve"], "d": ["reserve"], "e": ["honed"], "f": []}))
        # One tied class cannot count for two strategies.
        self.assertFalse(balance_report.best_assignment({"a": ["gear"], "b": ["gear"], "c": ["reserve"], "d": ["honed"], "e": ["reserve", "honed"]}))
        self.assertTrue(balance_report.best_assignment({"a": ["gear"], "b": ["gear"], "c": ["reserve"], "d": ["honed"], "e": ["reserve", "honed"], "f": ["honed", "reserve"]}))

    def test_holds_met_missed_borderline(self):
        self.assertEqual(balance_report.hold(0.60, 0.80, 0.55, 0.90), "met")
        self.assertEqual(balance_report.hold(0.40, 0.50, 0.55, 0.90), "missed")
        self.assertEqual(balance_report.hold(0.50, 0.60, 0.55, 0.90), "borderline")
        self.assertEqual(balance_report.hold(0.85, 0.95, 0.55, 0.90), "borderline")
        self.assertEqual(balance_report.hold(0.36, 0.60, 0.35, None), "met")
        self.assertEqual(balance_report.hold(0.20, 0.30, 0.35, None), "missed")
        self.assertEqual(balance_report.hold(0.30, 0.40, 0.35, None), "borderline")

    def ratio(self, d, g, n=8):
        """Every seed with the same Dangerous and Guarded means d and g."""
        seeds = [str(i) for i in range(n)]
        draws = balance_report.resamples(seeds, count=200)
        return balance_report.ratio_interval({s: (d, 1) for s in seeds}, {s: (g, 1) for s in seeds}, draws, 1.5)

    def test_ratio_sign_rule_fixtures(self):
        self.assertEqual(self.ratio(0.2, 0.0)["low"], math.inf)          # G = 0, D > 0: above
        self.assertEqual(self.ratio(0.0, 0.0)["low"], -math.inf)         # G = 0, D = 0: below
        self.assertEqual(self.ratio(-0.2, -0.1)["low"], -math.inf)       # both negative: below, never 2.0
        self.assertEqual(self.ratio(-0.2, 0.1)["low"], -math.inf)        # G > 0, D < 0: below
        self.assertEqual(self.ratio(0.2, -0.1)["low"], math.inf)         # G < 0, D > 0: above
        self.assertEqual(self.ratio(-0.2, 0.0)["sign_decided"], 200)
        self.assertAlmostEqual(self.ratio(0.3, 0.1)["low"], 3.0)
        self.assertEqual(self.ratio(0.3, 0.1)["sign_decided"], 0)

    def test_zero_denominator_resample(self):
        # Only seed 0 has Guarded HP lost; resamples without it are decided by the sign rule (D > 0, G = 0: above).
        seeds = [str(i) for i in range(6)]
        draws = balance_report.resamples(seeds, count=400)
        den = {s: (0.1 if s == "0" else 0.0, 1) for s in seeds}
        r = balance_report.ratio_interval({s: (0.2, 1) for s in seeds}, den, draws, 1.5)
        self.assertGreater(r["sign_decided"], 0)
        self.assertEqual(r["high"], math.inf)

    def per_fight_data(self, guarded, dangerous, seeds=8):
        """Encounter rows with net HP lost fractions per fight and route; one fight per (party, seed, stage)."""
        runs, encounters = [], []
        for i in range(seeds):
            seed = str(i)
            runs.append({"party": "p", "archetype": "rotation", "seed": seed, "outcome": "Victory"})
            for stage in range(1, 6):
                route = "guarded" if i % 2 else "dangerous"
                lost = (guarded if route == "guarded" else dangerous)[stage - 1]
                encounters.append({"party": "p", "seed": seed, "stage": str(stage), "route": route, "outcome": "Defeat" if stage == 5 and i < 2 else "Victory",
                                   "hp_start": "100", "hp_end": str(100 - lost), "hp_max": "100"})
        return {"runs": runs, "encounters": encounters, "heroes": []}

    def test_dangerous_hold_is_per_fight(self):
        draws = balance_report.resamples([str(i) for i in range(8)], count=300)
        met = balance_report.slice_holds(self.per_fight_data([10] * 5, [30] * 5), draws)["dangerous_1_5x"]
        self.assertEqual(met["verdict"], "met")
        # Pooled over fights the ratio passes (130 / 50 = 2.6x), but fight 3 alone is 1.0x: missed.
        one_fails = balance_report.slice_holds(self.per_fight_data([10, 10, 10, 10, 10], [30, 30, 10, 30, 30]), draws)["dangerous_1_5x"]
        self.assertEqual(one_fails["fights"]["3"]["verdict"], "missed")
        self.assertEqual(one_fails["verdict"], "missed")

    def test_borderline_dangerous_and_finale_holds(self):
        draws = balance_report.resamples([str(i) for i in range(8)], count=300)
        data = self.per_fight_data([10] * 5, [15] * 5)
        # Every seed alternates routes, so a resample can draw only one route's seeds: the ratio straddles 1.5 when the point sits on it.
        holds = balance_report.slice_holds(data, draws)
        self.assertIn(holds["dangerous_1_5x"]["fights"]["1"]["verdict"], ("borderline", "met"))
        self.assertIn(holds["finale_highest"]["verdict"], ("met", "borderline"))
