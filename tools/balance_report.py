"""Summarize a balance-harness result folder (#179). Python 3.11+, no dependencies.

The Unreal tests `Balance.Baseline.*` write one subfolder per group (runs.csv, encounters.csv,
heroes.csv, group.json). This merges every group under the folder into summary.json and
summary.md. Definitions follow docs/runs/gameplay-depth/07-balance-baseline-contract.md.
"""

import argparse
import csv
import json
import math
from collections import Counter, defaultdict
from pathlib import Path
import statistics
import sys

GROUP_FILES = ("runs.csv", "encounters.csv", "heroes.csv", "group.json")
RESOURCES = ("kit", "slot", "wind", "maneuver", "rage")
OUTLIER_Z = 3.0
WIN_RATE_GAP = 0.15
RARE = 0.10


def wilson(successes, trials, z=1.96):
    """95% Wilson score interval for a proportion, as (low, high)."""
    if trials <= 0:
        return (0.0, 1.0)
    p = successes / trials
    denominator = 1 + z * z / trials
    centre = (p + z * z / (2 * trials)) / denominator
    half = z * math.sqrt(p * (1 - p) / trials + z * z / (4 * trials * trials)) / denominator
    return (max(0.0, centre - half), min(1.0, centre + half))


def robust_z(values):
    """|x - median| / (1.4826 * MAD) per value; falls back to the mean absolute deviation, then 0."""
    if not values:
        return []
    median = statistics.median(values)
    deviations = [abs(v - median) for v in values]
    scale = 1.4826 * statistics.median(deviations)
    if scale == 0:
        scale = 1.2533 * statistics.fmean(deviations)
    return [0.0 if scale == 0 else (v - median) / scale for v in values]


def read_csv(path):
    with path.open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def load(folder):
    """Rows from every group folder under `folder`, tagged with their group."""
    folder = Path(folder)
    groups = sorted(p.parent for p in folder.glob("*/group.json"))
    if not groups:
        raise ValueError(f"No balance groups (*/group.json) under {folder}; run the Balance.Baseline. tests first")
    data = {"groups": [], "runs": [], "encounters": [], "heroes": []}
    for group in groups:
        missing = [name for name in GROUP_FILES if not (group / name).is_file()]
        if missing:
            raise ValueError(f"Group {group.name} is missing {', '.join(missing)}")
        meta = json.loads((group / "group.json").read_text(encoding="utf-8"))
        data["groups"].append(meta)
        for kind in ("runs", "encounters", "heroes"):
            for row in read_csv(group / f"{kind}.csv"):
                row["group"] = group.name
                data[kind].append(row)
    return data


def number(row, key):
    return int(row[key]) if row.get(key, "") != "" else 0


def rate(successes, trials):
    low, high = wilson(successes, trials)
    return {"n": trials, "wins": successes, "rate": successes / trials if trials else 0.0, "low": low, "high": high}


def mean(values):
    return statistics.fmean(values) if values else 0.0


def run_key(row):
    return (row["party"], row["seed"])


