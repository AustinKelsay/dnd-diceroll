#!/usr/bin/env python3
"""Measure how well one class's figure reads against the ground behind it (#150, from the gate 2 finding on #148).

Reads the `battle` and `targeting` shots of a capture gallery run (`python3 tools/dev.py gallery`). The game writes every figure's
projected footprint box into the manifest (`units`: the footprint ring's square, from the board top to the figure's head, with the camera
the screenshot sees). For each shot and each unit of the chosen role this tool takes

* the figure region: the middle half of the box's width and rows 8% to 80% of its height (head, torso and legs, not the ring or the floor
  under the feet), and
* the ground: two strips of the same rows, each as wide as the box and just outside it on the left and right. Other figures, the HUD or
  scenery inside a strip count too; that is the ground the figure is actually seen against.

and reports, with luminance computed from the 8-bit sRGB code values after the standard sRGB-to-linear conversion (Rec. 709 weights):

* contrast ratio: (lighter + 0.05) / (darker + 0.05) of the mean linear luminance of the figure region and of the ground (WCAG's formula; 1 is
  invisible, 3 is the usual floor for large shapes)
* merge fraction: the share of figure-region pixels whose 8-bit luma (Rec. 709 weights on the code values) is within MERGE_BAND of the ground's median luma; a
  figure that merges with a shadow pool has a high one
* mean luma of the figure and of the ground, 0 to 255

It does not separate the figure from the pixels around it (there is no mask), so the figure region also holds some floor, ring and bar; compare the same
fixtures with and without the change (for example `gallery --static-figures` against a plain gallery), not an absolute value against a standard. A
visual check of silhouette, ring and bar still goes with it. Standard library only (the PNG decoder of tools/impact_flash.py).

    python3 tools/figure_contrast.py .local/gallery/<run> --role Rogue                   # the default-zoom shots, every fixture
    python3 tools/figure_contrast.py .local/gallery/<run> --role Rogue --shot battle-yaw000-default --json out.json

Exit 0 when something was measured, 2 when nothing could be.
"""
import argparse
import json
from pathlib import Path
import statistics
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import impact_flash

MERGE_BAND = 12.0
FIGURE_COLUMNS = (0.25, 0.75)
ROWS = (0.08, 0.80)


def linear(code):
    value = code / 255.0
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def relative_luminance(r, g, b):
    return 0.2126 * linear(r) + 0.7152 * linear(g) + 0.0722 * linear(b)


def contrast_ratio(a, b):
    high, low = max(a, b), min(a, b)
    return (high + 0.05) / (low + 0.05)


def pixels(image, channels, x0, y0, x1, y1):
    """The (r, g, b) of every pixel in the box, clipped to the image."""
    width = len(image[0]) // channels
    x0, x1 = max(0, x0), min(width, x1)
    y0, y1 = max(0, y0), min(len(image), y1)
    for y in range(y0, y1):
        row = image[y]
        for x in range(x0, x1):
            i = x * channels
            yield row[i], row[i + 1], row[i + 2]


def regions(box):
    """(figure box, [left strip, right strip]) in pixels for a footprint box (x0, y0, x1, y1)."""
    x0, y0, x1, y1 = box
    width, height = x1 - x0, y1 - y0
    top, bottom = y0 + int(height * ROWS[0]), y0 + int(height * ROWS[1])
    figure = (x0 + int(width * FIGURE_COLUMNS[0]), top, x0 + int(width * FIGURE_COLUMNS[1]), bottom)
    return figure, [(x0 - width, top, x0, bottom), (x1, top, x1 + width, bottom)]


def measure_unit(image, channels, box):
    figure_box, strips = regions(box)
    figure = list(pixels(image, channels, *figure_box))
    ground = [p for strip in strips for p in pixels(image, channels, *strip)]
    if not figure or not ground:
        raise ValueError(f"The box {box} has no pixels in the image")
    figure_luminance = statistics.fmean(relative_luminance(*p) for p in figure)
    ground_luminance = statistics.fmean(relative_luminance(*p) for p in ground)
    figure_luma = [impact_flash.luma(*p) for p in figure]
    ground_luma = [impact_flash.luma(*p) for p in ground]
    median = statistics.median(ground_luma)
    return {
        "contrastRatio": round(contrast_ratio(figure_luminance, ground_luminance), 3),
        "mergeFraction": round(sum(1 for value in figure_luma if abs(value - median) <= MERGE_BAND) / len(figure_luma), 4),
        "figureLuma": round(statistics.fmean(figure_luma), 2),
        "groundLuma": round(statistics.fmean(ground_luma), 2),
        "figurePixels": len(figure),
        "groundPixels": len(ground),
    }


