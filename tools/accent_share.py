#!/usr/bin/env python3
"""How much of a figure the duplicate-identity accent recolours, measured in game (#150 batch 4, lead's call on PR #203: at most about 10 to 15%).

Takes two gallery runs of the same `acc-<class>-root` fixture (four heroes of one build of one class, so each carries an accent), one plain and one
with `gallery --no-accents` (the same subclass palette without the accent), at the default-zoom shots. For every hero of the class and every shot it
compares the two screenshots in the figure region (the middle half of the unit's footprint box, rows 8% to 80%, the region tools/figure_contrast.py
uses) and reports

* changed: the pixels whose colour moved by more than THRESHOLD in any channel,
* figure: the pixels of that region whose luma differs from the ground beside it (the two strips of figure_contrast) by more than MERGE_BAND, in the
  plain run: an estimate of the figure's own visible pixels (there is no figure mask),
* share = changed / figure,
* the accent patch: the mean colour of the changed pixels of each copy (in the run with accents), and how far apart the copies' patches are: for every pair of the four copies
  in a shot the CIE76 colour distance (Delta E) between their patch colours, averaged over the six pairs and then over the shots ("separation"), plus the nearest pair in any
  shot ("nearest"). A patch of a few pixels or a distance below about 20 is what "the copies look the same at the default zoom" measured as (rogues in PR #203).

    python3 tools/accent_share.py --with RUN [RUN ...] --without RUN [RUN ...] [--max 0.15] [--json FILE]
    python3 tools/accent_share.py ... --classes rogue,paladin           # only these classes (an evidence run of the classes that changed)

Runs are matched by fixture name and shot. **Every one of the 13 classes must have both runs, every default-zoom shot of tools/capture/palette-poses.json captured in
both, and its four heroes in each shot** (20 measurements); anything missing is reported and fails the run rather than being skipped. Exit 1 when a class's mean exceeds --max or its largest single view exceeds --max-worst (default 1.5 x --max)
(or its copies are closer than --min-separation) or anything is missing. Standard library only.
"""
import argparse
import itertools
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


def srgb_to_lab(rgb):
    """CIELAB (D65) of an 8-bit sRGB colour."""
    r, g, b = (figure_contrast.linear(c) for c in rgb)
    x, y, z = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047, 0.2126 * r + 0.7152 * g + 0.0722 * b, (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883
    f = lambda t: t ** (1 / 3) if t > 216 / 24389 else (24389 / 27 * t + 16) / 116
    fx, fy, fz = f(x), f(y), f(z)
    return 116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz)


def delta_e(a, b):
    """CIE76 colour distance between two 8-bit sRGB colours (about 2 is a just-noticeable difference, 50 is opposite colours)."""
    return sum((p - q) ** 2 for p, q in zip(srgb_to_lab(a), srgb_to_lab(b))) ** 0.5


def separation(colours):
    """(mean, nearest) pair distance over the copies' patch colours; None when fewer than two copies have a patch."""
    distances = [delta_e(a, b) for a, b in itertools.combinations([c for c in colours if c is not None], 2)]
    return (statistics.fmean(distances), min(distances)) if distances else None


def over_limit(shares, limit, worst_limit=None):
    """Whether a class's mean share exceeds `limit` or its largest single view exceeds `worst_limit` (default 1.5 x limit)."""
    return statistics.fmean(shares) > limit or max(shares) > (worst_limit if worst_limit is not None else limit * 1.5)


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
        colours = []
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
            patch = [a for a, b in zip(pa, pb) if max(abs(a[0] - b[0]), abs(a[1] - b[1]), abs(a[2] - b[2])) > THRESHOLD]
            changed = len(patch)
            figure = sum(1 for p in pb if abs(impact_flash.luma(*p) - median) > figure_contrast.MERGE_BAND)
            colour = tuple(round(statistics.fmean(p[k] for p in patch), 1) for k in range(3)) if patch else None
            colours.append(colour)
            rows.append(dict(shot=name, unit=unit["name"], changed=changed, figure=figure, share=round(changed / max(figure, 1), 4), patch_rgb=colour))
        pairs = separation(colours)
        for row in rows:
            if row["shot"] == name:
                row["separation"], row["nearest"] = (round(pairs[0], 1), round(pairs[1], 1)) if pairs else (None, None)
    return rows


def separation_summary(rows):
    """(mean patch pixels per copy, mean separation, nearest pair in any shot) of one class's rows; the separation is per shot, so count each shot once."""
    per_shot = {row["shot"]: (row["separation"], row["nearest"]) for row in rows}
    means = [value[0] for value in per_shot.values() if value[0] is not None]
    nearest = [value[1] for value in per_shot.values() if value[1] is not None]
    return (statistics.fmean(row["changed"] for row in rows), statistics.fmean(means) if means else 0.0, min(nearest) if nearest else 0.0)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--with", dest="with_runs", nargs="+", required=True)
    parser.add_argument("--without", dest="without_runs", nargs="+", required=True)
    parser.add_argument("--max", type=float, default=0.15)
    parser.add_argument("--json", metavar="FILE")
    parser.add_argument("--max-worst", type=float, help="the limit for the largest single hero view (default 1.5 x --max)")
    parser.add_argument("--classes", help="comma-separated classes to measure (default all 13; every named class must exist)")
    parser.add_argument("--min-separation", type=float, default=0.0, help="fail a class whose copies' mean patch colours are closer than this Delta E (default: report only)")
    args = parser.parse_args(argv)
    with_runs, without_runs = manifests(args.with_runs), manifests(args.without_runs)
    classes, shots = expected_classes(), expected_shots()
    if args.classes:
        wanted = args.classes.split(",")
        unknown = [name for name in wanted if name not in classes]
        if unknown:
            parser.error("unknown class: " + ", ".join(unknown))
        classes = wanted
    result, failed, missing = {}, [], []
    for cls in classes:
        fixture = f"acc-{cls}-root"
        found = problems(cls, with_runs.get(fixture, (None, None))[1], without_runs.get(fixture, (None, None))[1], shots)
        if found:
            missing += found
            continue
        rows = measure(with_runs[fixture], without_runs[fixture], cls.capitalize(), shots)
        shares = [r["share"] for r in rows]
        patch, apart, nearest = separation_summary(rows)
        result[cls] = dict(units=len(rows), mean=round(statistics.fmean(shares), 4), worst=max(shares), patch_px=round(patch, 1), separation=round(apart, 1), nearest=round(nearest, 1), rows=rows)
        if over_limit(shares, args.max, args.max_worst) or apart < args.min_separation:
            failed.append(cls)
        print(f"{cls:10} {len(rows):3} measurements  accent share of the figure: mean {statistics.fmean(shares) * 100:5.1f}%  worst {max(shares) * 100:5.1f}%"
              f"   patch {patch:5.0f} px per copy  copies apart: mean dE {apart:5.1f}  nearest {nearest:5.1f}")
    for line in missing:
        print("MISSING " + line, file=sys.stderr)
    if args.json:
        Path(args.json).write_text(json.dumps(result, indent=1) + "\n")
    if failed:
        print("over the limit: " + ", ".join(failed), file=sys.stderr)
    return 1 if failed or missing or not result else 0


if __name__ == "__main__":
    sys.exit(main())
