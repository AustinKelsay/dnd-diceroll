#!/usr/bin/env python3
"""Luma of one overview prop in a capture (issue #149): the war table's tankard, to check the candle bloom does not blow it out.

  python3 tools/overview_probe.py CAPTURE.png --at X Y [--radius 0.085 --height 0.14] [--json FILE]

X and Y are the prop's table position in metres (the asset frame: X toward the viewer, Y the viewer's right, from the middle of the table top). The prop's box is projected with the
overview camera (eye 420 cm back and 208 cm over the table top, aimed 32 degrees down, horizontal FOV 50), then measured with the luma of impact_flash.py: Rec. 709 weights on the
8-bit sRGB code values, a clipping proxy. A box passes when its peak luma is below 254 and under 2 % of its pixels are pure white (a few of them are the candle flame's own).
Standard library only. Exit 1 when the box fails.
"""
import argparse
import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import impact_flash  # noqa: E402

EYE = (0.0, -420.0, 208.0)
LOOK = (0.0, 450.0, -282.0)
FOV = 50.0


def _norm(v):
    n = math.sqrt(sum(a * a for a in v))
    return tuple(a / n for a in v)


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def project(point_cm, width, height):
    """Table-top cm in Unreal axes (X screen-left, Y away from the camera, Z up) to pixels."""
    forward = _norm(LOOK)
    right = _norm(_cross((0, 0, 1), forward))
    up = _cross(forward, right)
    v = tuple(point_cm[i] - EYE[i] for i in range(3))
    x, y, z = (sum(v[i] * axis[i] for i in range(3)) for axis in (right, up, forward))
    t = math.tan(math.radians(FOV / 2))
    return ((x / (z * t)) * 0.5 + 0.5) * width, (0.5 - (y / (z * t / (width / height))) * 0.5) * height


def table_to_ue(x_a, y_a, z=0.0):
    """Asset frame metres (X toward the viewer, Y the viewer's right) to Unreal cm relative to the table top: X = -Y_a, Y = -X_a."""
    return (-y_a * 100.0, -x_a * 100.0, z * 100.0)


def prop_box(x_a, y_a, radius, height, width, image_height):
    pixels = [project(table_to_ue(x_a + dx, y_a + dy, z), width, image_height) for dx in (-radius, radius) for dy in (-radius, radius) for z in (0.0, height)]
    return (int(min(p[0] for p in pixels)), int(min(p[1] for p in pixels)), int(max(p[0] for p in pixels)) + 1, int(max(p[1] for p in pixels)) + 1)


def measure(png, x_a, y_a, radius=0.085, height=0.14):
    width, image_height, channels, rows = impact_flash.read_png(png)
    box = prop_box(x_a, y_a, radius, height, width, image_height)
    stats = impact_flash.measure_box(rows, channels, box)
    stats["box"] = list(box)
    stats["pass"] = impact_flash.verdict(stats)
    return stats


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("png")
    parser.add_argument("--at", nargs=2, type=float, required=True, metavar=("X", "Y"))
    parser.add_argument("--radius", type=float, default=0.085)
    parser.add_argument("--height", type=float, default=0.14)
    parser.add_argument("--json")
    args = parser.parse_args(argv)
    stats = measure(args.png, args.at[0], args.at[1], args.radius, args.height)
    print(json.dumps(stats))
    if args.json:
        Path(args.json).write_text(json.dumps(stats, indent=2) + "\n")
    return 0 if stats["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
