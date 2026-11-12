#!/usr/bin/env python3
"""Per-build readability table for the subclass palettes (#150 batch 4), from a `gallery` run of the palette sweep fixtures.

The fixtures `pal-NN-vigil|root|vault` (written by `Project.Tools.CaptureFixtures`, listed in Saved/CaptureFixtures/palette-parties.json) give every
catalog build a place on the three cast boards (Drowned Vigil, Root of the World, the Vault) in a party of four distinct classes, so a build is the
only hero of its class and `tools/figure_contrast.py --role <Class>` measures exactly that build. For each build this report takes the default-zoom
shots (the four yaws and the targeting shot, 5 per board, 15 in all) and prints the gate of the rigged cast (#150, rigged Rim 1.2 / Fill 1.0):

* the mean region contrast over all its shots at least 2.3,
* the worst single shot at least 1.4,
* every per-board mean (Vigil, Root, Vault) at least 2.0.

A build that appears as a filler in another party is measured only where it is a real member. **A build passes only when every expected build x board x
shot was measured** (3 boards x the 5 default-zoom shots of tools/capture/palette-poses.json = 15): a missing capture, a skipped shot or a missing build
fails with the list of what is missing, never passes on the measurements that happen to exist. Standard library only.

    python3 tools/dev.py gallery --fixture pal-00-root ... --poses tools/capture/palette-poses.json --res 1920x1080
    python3 tools/palette_report.py .local/gallery/<run> --markdown out.md --json out.json
    python3 tools/palette_report.py .local/gallery/<run> --builds fighter.champion,fighter.psi-warrior
    python3 tools/palette_report.py .local/gallery/<run> --duplicates fighter,paladin,rogue

`--duplicates CLASSES` reports the same gate for the duplicate-accent parties instead: the fixtures `dup-four-<class>s-vigil|root|vault` (four copies of one build, each
carrying its party-slot accent), every copy of the class counted in every default-zoom shot (3 boards x 5 shots x 4 copies = 60 measurements per class; anything fewer fails).
"""
import argparse
import collections
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
POSES = Path(__file__).resolve().parent / "capture" / "palette-poses.json"


def expected_shots():
    """The default-zoom shots the gate counts: the shots of the palette poses file whose name says -default (four yaws and the targeting shot)."""
    return sorted(shot["name"] for shot in json.loads(POSES.read_text())["shots"] if "-default" in shot["name"])


DUPLICATE_FIXTURE = re.compile(r"^dup-four-([a-z]+)s-(vigil|root|vault)$")
COPIES = 4


def duplicate_measurements(runs, classes):
    """[(class, board, shot, unit, contrast, merge)] for every copy of the class in the `dup-four-<class>s-<board>` fixtures of the runs."""
    found = []
    for manifest_path in sorted(path for run in runs for path in Path(run).rglob("manifest.json")):
        manifest = json.loads(manifest_path.read_text())
        match = DUPLICATE_FIXTURE.match(manifest.get("fixture", ""))
        if not match or match.group(1) not in classes:
            continue
        for r in figure_contrast.measure_manifest(manifest_path, match.group(1).capitalize()):
            found.append((match.group(1), match.group(2), r["shot"], r["unit"], r["contrastRatio"], r["mergeFraction"]))
    return found


