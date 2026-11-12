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


if __name__ == "__main__":
    unittest.main()
