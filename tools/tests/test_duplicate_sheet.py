import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import duplicate_sheet  # noqa: E402

SHOT = duplicate_sheet.SHOT
BOX = {"x0": 100, "y0": 200, "x1": 160, "y1": 300}


def unit(role="Rogue", side="Party", index=0):
    return {"role": role, "side": side, "name": f"Hero {index}", "box": dict(BOX, x0=100 + 80 * index, x1=160 + 80 * index)}


def write_run(root, fixture, role="Rogue", heroes=4, shot=SHOT, status="captured", foes=1):
    """One gallery run folder with the manifest the game writes for a fixture: `heroes` party units and `foes` enemies in the shot."""
    folder = Path(root) / fixture / "1920x1080"
    folder.mkdir(parents=True)
    units = [unit(role, index=i) for i in range(heroes)] + [unit("Foe", "Enemy", 9 + i) for i in range(foes)]
    (folder / "manifest.json").write_text(json.dumps({"fixture": fixture, "shots": [{"name": shot, "status": status, "file": f"{shot}.png", "units": units}]}))
    return folder


def run_main(runs, *extra, out):
    """duplicate_sheet.main with ImageMagick mocked: (exit code, stderr, magick calls)."""
    err, calls = io.StringIO(), []
    with mock.patch.object(duplicate_sheet.shutil, "which", return_value="/usr/bin/magick"), \
            mock.patch.object(duplicate_sheet.subprocess, "run", side_effect=lambda command, check: calls.append(command)), \
            contextlib.redirect_stderr(err), contextlib.redirect_stdout(io.StringIO()):
        code = duplicate_sheet.main([*[str(r) for r in runs], "--out", str(out), *extra])
    return code, err.getvalue(), calls


class DuplicateSheetTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name) / "run"
        self.out = Path(self.tmp.name) / "sheet.png"

    def complete(self, party="rogues", role="Rogue", **kwargs):
        for board in duplicate_sheet.BOARDS:
            write_run(self.root, f"dup-four-{party}-{board}", role, **kwargs)

    def test_a_complete_request_renders_every_tile_row_and_the_sheet(self):
        self.complete("rogues", "Rogue")
        self.complete("paladins", "Paladin")
        code, err, calls = run_main([self.root], "--classes", "dup-four-rogues,dup-four-paladins", out=self.out)
        self.assertEqual((code, err), (0, ""))
        crops = [c for c in calls if "-crop" in c]
        self.assertEqual(len(crops), 6)                                        # 2 parties x 3 boards
        self.assertEqual(len([c for c in calls if "+append" in c]), 2)         # one strip per party
        self.assertEqual(calls[-1][-1], str(self.out))                          # the final -append writes the sheet
        # the crop is the union of the four heroes' boxes (the foe is not part of the party), padded
        geometry = crops[0][crops[0].index("-crop") + 1]
        self.assertEqual(geometry, "450x160+25+170")                            # x 100..400 and y 200..300 plus 75 and 30 px of margin

    def test_the_default_is_all_four_parties(self):
        for party, role in (("fighters", "Fighter"), ("paladins", "Paladin"), ("rogues", "Rogue")):
            self.complete(party, role)
        for board in duplicate_sheet.BOARDS:
            write_run(self.root, f"dup-two-pairs-{board}", "Cleric")
        code, err, calls = run_main([self.root], out=self.out)
        self.assertEqual(code, 0, err)
        self.assertEqual(len([c for c in calls if "-crop" in c]), 12)

    def test_one_rogue_capture_with_rogues_and_paladins_requested_fails_and_renders_nothing(self):
        write_run(self.root, "dup-four-rogues-root", "Rogue")
        code, err, calls = run_main([self.root], "--classes", "dup-four-rogues,dup-four-paladins", out=self.out)
        self.assertEqual(code, 1)
        self.assertEqual(calls, [])
        self.assertFalse(self.out.exists())
        for name in ("dup-four-rogues-vigil", "dup-four-rogues-vault", *(f"dup-four-paladins-{b}" for b in duplicate_sheet.BOARDS)):
            self.assertIn(f"MISSING {name}: no run has this fixture", err)
        self.assertNotIn("dup-four-rogues-root", err)
        self.assertIn("5 of 6 fixtures are missing or incomplete", err)

    def test_a_skipped_or_absent_shot_is_listed(self):
        self.complete("rogues", "Rogue")
        write_run(Path(self.tmp.name) / "other", "dup-four-paladins-root", "Paladin", status="skipped")
        write_run(Path(self.tmp.name) / "other", "dup-four-paladins-vigil", "Paladin", shot="battle-yaw090-default")
        code, err, calls = run_main([self.root, Path(self.tmp.name) / "other"], "--classes", "dup-four-rogues,dup-four-paladins", out=self.out)
        self.assertEqual(code, 1)
        self.assertEqual(calls, [])
        self.assertIn(f"dup-four-paladins-root: shot {SHOT} was not captured", err)
        self.assertIn(f"dup-four-paladins-vigil: shot {SHOT} was not captured", err)
        self.assertIn("dup-four-paladins-vault: no run has this fixture", err)

    def test_a_party_that_is_not_four_heroes_is_listed(self):
        self.complete("rogues", "Rogue", heroes=3)
        code, err, calls = run_main([self.root], "--classes", "dup-four-rogues", out=self.out)
        self.assertEqual(code, 1)
        self.assertEqual(calls, [])
        self.assertEqual(err.count("shows 3 party heroes, expected 4"), 3)

    def test_foes_do_not_count_as_heroes(self):
        self.complete("rogues", "Rogue", heroes=2, foes=4)                       # 6 units in the shot, only 2 are the party
        code, err, _ = run_main([self.root], "--classes", "dup-four-rogues", out=self.out)
        self.assertEqual(code, 1)
        self.assertIn("shows 2 party heroes, expected 4", err)

    def test_the_same_fixture_in_two_runs_is_ambiguous(self):
        self.complete("rogues", "Rogue")
        for board in duplicate_sheet.BOARDS:
            write_run(Path(self.tmp.name) / "before", f"dup-four-rogues-{board}", "Rogue")
        code, err, calls = run_main([self.root, Path(self.tmp.name) / "before"], "--classes", "dup-four-rogues", out=self.out)
        self.assertEqual(code, 1)
        self.assertEqual(calls, [])
        self.assertEqual(err.count("is captured in 2 runs"), 3)

    def test_an_unknown_party_is_refused(self):
        self.complete("rogues", "Rogue")
        err = io.StringIO()
        with contextlib.redirect_stderr(err), self.assertRaises(SystemExit) as caught:
            duplicate_sheet.main([str(self.root), "--out", str(self.out), "--classes", "dup-four-rogue"])
        self.assertEqual(caught.exception.code, 2)
        self.assertIn("unknown party: dup-four-rogue", err.getvalue())

    def test_another_shot_is_validated_and_cropped(self):
        for board in duplicate_sheet.BOARDS:
            write_run(self.root, f"dup-four-rogues-{board}", "Rogue", shot="battle-yawm90-default")
        code, err, _ = run_main([self.root], "--classes", "dup-four-rogues", out=self.out)
        self.assertEqual(code, 1)                                                # the default shot was not captured
        self.assertIn(f"shot {SHOT} was not captured", err)
        code, err, calls = run_main([self.root], "--classes", "dup-four-rogues", "--shot", "battle-yawm90-default", out=self.out)
        self.assertEqual((code, err), (0, ""))
        self.assertEqual(len([c for c in calls if "-crop" in c]), 3)


if __name__ == "__main__":
    unittest.main()
