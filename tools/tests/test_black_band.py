"""Offline tests for tools/black_band.py (#209, on-screen D1): a solid black band across the board area of a shot is found, a dark but lit floor is not."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import black_band
from test_impact_flash import encode


def image(width, height, band=None, floor=(22, 26, 48)):
    """RGBA rows: the floor colour everywhere, black rows in `band` (a range of rows) across the middle of the width."""
    rows = []
    for y in range(height):
        black = band is not None and y in band
        rows.append([(0, 0, 0, 255) if black and width * 0.1 < x < width * 0.97 else floor + (255,) for x in range(width)])
    return rows


def write_shot(directory, name, pixels):
    (directory / f"{name}.png").write_bytes(encode(pixels, channels=4, filters=(0,)))
    return {"name": name, "mode": "battle", "status": "captured", "file": f"{name}.png"}


class BlackBandTest(unittest.TestCase):
    def test_longest_run_is_found_in_the_board_rows(self):
        pixels = image(100, 100, band=range(80, 88))
        width, height, channels, rows = black_band.impact_flash.read_png(self.write(pixels))
        self.assertEqual(black_band.longest_black_run(width, height, channels, rows), (8, 80))

    def test_a_dark_floor_is_not_a_band(self):
        pixels = image(100, 100, band=None, floor=(30, 34, 60))
        width, height, channels, rows = black_band.impact_flash.read_png(self.write(pixels))
        self.assertEqual(black_band.longest_black_run(width, height, channels, rows)[0], 0)

    def test_rows_below_the_board_area_are_ignored(self):
        pixels = image(100, 100, band=range(92, 100))
        width, height, channels, rows = black_band.impact_flash.read_png(self.write(pixels))
        self.assertEqual(black_band.longest_black_run(width, height, channels, rows)[0], 0)

    def test_manifest_walk_fails_only_the_banded_shot(self):
        with tempfile.TemporaryDirectory() as folder:
            directory = Path(folder)
            shots = [write_shot(directory, "vault-far-yaw090", image(100, 100, band=range(78, 88))), write_shot(directory, "vault-far-yaw000", image(100, 100))]
            (directory / "manifest.json").write_text(json.dumps({"fixture": "board-vault", "shots": shots}))
            results = black_band.measure_manifest(directory / "manifest.json", "vault-far")
            by_name = {r["shot"]: r for r in results}
            self.assertEqual(by_name["vault-far-yaw090"]["runRows"], 10)
            self.assertEqual(by_name["vault-far-yaw000"]["runRows"], 0)
            self.assertEqual(black_band.main([folder, "--pattern", "vault-far"]), 1)
            self.assertEqual(black_band.main([folder, "--pattern", "yaw000"]), 0)
            self.assertEqual(black_band.main([folder, "--pattern", "nothing-matches"]), 2)

    def write(self, pixels):
        folder = tempfile.mkdtemp()
        self.addCleanup(lambda: __import__("shutil").rmtree(folder, ignore_errors=True))
        path = Path(folder) / "shot.png"
        path.write_bytes(encode(pixels, channels=4, filters=(0,)))
        return path


if __name__ == "__main__":
    unittest.main()
