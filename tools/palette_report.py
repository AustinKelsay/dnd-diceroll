#!/usr/bin/env python3
"""Per-build readability table for the subclass palettes (#150 batch 4), from a `gallery` run of the palette sweep fixtures.

The fixtures `pal-NN-vigil|root|vault` (written by `Project.Tools.CaptureFixtures`, listed in Saved/CaptureFixtures/palette-parties.json) give every
catalog build a place on the three cast boards (Drowned Vigil, Root of the World, the Vault) in a party of four distinct classes, so a build is the
only hero of its class and `tools/figure_contrast.py --role <Class>` measures exactly that build. For each build this report takes the default-zoom
shots (the four yaws and the targeting shot, 5 per board, 15 in all) and prints the gate of the rigged cast (#150, rigged Rim 1.2 / Fill 1.0):

* the mean region contrast over all its shots at least 2.3,
* the worst single shot at least 1.4,
* every per-board mean (Vigil, Root, Vault) at least 2.0.

A build that appears as a filler in another party is measured only where it is a real member. Standard library only.

    python3 tools/dev.py gallery --fixture pal-00-root ... --poses tools/capture/palette-poses.json --res 1920x1080
    python3 tools/palette_report.py .local/gallery/<run> --markdown out.md --json out.json
    python3 tools/palette_report.py .local/gallery/<run> --builds fighter.champion,fighter.psi-warrior
"""
import argparse
import json
from pathlib import Path
import re
import statistics
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import figure_contrast

BOARDS = ("vigil", "root", "vault")
MEAN_FLOOR, WORST_FLOOR, BOARD_FLOOR = 2.3, 1.4, 2.0
FIXTURE = re.compile(r"^(pal-\d+)-(vigil|root|vault)$")
PARTIES = Path("Saved/CaptureFixtures/palette-parties.json")


def role_of(build):
    return build.split(".")[0].capitalize()


def real_members(parties):
    """{party id: {role: build}}: the builds of each party that are not fillers."""
    members = {}
    for party in parties["parties"]:
        fillers = set(party.get("filler", []))
        members[party["id"]] = {role_of(b): b for b in party["builds"] if b not in fillers}
    return members


def aggregate(measurements):
    """measurements: [(build, board, contrast, merge)] -> {build: row} with the gate applied."""
    rows = {}
    for build, board, contrast, merge in measurements:
        row = rows.setdefault(build, {"build": build, "shots": [], "merge": [], "boards": {name: [] for name in BOARDS}})
        row["shots"].append(contrast)
        row["merge"].append(merge)
        row["boards"][board].append(contrast)
    for row in rows.values():
        row["mean"] = round(statistics.fmean(row["shots"]), 3)
        row["worst"] = min(row["shots"])
        row["boardMeans"] = {name: round(statistics.fmean(values), 3) if values else None for name, values in row["boards"].items()}
        row["mergeMean"] = round(statistics.fmean(row["merge"]), 4)
        row["failures"] = [reason for ok, reason in (
            (row["mean"] >= MEAN_FLOOR, f"mean {row['mean']:.2f} < {MEAN_FLOOR}"),
            (row["worst"] >= WORST_FLOOR, f"worst {row['worst']:.2f} < {WORST_FLOOR}"),
            *((value is None or value >= BOARD_FLOOR, f"{name} mean {value} < {BOARD_FLOOR}") for name, value in row["boardMeans"].items())) if not ok]
        row["pass"] = not row["failures"]
        del row["boards"]
    return rows


def collect(run, parties, only=None):
    members = real_members(parties)
    measurements = []
    for manifest_path in sorted(Path(run).rglob("manifest.json")):
        manifest = json.loads(manifest_path.read_text())
        match = FIXTURE.match(manifest.get("fixture", ""))
        if not match or match.group(1) not in members:
            continue
        party, board = match.groups()
        for role, build in members[party].items():
            if only and build not in only:
                continue
            for r in figure_contrast.measure_manifest(manifest_path, role):
                measurements.append((build, board, r["contrastRatio"], r["mergeFraction"]))
    return measurements


def markdown(rows):
    lines = ["| Build | Mean | Worst | Vigil | Root | Vault | Merge | Gate |", "| --- | --- | --- | --- | --- | --- | --- | --- |"]
    for build in sorted(rows):
        r = rows[build]
        cells = [f"{r['boardMeans'][name]:.2f}" if r["boardMeans"][name] is not None else "-" for name in BOARDS]
        lines.append(f"| `{build}` | {r['mean']:.2f} | {r['worst']:.2f} | " + " | ".join(cells) + f" | {r['mergeMean'] * 100:.1f}% | {'pass' if r['pass'] else 'FAIL: ' + '; '.join(r['failures'])} |")
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("run", help="a gallery run folder")
    parser.add_argument("--parties", default=str(PARTIES), help="palette-parties.json written by the fixture test")
    parser.add_argument("--builds", help="comma-separated catalog build ids to report (default all measured)")
    parser.add_argument("--json", metavar="FILE")
    parser.add_argument("--markdown", metavar="FILE")
    args = parser.parse_args(argv)
    parties = json.loads(Path(args.parties).read_text())
    measurements = collect(args.run, parties, set(args.builds.split(",")) if args.builds else None)
    if not measurements:
        print("No palette sweep measurements found.", file=sys.stderr)
        return 2
    rows = aggregate(measurements)
    text = markdown(rows)
    print(text)
    if args.markdown:
        Path(args.markdown).write_text(text)
    if args.json:
        Path(args.json).write_text(json.dumps({"gate": {"mean": MEAN_FLOOR, "worst": WORST_FLOOR, "board": BOARD_FLOOR}, "builds": rows}, indent=2) + "\n")
    failed = [b for b, r in rows.items() if not r["pass"]]
    print(f"{len(rows)} builds measured, {len(failed)} below the gate" + (": " + ", ".join(sorted(failed)) if failed else ""))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