def aggregate_duplicates(measurements, classes, shots=None, copies=COPIES):
    """{class: row} with the same floors as `aggregate`, over every copy: each class needs boards x shots x copies measurements, each exactly once."""
    shots = list(shots) if shots is not None else expected_shots()
    rows = {}
    for cls in classes:
        mine = [m for m in measurements if m[0] == cls]
        counts = collections.Counter((m[1], m[2], m[3]) for m in mine)
        expected = len(BOARDS) * len(shots) * copies
        failures = []
        boards = {name: [m[4] for m in mine if m[1] == name] for name in BOARDS}
        per = {name: collections.Counter(m[2] for m in mine if m[1] == name) for name in BOARDS}
        missing = [f"{name}/{shot}" for name in BOARDS for shot in shots if per[name][shot] != copies]
        if len(mine) != expected or missing or any(count > 1 for count in counts.values()):
            failures.append(f"{len(mine)} of {expected} measurements" + (f" (wrong count in {', '.join(missing[:4])}{', ...' if len(missing) > 4 else ''})" if missing else ""))
        mean = round(statistics.fmean(m[4] for m in mine), 3) if mine else None
        worst = min((m[4] for m in mine), default=None)
        board_means = {name: round(statistics.fmean(values), 3) if values else None for name, values in boards.items()}
        if mine:
            for ok, reason in ((mean >= MEAN_FLOOR, f"mean {mean:.2f} < {MEAN_FLOOR}"), (worst >= WORST_FLOOR, f"worst {worst:.2f} < {WORST_FLOOR}"),
                               *((value is None or value >= BOARD_FLOOR, f"{name} mean {value} < {BOARD_FLOOR}") for name, value in board_means.items())):
                if not ok:
                    failures.append(reason)
        rows[cls] = {"build": f"dup-four-{cls}s", "mean": mean, "worst": worst, "boardMeans": board_means, "measurements": len(mine),
                     "mergeMean": round(statistics.fmean(m[5] for m in mine), 4) if mine else None, "failures": failures, "pass": not failures}
    return rows


def role_of(build):
    return build.split(".")[0].capitalize()


def real_members(parties):
    """{party id: {role: build}}: the builds of each party that are not fillers."""
    members = {}
    for party in parties["parties"]:
        fillers = set(party.get("filler", []))
        members[party["id"]] = {role_of(b): b for b in party["builds"] if b not in fillers}
    return members


def aggregate(measurements, expected_builds=None, shots=None):
    """measurements: [(build, board, shot, contrast, merge)] -> {build: row} with the gate applied.

    With expected_builds (and shots, default the palette poses' default-zoom shots) every expected build x board x shot must have been measured exactly once;
    an expected build with no measurement at all gets a failing row. Without expected_builds only the builds seen are checked (the shots still must be complete)."""
    shots = list(shots) if shots is not None else expected_shots()
    rows = {}
    for build, board, shot, contrast, merge in measurements:
        row = rows.setdefault(build, {"build": build, "shots": [], "merge": [], "boards": {name: [] for name in BOARDS}, "seen": []})
        row["shots"].append(contrast)
        row["merge"].append(merge)
        row["boards"][board].append(contrast)
        row["seen"].append((board, shot))
    for build in expected_builds or ():
        rows.setdefault(build, {"build": build, "shots": [], "merge": [], "boards": {name: [] for name in BOARDS}, "seen": []})
    for row in rows.values():
        seen = collections.Counter(row["seen"])
        missing = [f"{board}/{shot}" for board in BOARDS for shot in shots if seen[(board, shot)] == 0]
        duplicated = [f"{board}/{shot}" for (board, shot), count in seen.items() if count > 1]
        row["missing"] = missing
        row["mean"] = round(statistics.fmean(row["shots"]), 3) if row["shots"] else None
        row["worst"] = min(row["shots"]) if row["shots"] else None
        row["boardMeans"] = {name: round(statistics.fmean(values), 3) if values else None for name, values in row["boards"].items()}
        row["mergeMean"] = round(statistics.fmean(row["merge"]), 4) if row["merge"] else None
        failures = []
        if missing:
            failures.append(f"missing {len(missing)} of {len(BOARDS) * len(shots)} measurements ({', '.join(missing[:4])}{', ...' if len(missing) > 4 else ''})")
        if duplicated:
            failures.append("measured more than once: " + ", ".join(duplicated[:4]))
        if row["shots"]:
            for ok, reason in ((row["mean"] >= MEAN_FLOOR, f"mean {row['mean']:.2f} < {MEAN_FLOOR}"), (row["worst"] >= WORST_FLOOR, f"worst {row['worst']:.2f} < {WORST_FLOOR}"),
                               *((value is None or value >= BOARD_FLOOR, f"{name} mean {value} < {BOARD_FLOOR}") for name, value in row["boardMeans"].items())):
                if not ok:
                    failures.append(reason)
        row["failures"] = failures
        row["pass"] = not failures
        del row["boards"], row["seen"]
    return rows


