"""Offline tests for tools/impact_flash.py (#189): the PNG decoder, the footprint statistics and the manifest walk."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import impact_flash


def chunk(kind, body):
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))


def paeth(a, b, c):
    return impact_flash._paeth(a, b, c)


def encode(pixels, channels=4, filters=(0, 1, 2, 3, 4)):
    """A PNG of `pixels` (rows of tuples), each row written with the next filter in `filters`, so every filter is exercised."""
    height, width = len(pixels), len(pixels[0])
    raw = bytearray()
    previous = bytes(width * channels)
    for y, row in enumerate(pixels):
        line = bytes(value for pixel in row for value in pixel[:channels])
        kind = filters[y % len(filters)]
        out = bytearray()
        for i, value in enumerate(line):
            left = line[i - channels] if i >= channels else 0
            up = previous[i]
            corner = previous[i - channels] if i >= channels else 0
            predicted = (0, left, up, (left + up) >> 1, paeth(left, up, corner))[kind]
            out.append((value - predicted) & 255)
        raw += bytes([kind]) + out
        previous = line
    header = struct.pack(">IIBBBBB", width, height, 8, 6 if channels == 4 else 2, 0, 0, 0)
    return impact_flash.PNG_SIGNATURE + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(raw))) + chunk(b"IEND", b"")


def gradient(width, height, channels=4):
    return [[((x * 7 + y * 3) & 255, (x * 5 + y * 11) & 255, (x * 13 + y * 2) & 255, 255)[:channels] for x in range(width)] for y in range(height)]


class ImpactFlashTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def write(self, name, pixels, channels=4):
        path = self.root / name
        path.write_bytes(encode(pixels, channels))
        return path

    def test_decoder_reads_every_filter_in_rgba_and_rgb(self):
        for channels in (4, 3):
            pixels = gradient(23, 10, channels)
            width, height, found, rows = impact_flash.read_png(self.write(f"g{channels}.png", pixels, channels))
            self.assertEqual((width, height, found), (23, 10, channels))
            for y, row in enumerate(pixels):
                self.assertEqual(rows[y], bytes(v for pixel in row for v in pixel), f"row {y} with {channels} channels")

    def test_decoder_can_stop_early(self):
        pixels = gradient(30, 12)
        _, height, channels, rows = impact_flash.read_png(self.write("g.png", pixels), rows=5, columns=9)
        self.assertEqual(height, 12)
        self.assertEqual(len(rows), 5)
        for y in range(5):
            self.assertEqual(rows[y], bytes(v for pixel in pixels[y][:9] for v in pixel))

    def test_decoder_rejects_what_it_cannot_read(self):
        bad = self.root / "bad.png"
        bad.write_bytes(b"not a png at all")
        with self.assertRaises(ValueError):
            impact_flash.read_png(bad)
        interlaced = impact_flash.PNG_SIGNATURE + chunk(b"IHDR", struct.pack(">IIBBBBB", 2, 2, 8, 6, 0, 0, 1)) + chunk(b"IDAT", zlib.compress(bytes(18))) + chunk(b"IEND", b"")
        bad.write_bytes(interlaced)
        with self.assertRaises(ValueError):
            impact_flash.read_png(bad)
        short = impact_flash.PNG_SIGNATURE + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 4, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(bytes(10))) + chunk(b"IEND", b"")
        bad.write_bytes(short)
        with self.assertRaisesRegex(ValueError, "truncated"):
            impact_flash.read_png(bad)

    def test_statistics_count_white_and_peak(self):
        grey, white = (100, 100, 100, 255), (255, 255, 255, 255)
        pixels = [[white if (x, y) in {(2, 2), (3, 2)} else grey for x in range(10)] for y in range(10)]
        _, _, channels, rows = impact_flash.read_png(self.write("w.png", pixels))
        stats = impact_flash.measure_box(rows, channels, (0, 0, 10, 10))
        self.assertEqual(stats["pixels"], 100)
        self.assertAlmostEqual(stats["whiteFraction"], 0.02)
        self.assertAlmostEqual(stats["peakLuma"], 255.0, places=1)
        self.assertFalse(impact_flash.verdict(stats))
        # The same two pixels are outside a box that misses them.
        inside = impact_flash.measure_box(rows, channels, (5, 5, 10, 10))
        self.assertEqual(inside["whiteFraction"], 0.0)
        self.assertAlmostEqual(inside["peakLuma"], 100.0, places=1)
        self.assertTrue(impact_flash.verdict(inside))
        with self.assertRaises(ValueError):
            impact_flash.measure_box(rows, channels, (4, 4, 4, 9))

    def test_verdict_limits(self):
        edge = {"peakLuma": impact_flash.CLIP_LEVEL - 0.01, "whiteFraction": impact_flash.MAX_WHITE_FRACTION - 0.0001}
        self.assertTrue(impact_flash.verdict(edge))
        self.assertFalse(impact_flash.verdict({**edge, "peakLuma": impact_flash.CLIP_LEVEL}))
        self.assertFalse(impact_flash.verdict({**edge, "whiteFraction": impact_flash.MAX_WHITE_FRACTION}))
        # Near-white (just under the white level) is not "pure white".
        near = [[(impact_flash.WHITE_LEVEL - 1,) * 3 + (255,)] * 4] * 4
        _, _, channels, rows = impact_flash.read_png(self.write("near.png", near))
        stats = impact_flash.measure_box(rows, channels, (0, 0, 4, 4))
        self.assertEqual(stats["whiteFraction"], 0.0)
        self.assertEqual(stats["clippedFraction"], 0.0)

    def manifest(self, shots, width=20, height=12):
        path = self.root / "manifest.json"
        path.write_text(json.dumps({"version": 1, "status": "ok", "fixture": "board-a", "resolution": {"width": width, "height": height}, "shots": shots}))
        return path

    def impact(self, name, file, box, status="captured"):
        shot = {"name": name, "mode": "impact", "status": status, "file": file, "target": {"id": 3, "name": "Raider", "cellX": 4, "cellY": 5}}
        if box:
            shot["target"]["footprint"] = dict(zip(("x0", "y0", "x1", "y1"), box))
        return shot

    def test_manifest_walk_measures_only_captured_impact_shots_with_a_box(self):
        blown = [[(255, 255, 255, 255)] * 20 for _ in range(12)]
        calm = [[(90, 60, 50, 255)] * 20 for _ in range(12)]
        self.write("blown.png", blown)
        self.write("calm.png", calm)
        path = self.manifest([
            self.impact("hit-blown", "blown.png", (2, 2, 10, 10)),
            self.impact("hit-calm", "calm.png", (2, 2, 10, 10)),
            self.impact("hit-skipped", "calm.png", (2, 2, 10, 10), status="skipped"),
            self.impact("hit-nobox", "calm.png", None),
            {"name": "battle", "mode": "battle", "status": "captured", "file": "calm.png"},
        ])
        results = impact_flash.measure_manifest(path)
        self.assertEqual([r["shot"] for r in results], ["hit-blown", "hit-calm"])
        self.assertFalse(results[0]["pass"])
        self.assertTrue(results[1]["pass"])
        self.assertEqual(results[1]["box"], [2, 2, 10, 10])
        self.assertEqual(results[1]["cell"], [4, 5])
        self.assertEqual([r["shot"] for r in impact_flash.measure_manifest(path, {"hit-calm"})], ["hit-calm"])

    def test_unmeasured_shots_are_named(self):
        path = self.manifest([self.impact("near", "calm.png", None), self.impact("far", "calm.png", (0, 0, 5, 5)), self.impact("skipped", "calm.png", None, status="skipped")])
        self.assertEqual(impact_flash.unmeasured_shots(path), ["board-a 20x12 near"])
        self.assertEqual(impact_flash.unmeasured_shots(path, {"far"}), [])

    def test_size_mismatch_is_reported(self):
        self.write("calm.png", [[(90, 60, 50, 255)] * 20 for _ in range(12)])
        path = self.manifest([self.impact("hit", "calm.png", (0, 0, 5, 5))], width=40, height=12)
        with self.assertRaisesRegex(ValueError, "20x12"):
            impact_flash.measure_manifest(path)

    def test_command_line_exit_codes(self):
        self.write("blown.png", [[(255, 255, 255, 255)] * 20 for _ in range(12)])
        self.write("calm.png", [[(90, 60, 50, 255)] * 20 for _ in range(12)])
        self.manifest([self.impact("hit", "calm.png", (2, 2, 10, 10))])
        output = self.root / "out.json"
        self.assertEqual(impact_flash.main([str(self.root), "--json", str(output)]), 0)
        self.assertEqual(json.loads(output.read_text())["shots"][0]["shot"], "hit")
        self.manifest([self.impact("hit", "blown.png", (2, 2, 10, 10))])
        self.assertEqual(impact_flash.main([str(self.root / "manifest.json")]), 1)
        self.manifest([self.impact("hit", "blown.png", None)])
        self.assertEqual(impact_flash.main([str(self.root)]), 2)


if __name__ == "__main__":
    unittest.main()
