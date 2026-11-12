import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import accent_share  # noqa: E402

SHOTS = accent_share.expected_shots()


def manifest(cls="fighter", shots=SHOTS, heroes=4, skip=()):
    return {"fixture": f"acc-{cls}-root", "shots": [
        {"name": name, "status": "skipped" if name in skip else "captured", "units": [{"role": cls.capitalize(), "name": f"Hero {i}", "box": {}} for i in range(heroes)]} for name in shots]}


class AccentShareTests(unittest.TestCase):
    def test_all_thirteen_classes_are_expected_with_five_shots(self):
        self.assertEqual(len(accent_share.expected_classes()), 13)
        self.assertEqual(len(SHOTS), 5)

    def test_a_complete_pair_has_no_problems(self):
        self.assertEqual(accent_share.problems("fighter", manifest(), manifest(), SHOTS), [])

    def test_a_missing_run_is_a_problem(self):
        found = accent_share.problems("rogue", None, manifest("rogue"), SHOTS)
        self.assertEqual(len(found), 1)
        self.assertIn("no with accents run", found[0])
        self.assertEqual(len(accent_share.problems("rogue", None, None, SHOTS)), 2)

    def test_a_skipped_or_absent_shot_is_a_problem_in_either_run(self):
        found = accent_share.problems("cleric", manifest("cleric", skip={SHOTS[1]}), manifest("cleric", shots=SHOTS[:3]), SHOTS)
        self.assertTrue(any("with accents" in line and SHOTS[1] in line for line in found), found)
        self.assertEqual(sum("without accents" in line for line in found), len(SHOTS) - 3)

    def test_a_shot_without_all_four_heroes_is_a_problem(self):
        found = accent_share.problems("bard", manifest("bard", heroes=3), manifest("bard"), SHOTS)
        self.assertEqual(len(found), len(SHOTS))
        self.assertTrue(all("expected 4" in line for line in found))

    def test_the_limits_apply_to_the_mean_and_to_the_largest_single_view(self):
        shares = [0.05, 0.06, 0.24]                       # mean 0.117
        self.assertTrue(accent_share.over_limit(shares, 0.10))
        self.assertTrue(accent_share.over_limit(shares, 0.15))        # the single view 0.24 is over the default 1.5 x 0.15 = 0.225
        self.assertFalse(accent_share.over_limit(shares, 0.15, 0.25))
        self.assertTrue(accent_share.over_limit(shares, 0.15, 0.20))
        self.assertFalse(accent_share.over_limit([0.05, 0.06, 0.08], 0.10))

    def test_delta_e_is_zero_for_equal_colours_and_large_for_opposite_hues(self):
        self.assertEqual(accent_share.delta_e((120, 80, 40), (120, 80, 40)), 0.0)
        self.assertGreater(accent_share.delta_e((200, 30, 30), (30, 30, 200)), 80)
        # white against black is the full lightness range, 100
        self.assertAlmostEqual(accent_share.delta_e((255, 255, 255), (0, 0, 0)), 100.0, delta=0.5)

    def test_separation_is_the_mean_and_nearest_pair_distance_of_the_copies_with_a_patch(self):
        red, green, blue = (200, 40, 40), (40, 200, 40), (40, 40, 200)
        mean, nearest = accent_share.separation([red, green, blue, None])       # a copy with no patch has no colour to compare
        pairs = [accent_share.delta_e(a, b) for a, b in ((red, green), (red, blue), (green, blue))]
        self.assertAlmostEqual(mean, sum(pairs) / 3)
        self.assertAlmostEqual(nearest, min(pairs))
        self.assertIsNone(accent_share.separation([red, None, None, None]))
        self.assertIsNone(accent_share.separation([None] * 4))

    def test_four_copies_with_the_same_patch_colour_are_not_apart(self):
        mean, nearest = accent_share.separation([(90, 90, 200)] * 4)
        self.assertEqual((mean, nearest), (0.0, 0.0))

    def test_the_separation_summary_counts_each_shot_once(self):
        rows = [dict(shot="a", changed=40, separation=30.0, nearest=20.0) for _ in range(4)] + [dict(shot="b", changed=20, separation=50.0, nearest=10.0) for _ in range(4)]
        patch, apart, nearest = accent_share.separation_summary(rows)
        self.assertEqual((patch, apart, nearest), (30.0, 40.0, 10.0))


if __name__ == "__main__":
    unittest.main()
