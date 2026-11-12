import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import overview_probe  # noqa: E402


def write_png(path, width, height, pixel):
    """An 8-bit RGB PNG filled by pixel(x, y) -> (r, g, b)."""
    raw = b"".join(b"\0" + b"".join(bytes(pixel(x, y)) for x in range(width)) for y in range(height))

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    Path(path).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


class OverviewProbeTests(unittest.TestCase):
    def test_projection_matches_the_overview_camera(self):
        # The table's middle is above the screen centre's line of sight and the near edge lower in the frame; the two sides mirror about the middle.
        centre = overview_probe.project(overview_probe.table_to_ue(0.0, 0.0), 1920, 1080)
        near = overview_probe.project(overview_probe.table_to_ue(1.7, 0.0), 1920, 1080)
        self.assertAlmostEqual(near[0], 960.0, places=3)
        self.assertGreater(near[1], 800)
        self.assertLess(centre[1], near[1])
        right = overview_probe.project(overview_probe.table_to_ue(1.7, 0.5), 1920, 1080)
        left = overview_probe.project(overview_probe.table_to_ue(1.7, -0.5), 1920, 1080)
        self.assertGreater(right[0], 960)
        self.assertAlmostEqual(right[0] - 960, 960 - left[0], places=3)
        # The viewer's right is +Y of the asset; the same frame at 1280 x 720 is the same picture at two thirds of the size.
        small = overview_probe.project(overview_probe.table_to_ue(1.7, 0.5), 1280, 720)
        self.assertAlmostEqual(small[0] * 1.5, right[0], places=3)
        self.assertAlmostEqual(small[1] * 1.5, right[1], places=3)

    def test_a_blown_out_prop_fails_and_a_shaded_one_passes(self):
        box = overview_probe.prop_box(0.7, 1.0, 0.085, 0.14, 1920, 1080)
        with tempfile.TemporaryDirectory() as directory:
            blown = Path(directory) / "blown.png"
            shaded = Path(directory) / "shaded.png"
            write_png(blown, 1920, 1080, lambda x, y: (255, 255, 255) if box[0] <= x < box[2] and box[1] <= y < box[3] else (40, 30, 20))
            write_png(shaded, 1920, 1080, lambda x, y: (200, 190, 170) if box[0] <= x < box[2] and box[1] <= y < box[3] else (40, 30, 20))
            self.assertFalse(overview_probe.measure(blown, 0.7, 1.0)["pass"])
            result = overview_probe.measure(shaded, 0.7, 1.0)
            self.assertTrue(result["pass"])
            self.assertLess(result["peakLuma"], 254)
            self.assertEqual(overview_probe.main([str(blown), "--at", "0.7", "1.0"]), 1)
            self.assertEqual(overview_probe.main([str(shaded), "--at", "0.7", "1.0"]), 0)


if __name__ == "__main__":
    unittest.main()