def collect(runs, parties, only=None):
    members = real_members(parties)
    measurements = []
    for manifest_path in sorted(path for run in runs for path in Path(run).rglob("manifest.json")):
        manifest = json.loads(manifest_path.read_text())
        match = FIXTURE.match(manifest.get("fixture", ""))
        if not match or match.group(1) not in members:
            continue
        party, board = match.groups()
        for role, build in members[party].items():
            if only and build not in only:
                continue
            for r in figure_contrast.measure_manifest(manifest_path, role):
                measurements.append((build, board, r["shot"], r["contrastRatio"], r["mergeFraction"]))
    return measurements


def markdown(rows):
    lines = ["| Build | Mean | Worst | Vigil | Root | Vault | Merge | Gate |", "| --- | --- | --- | --- | --- | --- | --- | --- |"]
    for build in sorted(rows):
        r = rows[build]
        cells = [f"{r['boardMeans'][name]:.2f}" if r["boardMeans"][name] is not None else "-" for name in BOARDS]
        mean = f"{r['mean']:.2f}" if r["mean"] is not None else "-"
        worst = f"{r['worst']:.2f}" if r["worst"] is not None else "-"
        merge = f"{r['mergeMean'] * 100:.1f}%" if r["mergeMean"] is not None else "-"
        lines.append(f"| `{build}` | {mean} | {worst} | " + " | ".join(cells) + f" | {merge} | {'pass' if r['pass'] else 'FAIL: ' + '; '.join(r['failures'])} |")
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("run", nargs="+", help="gallery run folder(s) (one `gallery` run per fixture is fine: every manifest below each is read)")
    parser.add_argument("--parties", default=str(PARTIES), help="palette-parties.json written by the fixture test")
    parser.add_argument("--builds", help="comma-separated catalog build ids to report and require (default: every real member of every party in palette-parties.json)")
    parser.add_argument("--json", metavar="FILE")
    parser.add_argument("--markdown", metavar="FILE")
    parser.add_argument("--duplicates", metavar="CLASSES", help="comma-separated classes: report the duplicate-accent parties dup-four-<class>s-* instead of the per-build sweep")
    args = parser.parse_args(argv)
    if args.duplicates:
        classes = args.duplicates.split(",")
        rows = aggregate_duplicates(duplicate_measurements(args.run, classes), classes)
        if not any(r["measurements"] for r in rows.values()):
            print("No duplicate-party measurements found.", file=sys.stderr)
            return 2
        text = markdown(rows)
        print(text)
        if args.markdown:
            Path(args.markdown).write_text(text)
        if args.json:
            Path(args.json).write_text(json.dumps({"gate": {"mean": MEAN_FLOOR, "worst": WORST_FLOOR, "board": BOARD_FLOOR}, "duplicates": rows}, indent=2) + "\n")
        failed = [c for c, r in rows.items() if not r["pass"]]
        print(f"{len(rows)} duplicate parties measured, {len(failed)} below the gate" + (": " + ", ".join(sorted(failed)) if failed else ""))
        return 1 if failed else 0
    parties = json.loads(Path(args.parties).read_text())
    only = set(args.builds.split(",")) if args.builds else None
    expected = sorted(only or {build for members in real_members(parties).values() for build in members.values()})
    measurements = collect(args.run, parties, only)
    if not measurements:
        print("No palette sweep measurements found.", file=sys.stderr)
        return 2
    rows = aggregate(measurements, expected)
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