def summarize(data):
    runs = data["runs"]
    fights = defaultdict(list)
    for row in data["encounters"]:
        fights[run_key(row)].append(row)
    victory = lambda row: row["outcome"] == "Victory"

    def run_block(rows):
        block = rate(sum(1 for r in rows if victory(r)), len(rows))
        played = [len(fights[run_key(r)]) for r in rows]
        block.update({
            "stalls": sum(1 for r in rows if r["outcome"] == "Stall"),
            "fights_won": mean([number(r, "encounters_won") for r in rows]),
            "rounds_per_fight": sum(number(r, "rounds") for r in rows) / max(1, sum(played)),
            "deaths_per_run": mean([number(r, "deaths") for r in rows]),
            "damage_per_run": mean([number(r, "damage") for r in rows]),
            "taken_per_run": mean([number(r, "damage_taken") for r in rows]),
            "healing_per_run": mean([number(r, "healing") for r in rows]),
            "protection_per_run": mean([number(r, "protection") for r in rows]),
            "specials_per_run": mean([number(r, "specials_used") for r in rows]),
            "potions_per_run": mean([number(r, "potions_used") for r in rows]),
        })
        return block

    summary = {"groups": [g["group"] for g in data["groups"]], "policy_version": sorted({g.get("policy_version") for g in data["groups"]}),
               "seconds": sum(g.get("seconds", 0) for g in data["groups"]), "overall": run_block(runs)}
    summary["gates"] = {key: sum(number(r, key) for r in runs) for key in ("policy_errors", "illegal_proposals", "passive_ends")}
    summary["parties"] = {party: dict(run_block(rows), archetype=rows[0]["archetype"], builds=rows[0]["builds"])
                          for party, rows in group_by(runs, "party").items()}
    summary["archetypes"] = {name: run_block(rows) for name, rows in group_by(runs, "archetype").items()}

    rotation = [r for r in runs if r["archetype"] == "rotation"]
    rotation_keys = {run_key(r) for r in rotation}
    rotation_rate = summary["archetypes"].get("rotation", {}).get("rate", 0.0)
    heroes = [h for h in data["heroes"] if run_key(h) in rotation_keys]
    summary["rotation_win_rate"] = rotation_rate

    def present(hero):
        """Fights this hero started: all fights of the run, or up to the one it died in."""
        played = len(fights[run_key(hero)])
        return min(played, number(hero, "death_stage")) if hero["survived"] == "0" and number(hero, "death_stage") else played

    run_rows = {run_key(r): r for r in runs}

    def hero_block(rows):
        party_runs = {run_key(h): h["outcome"] for h in rows}
        block = rate(sum(1 for outcome in party_runs.values() if outcome == "Victory"), len(party_runs))
        # How fast the parties containing these heroes finish their fights (lower is faster).
        played = sum(len(fights[key]) for key in party_runs) or 1
        block["party_rounds_per_fight"] = sum(number(run_rows[key], "rounds") for key in party_runs if key in run_rows) / played
        fights_present = sum(present(h) for h in rows) or 1
        turns = sum(number(h, "turns") for h in rows) or 1
        block.update({
            "heroes": len(rows),
            "death_rate": sum(1 for h in rows if h["survived"] == "0") / len(rows),
            "victory_deaths": sum(1 for h in rows if h["death_cause"] == "victory"),
            "deadline_deaths": sum(1 for h in rows if h["death_cause"] == "deadline"),
            "damage_per_fight": sum(number(h, "damage") for h in rows) / fights_present,
            "kills_per_fight": sum(number(h, "kills") for h in rows) / fights_present,
            "healing_per_fight": sum(number(h, "healing") for h in rows) / fights_present,
            "protection_per_fight": sum(number(h, "protection") for h in rows) / fights_present,
            "taken_per_fight": sum(number(h, "damage_taken") for h in rows) / fights_present,
            "downs_per_fight": sum(number(h, "downs") for h in rows) / fights_present,
            "idle_turn_rate": sum(number(h, "idle_turns") for h in rows) / turns,
        })
        return block

    summary["classes"] = {name: hero_block(rows) for name, rows in group_by(heroes, "class").items()}
    summary["slots"] = {name: hero_block(rows) for name, rows in group_by(heroes, "slot").items()}
    kits = {}
    for build, rows in group_by(heroes, "build").items():
        block = hero_block(rows)
        block["class"] = rows[0]["class"]
        turns = sum(number(h, "turns") for h in rows) or 1
        specials = []
        for k in (1, 2):
            command = rows[0][f"s{k}_command"]
            if not command:
                continue
            available = sum(number(h, f"s{k}_available_turns") for h in rows)
            uses = sum(number(h, f"s{k}_uses") for h in rows)
            blockers = Counter()
            for h in rows:
                if h[f"s{k}_top_blocker"]:
                    blockers[h[f"s{k}_top_blocker"]] += number(h, f"s{k}_top_blocker_turns")
            specials.append({"command": command, "available_rate": available / turns, "uses_per_run": uses / len(rows),
                             "use_when_available": uses / available if available else 0.0, "uses": uses,
                             "top_blocker": blockers.most_common(1)[0][0] if blockers else ""})
        block["specials"] = specials
        winners = [h for h in rows if h["outcome"] == "Victory" and h["survived"] == "1"]
        block["left_at_victory"] = {res: {"mean": mean([number(h, f"{res}_left") for h in winners]), "cap": number(rows[0], f"{res}_cap")}
                                    for res in RESOURCES if number(rows[0], f"{res}_cap") > 0}
        block["potions_left_at_victory"] = mean([number(h, "potions_left") for h in winners])
        kits[build] = block
    summary["kits"] = kits
    summary["outliers"] = outliers(kits, rotation_rate)
    summary["signatures"] = signatures(kits)
    summary["encounters"] = encounter_blocks(data["encounters"], fights)
    summary["stalls"] = [{"party": r["party"], "seed": r["seed"], "reason": r["stall_reason"]} for r in runs if r["outcome"] == "Stall"]
    summary["long_fights"] = long_fights(data["encounters"])
    deaths = sorted((h for h in data["heroes"] if h["survived"] == "0"), key=lambda h: (h["party"], int(h["seed"]), int(h["slot"])))
    summary["deaths"] = {"count": len(deaths), "causes": dict(Counter(h["death_cause"] for h in deaths)),
                         "list": [{k: h[k] for k in ("party", "seed", "slot", "build", "death_stage", "death_cause", "outcome")} for h in deaths[:40]]}
    return summary


