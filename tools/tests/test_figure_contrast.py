"""Offline tests for tools/figure_contrast.py (#150): the contrast and merge numbers on synthetic shots, and the manifest walk."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import figure_contrast
from test_impact_flash import encode

W, H = 120, 80
BOX = (40, 10, 80, 70)


def shot(ground, figure):
    """A W x H image of `ground` with a `figure` coloured block inside BOX (the middle half of its columns, rows 8% to 80%)."""
    figure_box, _ = figure_contrast.regions(BOX)
    rows = []
    for y in range(H):
        rows.append([(figure if figure_box[0] <= x < figure_box[2] and figure_box[1] <= y < figure_box[3] else ground) + (255,) for x in range(W)])
    return encode(rows)


def write(directory, name, png, role="Rogue", rigged=True):
    (directory / name).write_bytes(png)
    return {"name": name[:-4], "mode": "battle", "status": "captured", "file": name, "units": [
        {"id": 1, "name": "Nix", "role": role, "side": "party", "rigged": rigged, "box": {"x0": BOX[0], "y0": BOX[1], "x1": BOX[2], "y1": BOX[3]}}]}


class FigureContrastTest(unittest.TestCase):
    def test_luminance_and_ratio(self):
        self.assertAlmostEqual(figure_contrast.relative_luminance(255, 255, 255), 1.0, places=6)
        self.assertAlmostEqual(figure_contrast.relative_luminance(0, 0, 0), 0.0, places=6)
        self.assertAlmostEqual(figure_contrast.contrast_ratio(1.0, 0.0), 21.0, places=6)
        self.assertEqual(figure_contrast.contrast_ratio(0.3, 0.3), 1.0)

    def test_regions_sit_beside_the_box_and_in_it(self):
        figure, strips = figure_contrast.regions(BOX)
        self.assertEqual(figure, (50, 14, 70, 58))
        self.assertEqual(strips, [(0, 14, 40, 58), (80, 14, 120, 58)])

    def test_a_dark_figure_on_dark_ground_merges_and_a_pale_one_does_not(self):
        with tempfile.TemporaryDirectory() as folder:
            directory = Path(folder)
            dark = write(directory, "battle-yaw000-default.png", shot((30, 30, 40), (34, 32, 42)))
            pale = write(directory, "battle-yaw090-default.png", shot((30, 30, 40), (230, 220, 200)))
            (directory / "manifest.json").write_text(json.dumps({"fixture": "f", "resolution": {"width": W, "height": H}, "shots": [dark, pale]}))
            results = {r["shot"]: r for r in figure_contrast.measure_manifest(directory / "manifest.json", "Rogue")}
            self.assertEqual(results["battle-yaw000-default"]["mergeFraction"], 1.0)
            self.assertLess(results["battle-yaw000-default"]["contrastRatio"], 1.2)
            self.assertEqual(results["battle-yaw090-default"]["mergeFraction"], 0.0)
            self.assertGreater(results["battle-yaw090-default"]["contrastRatio"], 8.0)
            summary = figure_contrast.summarize(list(results.values()))
            self.assertEqual(summary["shots"], 2)
            self.assertEqual(summary["contrastRatioMin"], results["battle-yaw000-default"]["contrastRatio"])

    def test_only_the_chosen_role_and_pattern_are_measured(self):
        with tempfile.TemporaryDirectory() as folder:
            directory = Path(folder)
            rows = [write(directory, "battle-yaw000-default.png", shot((30, 30, 40), (200, 200, 200)), role="Cleric"),
                    write(directory, "battle-yaw000-near.png", shot((30, 30, 40), (200, 200, 200)))]
            (directory / "manifest.json").write_text(json.dumps({"fixture": "f", "shots": rows}))
            self.assertEqual(figure_contrast.measure_manifest(directory / "manifest.json", "Rogue"), [])
            self.assertEqual(len(figure_contrast.measure_manifest(directory / "manifest.json", "Cleric")), 1)
            self.assertEqual(len(figure_contrast.measure_manifest(directory / "manifest.json", "Rogue", pattern="-near")), 1)
            self.assertEqual(len(figure_contrast.measure_manifest(directory / "manifest.json", "Rogue", names={"battle-yaw000-near"})), 1)

    def test_main_exits_2_when_nothing_matches(self):
        with tempfile.TemporaryDirectory() as folder:
            (Path(folder) / "manifest.json").write_text(json.dumps({"fixture": "f", "shots": []}))
            self.assertEqual(figure_contrast.main([folder, "--role", "Rogue"]), 2)


if __name__ == "__main__":
    unittest.main()
