#!/usr/bin/env python3
"""The duplicate-identity accent sheet (#150 batch 4): parties with repeated classes, in the game's own render.

Reads gallery runs of the fixtures `dup-four-fighters-*`, `dup-four-paladins-*`, `dup-four-rogues-*` and `dup-two-pairs-*` (Project.Tools.CaptureFixtures), rendered with
tools/capture/palette-poses.json, and crops the party out of the `battle-yaw000-default` shot of each (or `--shot`) (the union of the party heroes' boxes in the
manifest, with a margin), one row per party with its boards (Root, Vigil, Vault) side by side. Four fighters of the same build differ only by the party-slot accent; two pairs show a class twice
with two subclasses. ImageMagick 7 (`magick`) does the cropping and layout.

**Incomplete evidence fails.** Every requested party (all four, or `--classes`) on every board (Root, Vigil, Vault) must have a captured shot with exactly four party
heroes in it, exactly once across the runs; anything missing is listed on stderr, nothing is rendered and the exit code is 1.

    python3 tools/duplicate_sheet.py RUN [RUN ...] --out dup.png [--shot battle-yawm90-default] [--classes dup-four-rogues,dup-four-paladins]
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

SHOT = "battle-yaw000-default"
BOARDS = ("root", "vigil", "vault")
PARTIES = ("dup-four-fighters", "dup-four-paladins", "dup-four-rogues", "dup-two-pairs")
ORDER = [f"{name}-{board}" for name in PARTIES for board in BOARDS]
HEROES = 4


def party_boxes(shot):
    """The footprint boxes of the party heroes in a captured shot (enemies and foes excluded)."""
    return [u["box"] for u in shot.get("units", []) if u.get("side", "Party") != "Enemy" and u.get("role") not in (None, "Foe")]


def collect(runs, shot_name, parties):
    """({fixture: (png path, party boxes)}, [problems]) for every requested party x board.

    A fixture is a problem when no run captured the shot, when more than one run did (the sheet would silently pick one), or when it does not show exactly HEROES party heroes."""
    captured, seen = {}, {}
    for manifest_path in sorted(path for run in runs for path in Path(run).rglob("manifest.json")):
        manifest = json.loads(manifest_path.read_text())
        name = manifest.get("fixture")
        if name not in ORDER or name.rsplit("-", 1)[0] not in parties:
            continue
        seen.setdefault(name, []).append(manifest_path)
        shot = next((s for s in manifest.get("shots", []) if s.get("name") == shot_name and s.get("status") == "captured"), None)
        if shot:
            captured.setdefault(name, []).append((manifest_path.parent / shot["file"], party_boxes(shot)))
    found, problems = {}, []
    for name in (n for n in ORDER if n.rsplit("-", 1)[0] in parties):
        if name not in seen:
            problems.append(f"{name}: no run has this fixture")
        elif name not in captured:
            problems.append(f"{name}: shot {shot_name} was not captured")
        elif len(captured[name]) > 1:
            problems.append(f"{name}: shot {shot_name} is captured in {len(captured[name])} runs; pass one run per fixture")
        elif len(captured[name][0][1]) != HEROES:
            problems.append(f"{name}: {shot_name} shows {len(captured[name][0][1])} party heroes, expected {HEROES}")
        else:
            found[name] = captured[name][0]
    return found, problems


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("run", nargs="+")
    parser.add_argument("--out", required=True)
    parser.add_argument("--shot", default=SHOT, help="the shot to crop (default %(default)s; battle-yawm90-default is the game's home view, from the side)")
    parser.add_argument("--classes", help="comma-separated parties to include, from " + ", ".join(PARTIES) + " (default all)")
    parser.add_argument("--scale", type=int, default=300, help="enlargement in percent (default %(default)s)")
    args = parser.parse_args(argv)
    parties = args.classes.split(",") if args.classes else list(PARTIES)
    unknown = [name for name in parties if name not in PARTIES]
    if unknown:
        parser.error("unknown party: " + ", ".join(unknown) + " (choose from " + ", ".join(PARTIES) + ")")
    found, problems = collect(args.run, args.shot, parties)
    if problems:
        for line in problems:
            print("MISSING " + line, file=sys.stderr)
        print(f"{len(problems)} of {len(parties) * len(BOARDS)} fixtures are missing or incomplete; nothing was rendered.", file=sys.stderr)
        return 1
    exe = shutil.which("magick")
    if not exe:
        raise SystemExit("ImageMagick 7 (magick) is required")
    out = Path(args.out)
    work = out.with_suffix("")
    work.mkdir(parents=True, exist_ok=True)
    rows = {}
    for name in ORDER:
        if name not in found:
            continue
        path, boxes = found[name]
        x0, y0 = min(b["x0"] for b in boxes), min(b["y0"] for b in boxes)
        x1, y1 = max(b["x1"] for b in boxes), max(b["y1"] for b in boxes)
        pad_x, pad_y = max(40, (x1 - x0) // 4), max(30, (y1 - y0) // 4)
        geometry = f"{x1 - x0 + 2 * pad_x}x{y1 - y0 + 2 * pad_y}+{max(0, x0 - pad_x)}+{max(0, y0 - pad_y)}"
        tile = work / f"{name}.png"
        subprocess.run([exe, str(path), "-crop", geometry, "+repage", "-scale", f"{args.scale}%", "-gravity", "northwest", "-fill", "white", "-pointsize", "28", "-annotate", "+10+8", name, str(tile)], check=True)
        rows.setdefault(name.rsplit("-", 1)[0], []).append(str(tile))
    # One row per party (its boards side by side), the rows stacked.
    strips = []
    for party, tiles in rows.items():
        strip = work / f"row-{party}.png"
        subprocess.run([exe, *tiles, "-background", "#23262e", "-gravity", "north", "+append", str(strip)], check=True)
        strips.append(str(strip))
    subprocess.run([exe, *strips, "-background", "#23262e", "-gravity", "west", "-append", str(out)], check=True)
    print(f"wrote {out} ({sum(len(t) for t in rows.values())} fixtures)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