def group_by(rows, key):
    result = defaultdict(list)
    for row in rows:
        result[row[key]].append(row)
    return dict(sorted(result.items()))


def outliers(kits, rotation_rate):
    found = []
    names = list(kits)
    for metric in ("rate", "party_rounds_per_fight", "death_rate", "damage_per_fight", "healing_per_fight", "protection_per_fight"):
        for name, z in zip(names, robust_z([kits[n][metric] for n in names])):
            if abs(z) >= OUTLIER_Z:
                found.append({"kit": name, "metric": metric, "value": kits[name][metric], "z": z})
    for name in names:
        kit = kits[name]
        if abs(kit["rate"] - rotation_rate) >= WIN_RATE_GAP and not kit["low"] <= rotation_rate <= kit["high"]:
            found.append({"kit": name, "metric": "win_rate_gap", "value": kit["rate"], "z": None})
    return found


def signatures(kits):
    flags = []
    for name, kit in kits.items():
        for special in kit["specials"]:
            if special["uses"] == 0:
                flags.append({"kit": name, "command": special["command"], "flag": "never used", **special})
            elif special["available_rate"] < RARE and special["top_blocker"] != "ResourceShort":
                flags.append({"kit": name, "command": special["command"], "flag": "rarely available", **special})
            elif special["use_when_available"] < RARE:
                flags.append({"kit": name, "command": special["command"], "flag": "rarely used when available", **special})
    return flags


def encounter_blocks(rows, fights):
    blocks = {}
    for (stage, route), group in sorted(group_by_pair(rows, "stage", "route").items()):
        finished = [r for r in group if r["outcome"] in ("Victory", "Defeat")]
        hp_lost = [(number(r, "hp_start") - number(r, "hp_end")) / max(1, number(r, "hp_max")) for r in finished]
        closest = [number(r, "hp_low") / max(1, number(r, "hp_max")) for r in finished]
        blocks[f"{stage}-{route}"] = dict(rate(sum(1 for r in finished if r["outcome"] == "Victory"), len(finished)), **{
            "stage": int(stage), "route": route,
            "rounds": mean([number(r, "rounds") for r in finished]),
            "hp_lost_fraction": mean(hp_lost),
            "hp_low_fraction": mean(closest),
            "hp_low_min": min(closest) if closest else 0.0,
            "downs": mean([number(r, "downs") for r in finished]),
            "deaths": mean([number(r, "deaths") for r in finished]),
            "conscious_start": mean([number(r, "conscious_start") for r in group]),
        })
    return blocks


def group_by_pair(rows, a, b):
    result = defaultdict(list)
    for row in rows:
        result[(row[a], row[b])].append(row)
    return result


