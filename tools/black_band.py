#!/usr/bin/env python3
"""Finds solid black bands in the board area of gallery shots (#209, on-screen D1).

From a camera outside the Vault's hall, the near wall's outer face is black and opaque, so a frame that shows it has rows across the board area
where almost every pixel is near-black: "a black band at the bottom of the frame". This reads the `battle` shots of a `gallery` run and reports, per
shot, the longest run of black rows between 45% and 89% of the height (above the action bar), a row counting as black when at least BLACK_SHARE of its pixels
between 20% and 94% of the width have every channel at or under BLACK_LEVEL (the party cards at the left are out of the span). A shot fails at a run of
MAX_RUN_FRACTION of the height or more. Standard library only (impact_flash's PNG decoder).

    python3 tools/black_band.py .local/gallery/<run> [--pattern vault-far] [--max-run 0.03]

Exit 0 when no shot has a band, 1 when one does, 2 when nothing was measured.
"""
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import impact_flash

BLACK_LEVEL = 14
BLACK_SHARE = 0.90
MAX_RUN_FRACTION = 0.03
ROW_SPAN = (0.45, 0.89)
COLUMN_SPAN = (0.20, 0.94)


def longest_black_run(width, height, channels, image):
    """(rows in the longest run of black rows, first row of it) in the board-area rows of the image."""
    first, last = int(height * ROW_SPAN[0]), int(height * ROW_SPAN[1])
    x0, x1 = int(width * COLUMN_SPAN[0]), int(width * COLUMN_SPAN[1])
    best = (0, first)
    run, start = 0, first
    for y in range(first, last):
        row = image[y]
        black = 0
        for x in range(x0, x1):
            base = x * channels
            if row[base] <= BLACK_LEVEL and row[base + 1] <= BLACK_LEVEL and row[base + 2] <= BLACK_LEVEL:
                black += 1
        if black >= BLACK_SHARE * (x1 - x0):
            if run == 0:
                start = y
            run += 1
            if run > best[0]:
                best = (run, start)
        else:
            run = 0
    return best


def measure_manifest(manifest_path, pattern=""):
    manifest_path = Path(manifest_path)
    manifest = json.loads(manifest_path.read_text())
    results = []
    for shot in manifest.get("shots", []):
        if shot.get("status") != "captured" or shot.get("mode") != "battle" or pattern not in shot["name"]:
            continue
        width, height, channels, image = impact_flash.read_png(manifest_path.parent / shot["file"])
        run, start = longest_black_run(width, height, channels, image)
        results.append({"fixture": manifest.get("fixture", ""), "shot": shot["name"], "size": [width, height], "runRows": run, "runFraction": round(run / height, 4), "firstRow": start})
    return results


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("path", help="a gallery run folder (or any folder with manifest.json files)")
    parser.add_argument("--pattern", default="", help="only shots whose name contains this")
    parser.add_argument("--max-run", type=float, default=MAX_RUN_FRACTION, help="fail at a run of this fraction of the height (default %(default)s)")
    parser.add_argument("--json", help="write the results here")
    args = parser.parse_args(argv)
    results = []
    for manifest in sorted(Path(args.path).rglob("manifest.json")):
        results.extend(measure_manifest(manifest, args.pattern))
    if not results:
        print("No battle shots were measured.", file=sys.stderr)
        return 2
    bad = [r for r in results if r["runFraction"] >= args.max_run]
    for r in sorted(results, key=lambda r: -r["runFraction"])[:12]:
        print(f"{r['fixture']:14s} {r['shot']:34s} {r['size'][0]}x{r['size'][1]}  black run {r['runRows']:4d} rows ({r['runFraction'] * 100:4.1f}%) from row {r['firstRow']}")
    print(f"{len(results)} shot(s) measured, {len(bad)} with a black band of {args.max_run * 100:.0f}% of the height or more")
    if args.json:
        Path(args.json).write_text(json.dumps({"results": results, "failed": [r["shot"] + "@" + str(r["size"][0]) for r in bad]}, indent=2))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
