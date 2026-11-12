#!/usr/bin/env python3
"""How much of a figure the duplicate-identity accent recolours, measured in game (#150 batch 4, lead's call on PR #203: at most about 10 to 15%).

Takes two gallery runs of the same `acc-<class>-root` fixture (four heroes of one build of one class, so each carries an accent), one plain and one
with `gallery --no-accents` (the same subclass palette without the accent), at the default-zoom shots. For every hero of the class and every shot it
compares the two screenshots in the figure region (the middle half of the unit's footprint box, rows 8% to 80%, the region tools/figure_contrast.py
uses) and reports

* changed: the pixels whose colour moved by more than THRESHOLD in any channel,
* figure: the pixels of that region whose luma differs from the ground beside it (the two strips of figure_contrast) by more than MERGE_BAND, in the
  plain run: an estimate of the figure's own visible pixels (there is no figure mask),
* share = changed / figure.

    python3 tools/accent_share.py --with RUN [RUN ...] --without RUN [RUN ...] [--max 0.15] [--json FILE]

Runs are matched by fixture name and shot. **Every one of the 13 classes must have both runs, every default-zoom shot of tools/capture/palette-poses.json captured in
both, and its four heroes in each shot** (20 measurements); anything missing is reported and fails the run rather than being skipped. Exit 1 when a class exceeds --max
or anything is missing. Standard library only.
"""
import argparse
import json
from pathlib import Path
import re
import statistics
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import figure_contrast
import impact_flash

THRESHOLD = 14
FIXTURE = re.compile(r"^acc-(\w+)-root$")
HEROES = 4
PALETTES = Path(__file__).resolve().parents[1] / "Art" / "rig-pilot" / "cast" / "palettes" / "palettes.json"
POSES = Path(__file__).resolve().parent / "capture" / "palette-poses.json"


def expected_classes():
    return sorted(json.loads(PALETTES.read_text())["classes"])


def expected_shots():
    return sorted(shot["name"] for shot in json.loads(POSES.read_text())["shots"] if "-default" in shot["name"])


def problems(cls, with_manifest, without_manifest, shots, heroes=HEROES):
    """What is missing for one class, from the two manifests alone (None for a missing run): the runs, each expected shot captured in both, and the heroes of the class in each."""
    found = []
    for label, manifest in (("with accents", with_manifest), ("without accents", without_manifest)):
        if manifest is None:
            found.append(f"{cls}: no {label} run (fixture acc-{cls}-root)")
            continue
        captured = {s["name"]: s for s in manifest.get("shots", []) if s.get("status") == "captured"}
        for shot in shots:
            if shot not in captured:
                found.append(f"{cls}: {label}: shot {shot} was not captured")
            else:
                count = sum(1 for u in captured[shot].get("units", []) if u.get("role") == cls.capitalize())
                if count != heroes:
                    found.append(f"{cls}: {label}: {shot} shows {count} {cls}s, expected {heroes}")
    return found


def manifests(runs):
    found = {}
    for run in runs:
        for path in sorted(Path(run).rglob("manifest.json")):
            manifest = json.loads(path.read_text())
            if FIXTURE.match(manifest.get("fixture", "")):
                found[manifest["fixture"]] = (path, manifest)
    return found


def measure(with_item, without_item, role, shots):
    (path_a, man_a), (path_b, man_b) = with_item, without_item
    shots_a = {s["name"]: s for s in man_a.get("shots", []) if s.get("status") == "captured"}
    shots_b = {s["name"]: s for s in man_b.get("shots", []) if s.get("status") == "captured"}
    rows = []
    for name in shots:
        shot = shots_a[name]
        for unit in shot.get("units", []):
            if unit.get("role") != role:
                continue
            box = unit["box"]
            rows_needed = box["y1"] + 2
            _, _, channels, image_a = impact_flash.read_png(path_a.parent / shot["file"], rows=rows_needed)
            _, _, _, image_b = impact_flash.read_png(path_b.parent / shots_b[name]["file"], rows=rows_needed)
            figure_box, strips = figure_contrast.regions((box["x0"], box["y0"], box["x1"], box["y1"]))
            pa = list(figure_contrast.pixels(image_a, channels, *figure_box))
            pb = list(figure_contrast.pixels(image_b, channels, *figure_box))
            ground = [impact_flash.luma(*p) for strip in strips for p in figure_contrast.pixels(image_b, channels, *strip)]
            if not pa or len(pa) != len(pb) or not ground:
                raise SystemExit(f"{role} {unit['name']} in {name}: the figure region has no pixels to compare")
            median = statistics.median(ground)
            changed = sum(1 for a, b in zip(pa, pb) if max(abs(a[0] - b[0]), abs(a[1] - b[1]), abs(a[2] - b[2])) > THRESHOLD)
            figure = sum(1 for p in pb if abs(impact_flash.luma(*p) - median) > figure_contrast.MERGE_BAND)
            rows.append(dict(shot=name, unit=unit["name"], changed=changed, figure=figure, share=round(changed / max(figure, 1), 4)))
    return rows


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--with", dest="with_runs", nargs="+", required=True)
    parser.add_argument("--without", dest="without_runs", nargs="+", required=True)
    parser.add_argument("--max", type=float, default=0.15)
    parser.add_argument("--json", metavar="FILE")
    args = parser.parse_args(argv)
    with_runs, without_runs = manifests(args.with_runs), manifests(args.without_runs)
    classes, shots = expected_classes(), expected_shots()
    result, failed, missing = {}, [], []
    for cls in classes:
        fixture = f"acc-{cls}-root"
        found = problems(cls, with_runs.get(fixture, (None, None))[1], without_runs.get(fixture, (None, None))[1], shots)
        if found:
            missing += found
            continue
        rows = measure(with_runs[fixture], without_runs[fixture], cls.capitalize(), shots)
        shares = [r["share"] for r in rows]
        result[cls] = dict(units=len(rows), mean=round(statistics.fmean(shares), 4), worst=max(shares), rows=rows)
        if statistics.fmean(shares) > args.max or max(shares) > args.max * 1.5:
            failed.append(cls)
        print(f"{cls:10} {len(rows):3} measurements  accent share of the figure: mean {statistics.fmean(shares) * 100:5.1f}%  worst {max(shares) * 100:5.1f}%")
    for line in missing:
        print("MISSING " + line, file=sys.stderr)
    if args.json:
        Path(args.json).write_text(json.dumps(result, indent=1) + "\n")
    if failed:
        print("over the limit: " + ", ".join(failed), file=sys.stderr)
    return 1 if failed or missing or not result else 0


if __name__ == "__main__":
    sys.exit(main())
