import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import palette_report  # noqa: E402

SHOTS = palette_report.expected_shots()
BOARDS = palette_report.BOARDS


def full(build, contrast=3.0, merge=0.1, skip=()):
    """Every expected board x shot of a build, minus the (board, shot) pairs in skip."""
    return [(build, board, shot, contrast, merge) for board in BOARDS for shot in SHOTS if (board, shot) not in skip]


class PaletteReportTests(unittest.TestCase):
    def test_the_expected_shots_are_the_five_default_zoom_ones(self):
        self.assertEqual(len(SHOTS), 5)
        self.assertTrue(all("-default" in name for name in SHOTS))
        self.assertIn("targeting-yawm90-default", SHOTS)

    def test_a_complete_build_passes(self):
        rows = palette_report.aggregate(full("fighter.champion"), ["fighter.champion"])
        self.assertTrue(rows["fighter.champion"]["pass"], rows["fighter.champion"]["failures"])
        self.assertEqual(rows["fighter.champion"]["missing"], [])

    def test_missing_measurements_fail_even_when_the_ones_that_exist_are_good(self):
        # One Root measurement of bard.valor used to pass (mean, worst and the board means of the single shot were fine).
        rows = palette_report.aggregate([("bard.valor", "root", SHOTS[0], 3.0, 0.1)], ["bard.valor"])
        row = rows["bard.valor"]
        self.assertFalse(row["pass"])
        self.assertEqual(len(row["missing"]), len(BOARDS) * len(SHOTS) - 1)
        self.assertTrue(any("missing" in reason for reason in row["failures"]))
        # One skipped shot on one board is enough.
        row = palette_report.aggregate(full("rogue.thief", skip={("vault", SHOTS[2])}), ["rogue.thief"])["rogue.thief"]
        self.assertFalse(row["pass"])
        self.assertEqual(row["missing"], [f"vault/{SHOTS[2]}"])

    def test_an_expected_build_with_no_measurement_gets_a_failing_row(self):
        rows = palette_report.aggregate(full("cleric.life"), ["cleric.life", "cleric.war"])
        self.assertTrue(rows["cleric.life"]["pass"])
        self.assertFalse(rows["cleric.war"]["pass"])
        self.assertIsNone(rows["cleric.war"]["mean"])
        self.assertIn("FAIL", palette_report.markdown({"cleric.war": rows["cleric.war"]}))

    def test_a_shot_measured_twice_is_reported(self):
        rows = palette_report.aggregate(full("monk.mercy") + [("monk.mercy", "root", SHOTS[0], 3.0, 0.1)], ["monk.mercy"])
        self.assertFalse(rows["monk.mercy"]["pass"])
        self.assertTrue(any("more than once" in reason for reason in rows["monk.mercy"]["failures"]))

    def test_gate_uses_mean_worst_and_each_board(self):
        # One shot under 1.4 fails the worst-shot gate even when the mean is fine.
        data = full("rogue.thief")
        data[0] = ("rogue.thief", data[0][1], data[0][2], 1.38, 0.2)
        row = palette_report.aggregate(data, ["rogue.thief"])["rogue.thief"]
        self.assertFalse(row["pass"])
        self.assertTrue(any("worst" in reason for reason in row["failures"]), row["failures"])
        # A board mean under 2.0 fails even with a good overall mean.
        data = [("bard.lore", board, shot, 1.9 if board == "vigil" else 4.0, 0.1) for board in BOARDS for shot in SHOTS]
        row = palette_report.aggregate(data, ["bard.lore"])["bard.lore"]
        self.assertFalse(row["pass"])
        self.assertTrue(any("vigil" in reason for reason in row["failures"]))
        self.assertIn("FAIL", palette_report.markdown({"bard.lore": row}))

    def test_a_filler_is_not_measured_in_the_party_it_fills(self):
        parties = {"parties": [{"id": "pal-17", "builds": ["bard.eloquence", "fighter.champion", "rogue.thief", "cleric.life"], "filler": ["fighter.champion", "rogue.thief"]}]}
        self.assertEqual(palette_report.real_members(parties), {"pal-17": {"Bard": "bard.eloquence", "Cleric": "cleric.life"}})
        self.assertEqual(palette_report.role_of("artificer.alchemist"), "Artificer")

    def test_the_palette_poses_file_has_the_default_zoom_shots_and_every_class(self):
        poses = json.loads((ROOT / "tools" / "capture" / "palette-poses.json").read_text())
        names = [shot["name"] for shot in poses["shots"]]
        self.assertEqual(len(names), len(set(names)))
        roles = {shot["role"] for shot in poses["shots"] if shot["mode"] == "pose"}
        self.assertEqual(len(roles), 13)


if __name__ == "__main__":
    unittest.main()
