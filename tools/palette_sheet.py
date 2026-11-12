#!/usr/bin/env python3
"""The in-game palette contact sheet (#150 batch 4): every class, every subclass, in the game's own render.

Reads a `gallery` run of the palette sweep fixtures (`pal-NN-root`, rendered with tools/capture/palette-poses.json): the shot
`pose-<class>-idle-near` of each fixture holds the party's hero of that class in Idle, close. This crops that hero (the unit's projected box in
the manifest, with a margin), labels it with its build and lays the classes out as rows, subclasses as columns, in catalog order. ImageMagick 7
(`magick`) does the cropping and the layout; the crops are written next to the sheet. A build that is a filler in a party is taken from the party
where it is a real member.

    python3 tools/palette_sheet.py .local/gallery/<run> --out evidence/palette-sheet.png [--board root] [--no-palettes-run .local/gallery/<run2>]
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import palette_report

MARGIN = 0.28
CELL = (300, 460)


def magick(*args):
    exe = shutil.which("magick")
    if not exe:
        raise SystemExit("ImageMagick 7 (magick) is required for the contact sheet")
    subprocess.run([exe, *map(str, args)], check=True)


def crops(run, parties, board, out_dir):
    members = palette_report.real_members(parties)
    found = {}
    for manifest_path in sorted(Path(run).rglob("manifest.json")):
        manifest = json.loads(manifest_path.read_text())
        match = palette_report.FIXTURE.match(manifest.get("fixture", ""))
        if not match or match.group(2) != board or match.group(1) not in members:
            continue
        for shot in manifest.get("shots", []):
            m = re.match(r"^pose-(\w+)-idle-near$", shot["name"])
            if not m or shot.get("status") != "captured":
                continue
            role = m.group(1).capitalize()
            build = members[match.group(1)].get(role)
            unit = next((u for u in shot.get("units", []) if u.get("role") == role and u.get("side", "Party") != "Enemy"), None)
            if not build or not unit:
                continue
            b = unit["box"]
            w, h = b["x1"] - b["x0"], b["y1"] - b["y0"]
            x0, y0 = max(0, int(b["x0"] - w * MARGIN)), max(0, int(b["y0"] - h * MARGIN * 0.5))
            geometry = f"{int(w * (1 + 2 * MARGIN))}x{int(h * (1 + MARGIN * 1.2))}+{x0}+{y0}"
            target = out_dir / f"{build}.png"
            magick(manifest_path.parent / shot["file"], "-crop", geometry, "+repage", "-resize", f"{CELL[0]}x{CELL[1]}", "-background", "#23262e", "-gravity", "center",
                   "-extent", f"{CELL[0]}x{CELL[1]}", "-gravity", "south", "-fill", "white", "-pointsize", "17", "-annotate", "+0+4", build.split(".", 1)[1], target)
            found[build] = target
    return found


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("run")
    parser.add_argument("--out", required=True)
    parser.add_argument("--parties", default=str(palette_report.PARTIES))
    parser.add_argument("--board", default="root", choices=palette_report.BOARDS)
    args = parser.parse_args(argv)
    parties = json.loads(Path(args.parties).read_text())
    out = Path(args.out)
    out_dir = out.with_suffix("")
    out_dir.mkdir(parents=True, exist_ok=True)
    found = crops(args.run, parties, args.board, out_dir)
    if not found:
        print("No pose-<class>-idle-near shots found in the pal-NN-" + args.board + " fixtures.", file=sys.stderr)
        return 2
    rows = []
    classes = []
    for party in parties["parties"]:
        for build in party["builds"]:
            if build not in party.get("filler", []) and build.split(".")[0] not in classes:
                classes.append(build.split(".")[0])
    for cls in classes:
        builds = sorted(b for b in found if b.startswith(cls + "."))
        if not builds:
            continue
        row = out_dir / f"row-{cls}.png"
        magick(*[found[b] for b in builds], "-background", "#23262e", "+append", "-gravity", "west", "-extent", f"{CELL[0] * 10}x{CELL[1]}", row)
        rows.append(row)
    magick(*rows, "-background", "#23262e", "-append", out)
    print(f"wrote {out} ({len(found)} builds, {len(rows)} classes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