def long_fights(rows, limit=10):
    finished = sorted((number(r, "rounds") for r in rows))
    if not finished:
        return {"p99": 0, "fights": []}
    p99 = finished[min(len(finished) - 1, math.ceil(0.99 * len(finished)) - 1)]
    longest = sorted((r for r in rows if number(r, "rounds") >= p99), key=lambda r: (-number(r, "rounds"), r["party"], int(r["seed"]), int(r["stage"])))
    return {"p99": p99, "count": len(longest), "fights": [{k: r[k] for k in ("party", "seed", "stage", "route", "recipe", "rounds", "outcome")} for r in longest[:limit]]}


def percent(value):
    return f"{100 * value:.0f}%"


def interval(block):
    return f"{percent(block['rate'])} ({percent(block['low'])}–{percent(block['high'])})"


def markdown(summary):
    lines = ["# Balance baseline summary", ""]
    o = summary["overall"]
    lines += [f"Groups: {', '.join(summary['groups'])}. Policy version {summary['policy_version']}. {o['n']} runs in {summary['seconds']:.1f} s.", "",
              f"Overall win rate {interval(o)}; {o['stalls']} stalls; {o['fights_won']:.2f} fights won per run; {o['rounds_per_fight']:.1f} rounds per fight; {o['deaths_per_run']:.2f} deaths per run.", "",
              f"Gates: policy errors {summary['gates']['policy_errors']}, illegal proposals {summary['gates']['illegal_proposals']}, passive ends {summary['gates']['passive_ends']}.", ""]
    lines += ["## Parties", "", "| Party | Archetype | Win rate (95% CI) | Fights won | Rounds/fight | Deaths/run | Damage | Taken | Healing | Protection | Specials | Potions |", "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |"]
    for name, p in summary["parties"].items():
        if p["archetype"] == "rotation":
            continue
        lines.append(f"| {name} | {p['archetype']} | {interval(p)} | {p['fights_won']:.2f} | {p['rounds_per_fight']:.1f} | {p['deaths_per_run']:.2f} | {p['damage_per_run']:.0f} | {p['taken_per_run']:.0f} | {p['healing_per_run']:.0f} | {p['protection_per_run']:.0f} | {p['specials_per_run']:.1f} | {p['potions_per_run']:.1f} |")
    lines += ["", "## Archetypes", "", "| Archetype | Runs | Win rate (95% CI) | Fights won | Rounds/fight | Deaths/run |", "| --- | --- | --- | --- | --- | --- |"]
    for name, a in summary["archetypes"].items():
        lines.append(f"| {name} | {a['n']} | {interval(a)} | {a['fights_won']:.2f} | {a['rounds_per_fight']:.1f} | {a['deaths_per_run']:.2f} |")
    hero_header = "| Win rate (95% CI) | Party rounds/fight | Death rate | Damage/fight | Kills/fight | Healing/fight | Protection/fight | Taken/fight | Idle turns |"
    hero_rule = "| --- | --- | --- | --- | --- | --- | --- | --- | --- |"
    def hero_cells(h):
        return f"{interval(h)} | {h['party_rounds_per_fight']:.2f} | {percent(h['death_rate'])} | {h['damage_per_fight']:.1f} | {h['kills_per_fight']:.2f} | {h['healing_per_fight']:.1f} | {h['protection_per_fight']:.1f} | {h['taken_per_fight']:.1f} | {percent(h['idle_turn_rate'])}"
    lines += ["", f"## Classes (rotation; win rate counts each run containing the class once; rotation mean {percent(summary['rotation_win_rate'])})", "", "| Class | Heroes " + hero_header, "| --- | --- " + hero_rule]
    for name, c in summary["classes"].items():
        lines.append(f"| {name} | {c['heroes']} | {hero_cells(c)} |")
    lines += ["", "## Slots (rotation; enemies attack the first legal target in roster order)", "", "| Slot | Heroes " + hero_header, "| --- | --- " + hero_rule]
    for name, c in summary["slots"].items():
        lines.append(f"| {name} | {c['heroes']} | {hero_cells(c)} |")
    lines += ["", "## Kits (rotation)", "", "| Kit | Runs | Win rate (95% CI) | Party rounds/fight | Death rate | Damage/fight | Healing/fight | Protection/fight | Idle | Special 1 avail / uses per run / top blocker | Special 2 avail / uses per run / top blocker | Left at victory |", "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |"]
    for name, k in sorted(summary["kits"].items(), key=lambda item: (-item[1]["rate"], item[0])):
        cells = []
        for i in range(2):
            if i < len(k["specials"]):
                s = k["specials"][i]
                cells.append(f"{s['command']} {percent(s['available_rate'])} / {s['uses_per_run']:.2f} / {s['top_blocker'] or '-'}")
            else:
                cells.append("-")
        left = ", ".join(f"{res} {v['mean']:.1f}/{v['cap']}" for res, v in k["left_at_victory"].items()) or "-"
        left += f", potions {k['potions_left_at_victory']:.1f}"
        lines.append(f"| {name} | {k['n']} | {interval(k)} | {k['party_rounds_per_fight']:.2f} | {percent(k['death_rate'])} | {k['damage_per_fight']:.1f} | {k['healing_per_fight']:.1f} | {k['protection_per_fight']:.1f} | {percent(k['idle_turn_rate'])} | {cells[0]} | {cells[1]} | {left} |")
    lines += ["", "## Encounters by stage and route", "", "| Stage | Route | Fights | Win rate | Rounds | HP lost | Lowest party HP (mean / worst) | Downs | Deaths | Heroes standing at start |", "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |"]
    for e in summary["encounters"].values():
        lines.append(f"| {e['stage']} | {e['route']} | {e['n']} | {interval(e)} | {e['rounds']:.1f} | {percent(e['hp_lost_fraction'])} | {percent(e['hp_low_fraction'])} / {percent(e['hp_low_min'])} | {e['downs']:.2f} | {e['deaths']:.2f} | {e['conscious_start']:.2f} |")
    lines += ["", "## Outliers", ""]
    lines += [f"- {o['kit']}: {o['metric']} {o['value']:.3f}" + (f" (robust z {o['z']:.1f})" if o["z"] is not None else "") for o in summary["outliers"]] or ["None."]
    lines += ["", "## Special signatures", ""]
    lines += [f"- {s['kit']} {s['command']}: {s['flag']} (available {percent(s['available_rate'])}, {s['uses_per_run']:.2f} uses/run, {percent(s['use_when_available'])} of available turns, top blocker {s['top_blocker'] or '-'})" for s in summary["signatures"]] or ["None."]
    d = summary["deaths"]
    lines += ["", f"## Deaths ({d['count']}: " + ", ".join(f"{cause or 'unknown'} {count}" for cause, count in sorted(d["causes"].items())) + ")", ""]
    lines += [f"- {h['party']} seed {h['seed']} slot {h['slot']} {h['build']}: fight {h['death_stage']}, {h['death_cause']} (run {h['outcome']})" for h in d["list"]] or ["None."]
    lines += ["", "## Stalls", ""]
    lines += [f"- {s['party']} seed {s['seed']}: {s['reason']}" for s in summary["stalls"]] or ["None."]
    lf = summary["long_fights"]
    lines += ["", f"## Long fights (99th percentile: {lf['p99']} rounds; {lf.get('count', 0)} fights at or above)", ""]
    lines += [f"- {f['party']} seed {f['seed']} stage {f['stage']} ({f['route']}, recipe {f['recipe']}): {f['rounds']} rounds, {f['outcome']}" for f in lf["fights"]] or ["None."]
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folder", help="Result folder, e.g. .local/balance/<stamp>")
    args = parser.parse_args(argv)
    try:
        summary = summarize(load(args.folder))
    except (ValueError, OSError, KeyError) as error:
        print(f"balance_report: {error}", file=sys.stderr)
        return 2
    folder = Path(args.folder)
    (folder / "summary.json").write_text(json.dumps(summary, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    (folder / "summary.md").write_text(markdown(summary), encoding="utf-8")
    o = summary["overall"]
    print(f"{o['n']} runs, win rate {interval(o)}, {o['stalls']} stalls -> {folder / 'summary.md'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
