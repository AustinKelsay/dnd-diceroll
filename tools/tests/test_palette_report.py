import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import palette_report  # noqa: E402


class PaletteReportTests(unittest.TestCase):
    def test_gate_uses_mean_worst_and_each_board(self):
        data = [("fighter.champion", board, c, 0.1) for board in ("vigil", "root", "vault") for c in (2.6, 2.8, 3.0)]
        rows = palette_report.aggregate(data)
        self.assertTrue(rows["fighter.champion"]["pass"])
        # One shot under 1.4 fails the worst-shot gate even when the mean is fine.
        data.append(("rogue.thief", "vault", 1.38, 0.2))
        data += [("rogue.thief", board, 3.0, 0.1) for board in ("vigil", "root", "vault") for _ in range(5)]
        row = palette_report.aggregate(data)["rogue.thief"]
        self.assertFalse(row["pass"])
        self.assertTrue(any("worst" in reason for reason in row["failures"]), row["failures"])
        # A board mean under 2.0 fails even with a good overall mean.
        data = [("bard.lore", "vigil", 1.9, 0.1)] * 3 + [("bard.lore", "root", 4.0, 0.1)] * 12 + [("bard.lore", "vault", 4.0, 0.1)] * 5
        row = palette_report.aggregate(data)["bard.lore"]
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
        default = [n for n in names if "-default" in n]
        self.assertEqual(len(default), 5)       # four yaws and the targeting shot: the shots the gate counts
        roles = {shot["role"] for shot in poses["shots"] if shot["mode"] == "pose"}
        self.assertEqual(len(roles), 13)


if __name__ == "__main__":
    unittest.main()
