#!/usr/bin/env python3
"""The duplicate-identity accent sheet (#150 batch 4): parties with repeated classes, in the game's own render.

Reads gallery runs of the fixtures `dup-four-fighters-*` and `dup-two-pairs-*` (Project.Tools.CaptureFixtures), rendered with
tools/capture/palette-poses.json, and crops the party out of the `battle-yaw000-default` shot of each (the union of the party heroes' boxes in the
manifest, with a margin), one row per fixture. Four fighters of the same build differ only by the party-slot accent; two pairs show a class twice
with two subclasses. ImageMagick 7 (`magick`) does the cropping and layout.

    python3 tools/duplicate_sheet.py RUN [RUN ...] --out dup.png
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

SHOT = "battle-yaw000-default"
ORDER = [f"{name}-{board}" for name in ("dup-four-fighters", "dup-two-pairs") for board in ("root", "vigil", "vault")]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("run", nargs="+")
    parser.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    exe = shutil.which("magick")
    if not exe:
        raise SystemExit("ImageMagick 7 (magick) is required")
    found = {}
    for manifest_path in sorted(path for run in args.run for path in Path(run).rglob("manifest.json")):
        manifest = json.loads(manifest_path.read_text())
        shot = next((s for s in manifest.get("shots", []) if s.get("name") == SHOT and s.get("status") == "captured"), None)
        if shot and manifest.get("fixture") in ORDER:
            party = [u["box"] for u in shot["units"] if u.get("side", "Party") != "Enemy" and u.get("role") not in (None, "Foe")]
            if party:
                found[manifest["fixture"]] = (manifest_path.parent / shot["file"], party)
    out = Path(args.out)
    work = out.with_suffix("")
    work.mkdir(parents=True, exist_ok=True)
    tiles = []
    for name in ORDER:
        if name not in found:
            continue
        path, boxes = found[name]
        x0, y0 = min(b["x0"] for b in boxes), min(b["y0"] for b in boxes)
        x1, y1 = max(b["x1"] for b in boxes), max(b["y1"] for b in boxes)
        pad_x, pad_y = max(40, (x1 - x0) // 4), max(30, (y1 - y0) // 4)
        geometry = f"{x1 - x0 + 2 * pad_x}x{y1 - y0 + 2 * pad_y}+{max(0, x0 - pad_x)}+{max(0, y0 - pad_y)}"
        tile = work / f"{name}.png"
        subprocess.run([exe, str(path), "-crop", geometry, "+repage", "-scale", "300%", "-gravity", "northwest", "-fill", "white", "-pointsize", "28", "-annotate", "+10+8", name, str(tile)], check=True)
        tiles.append(str(tile))
    if not tiles:
        print("No duplicate-party shots found.", file=sys.stderr)
        return 2
    subprocess.run([exe, *tiles, "-background", "#23262e", "-append", str(out)], check=True)
    print(f"wrote {out} ({len(tiles)} fixtures)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