def measure_manifest(manifest_path, role, names=None, pattern="-default"):
    manifest_path = Path(manifest_path)
    manifest = json.loads(manifest_path.read_text())
    results = []
    for shot in manifest.get("shots", []):
        if shot.get("status") != "captured" or not shot.get("units"):
            continue
        if names and shot["name"] not in names:
            continue
        if not names and pattern not in shot["name"]:
            continue
        for unit in shot["units"]:
            if unit.get("role") != role:
                continue
            box = unit["box"]
            width, height, channels, image = impact_flash.read_png(manifest_path.parent / shot["file"], rows=box["y1"] + 2)
            result = measure_unit(image, channels, (box["x0"], box["y0"], box["x1"], box["y1"]))
            result.update(fixture=manifest.get("fixture", ""), shot=shot["name"], unit=unit["name"], role=role, rigged=unit.get("rigged", False),
                          size=[manifest.get("resolution", {}).get("width"), manifest.get("resolution", {}).get("height")], box=[box["x0"], box["y0"], box["x1"], box["y1"]])
            results.append(result)
    return results


def summarize(results):
    if not results:
        return {}
    return {
        "shots": len(results),
        "contrastRatioMean": round(statistics.fmean(r["contrastRatio"] for r in results), 3),
        "contrastRatioMin": min(r["contrastRatio"] for r in results),
        "mergeFractionMean": round(statistics.fmean(r["mergeFraction"] for r in results), 4),
        "figureLumaMean": round(statistics.fmean(r["figureLuma"] for r in results), 2),
        "groundLumaMean": round(statistics.fmean(r["groundLuma"] for r in results), 2),
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("path", help="a gallery run folder (every manifest.json below it) or one manifest.json")
    parser.add_argument("--role", required=True, help="the class to measure: the role's enum name, for example Rogue, Cleric, Fighter, Wizard")
    parser.add_argument("--shot", action="append", help="measure only this shot name (repeatable); default every shot whose name has --pattern")
    parser.add_argument("--pattern", default="-default", help="substring of the shot names to measure when --shot is not given (default -default: the play zoom)")
    parser.add_argument("--json", metavar="FILE", help="also write every measurement and the per-fixture summary to FILE")
    args = parser.parse_args(argv)
    root = Path(args.path)
    manifests = [root] if root.is_file() else sorted(root.rglob("manifest.json"))
    results = []
    for manifest in manifests:
        results.extend(measure_manifest(manifest, args.role, set(args.shot or []), args.pattern))
    if not results:
        print(f"No {args.role} figures with a footprint box were found.", file=sys.stderr)
        return 2
    for r in results:
        print(f"{r['fixture']:22} {r['shot']:26} {r['unit']:8} rigged={str(r['rigged']):5} contrast {r['contrastRatio']:5.2f}  merge {r['mergeFraction'] * 100:5.1f}%"
              f"  figure luma {r['figureLuma']:6.1f}  ground luma {r['groundLuma']:6.1f}")
    by_fixture = {}
    for r in results:
        by_fixture.setdefault(r["fixture"], []).append(r)
    summary = {fixture: summarize(rows) for fixture, rows in sorted(by_fixture.items())}
    summary["all"] = summarize(results)
    for name, s in summary.items():
        print(f"{name:22} mean contrast {s['contrastRatioMean']:.2f} (min {s['contrastRatioMin']:.2f}), mean merge {s['mergeFractionMean'] * 100:.1f}% over {s['shots']} shot(s)")
    if args.json:
        Path(args.json).write_text(json.dumps({"role": args.role, "mergeBand": MERGE_BAND, "summary": summary, "shots": results}, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
