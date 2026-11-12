#!/usr/bin/env python3
"""The duplicate-identity accent sheet (#150 batch 4): parties with repeated classes, in the game's own render.

Reads gallery runs of the fixtures `dup-four-fighters-*`, `dup-four-paladins-*`, `dup-four-rogues-*` and `dup-two-pairs-*` (Project.Tools.CaptureFixtures), rendered with
tools/capture/palette-poses.json, and crops the party out of the `battle-yaw000-default` shot of each (or `--shot`) (the union of the party heroes' boxes in the
manifest, with a margin), one row per party with its boards (Root, Vigil, Vault) side by side. Four fighters of the same build differ only by the party-slot accent; two pairs show a class twice
with two subclasses. ImageMagick 7 (`magick`) does the cropping and layout.

    python3 tools/duplicate_sheet.py RUN [RUN ...] --out dup.png [--shot battle-yawm90-default] [--classes dup-four-rogues,dup-four-paladins]
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

SHOT = "battle-yaw000-default"
ORDER = [f"{name}-{board}" for name in ("dup-four-fighters", "dup-four-paladins", "dup-four-rogues", "dup-two-pairs") for board in ("root", "vigil", "vault")]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("run", nargs="+")
    parser.add_argument("--out", required=True)
    parser.add_argument("--shot", default=SHOT, help="the shot to crop (default %(default)s; battle-yawm90-default is the game's home view, from the side)")
    parser.add_argument("--classes", help="comma-separated fixture names to include, for example dup-four-rogues,dup-four-paladins (default all)")
    parser.add_argument("--scale", type=int, default=300, help="enlargement in percent (default %(default)s)")
    args = parser.parse_args(argv)
    exe = shutil.which("magick")
    if not exe:
        raise SystemExit("ImageMagick 7 (magick) is required")
    only = set(args.classes.split(",")) if args.classes else None
    found = {}
    for manifest_path in sorted(path for run in args.run for path in Path(run).rglob("manifest.json")):
        manifest = json.loads(manifest_path.read_text())
        shot = next((s for s in manifest.get("shots", []) if s.get("name") == args.shot and s.get("status") == "captured"), None)
        if shot and manifest.get("fixture") in ORDER and (only is None or manifest["fixture"].rsplit("-", 1)[0] in only):
            party = [u["box"] for u in shot["units"] if u.get("side", "Party") != "Enemy" and u.get("role") not in (None, "Foe")]
            if party:
                found[manifest["fixture"]] = (manifest_path.parent / shot["file"], party)
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
    if not rows:
        print("No duplicate-party shots found.", file=sys.stderr)
        return 2
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
