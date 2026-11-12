#!/usr/bin/env python3
"""The caster bolt tint strip (#150 batch 4): every caster's spell bolt in flight, side by side, in the game's own render.

Reads gallery runs made with tools/capture/bolt-strip-poses.json (`bolt-<class>-flight-near` impact shots, a bolt half way along its path at near
zoom) on fixtures that hold each class: `default-seed-battle` (wizard, cleric), `cast3-default-seed-battle` (sorcerer, warlock, druid) and
`cast4-default-seed-battle` (bard). The manifest records each bolt's projected box; this crops it with a margin and lays the crops out in a fixed
order with the class name under each. ImageMagick 7 (`magick`) does the cropping and layout.

    python3 tools/bolt_strip.py RUN [RUN ...] --out strip.png
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ORDER = ("wizard", "cleric", "sorcerer", "warlock", "druid", "bard")
MARGIN = 0.15
CELL = (260, 260)


def find(runs):
    """{class: (image path, box)} for each captured bolt-<class>-flight-near shot; the first run holding a class wins."""
    found = {}
    for manifest_path in sorted(path for run in runs for path in Path(run).rglob("manifest.json")):
        manifest = json.loads(manifest_path.read_text())
        for shot in manifest.get("shots", []):
            match = re.match(r"^bolt-(\w+)-flight-near$", shot.get("name", ""))
            box = (shot.get("target") or {}).get("bolt")
            if match and shot.get("status") == "captured" and box and match.group(1) not in found:
                found[match.group(1)] = (manifest_path.parent / shot["file"], box)
    return found


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("run", nargs="+")
    parser.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    exe = shutil.which("magick")
    if not exe:
        raise SystemExit("ImageMagick 7 (magick) is required")
    found = find(args.run)
    missing = [name for name in ORDER if name not in found]
    if missing:
        print("missing bolts: " + ", ".join(missing), file=sys.stderr)
        return 2
    out = Path(args.out)
    work = out.with_suffix("")
    work.mkdir(parents=True, exist_ok=True)
    tiles = []
    for name in ORDER:
        path, box = found[name]
        w, h = box["x1"] - box["x0"], box["y1"] - box["y0"]
        geometry = f"{int(w * (1 + 2 * MARGIN))}x{int(h * (1 + 2 * MARGIN))}+{max(0, int(box['x0'] - w * MARGIN))}+{max(0, int(box['y0'] - h * MARGIN))}"
        tile = work / f"{name}.png"
        subprocess.run([exe, str(path), "-crop", geometry, "+repage", "-resize", f"{CELL[0]}x{CELL[1]}", "-background", "#23262e", "-gravity", "center", "-extent",
                        f"{CELL[0]}x{CELL[1]}", "-gravity", "south", "-fill", "white", "-pointsize", "20", "-annotate", "+0+6", name, str(tile)], check=True)
        tiles.append(str(tile))
    subprocess.run([exe, *tiles, "-background", "#23262e", "+append", str(out)], check=True)
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
