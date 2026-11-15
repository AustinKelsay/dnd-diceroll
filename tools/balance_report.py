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
# Results and summaries live only under the checkout's ignored .local/ folder.
LOCAL_ROOT = Path(__file__).resolve().parents[1] / ".local"
# Encounter recipes by id (Run/ExpeditionRun.cpp): 0-2 every revision's, 3-6 revision 9's pressure layouts and finale (#188).
LAYOUTS = {0: "split stones", 1: "ruined pillars", 2: "broken wall", 3: "fallen colonnade", 4: "sunken causeway", 5: "cairn field", 6: "vault sanctum"}
# Fights 1-2, 3-4 and 5, and the revision 9 risk word each stage and route shows (contract 11). Earlier revisions show no risk word;
# their rows still sort into the same tiers, so a before-run can be compared with an after-run.
STAGE_KINDS = {1: "approach", 2: "approach", 3: "pressure", 4: "pressure", 5: "finale"}
RISK = {("approach", "guarded"): "low", ("pressure", "guarded"): "moderate", ("finale", "guarded"): "high",
        ("approach", "dangerous"): "high", ("pressure", "dangerous"): "severe", ("finale", "dangerous"): "severe"}
RISK_ORDER = ("low", "moderate", "high", "severe")


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
    summary["gates"] = {key: sum(number(r, key) for r in runs) for key in ("policy_errors", "illegal_proposals", "passive_ends", "protection_mismatches")}
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
            # Sum every run's full blocker counts before choosing the dominant one.
            blockers = Counter()
            for h in rows:
                blockers.update(parse_blockers(h[f"s{k}_blockers"]))
            ranked = sorted(blockers.items(), key=lambda item: (-item[1], item[0]))
            specials.append({"command": command, "available_rate": available / turns, "uses_per_run": uses / len(rows),
                             "use_when_available": uses / available if available else 0.0, "uses": uses,
                             "top_blocker": ranked[0][0] if ranked else "", "blockers": dict(ranked)})
        block["specials"] = specials
        winners = [h for h in rows if h["outcome"] == "Victory" and h["survived"] == "1"]
        block["left_at_victory"] = {res: {"mean": mean([number(h, f"{res}_left") for h in winners]), "cap": number(rows[0], f"{res}_cap")}
                                    for res in RESOURCES if number(rows[0], f"{res}_cap") > 0}
        block["potions_left_at_victory"] = mean([number(h, "potions_left") for h in winners])
        kits[build] = block
    summary["kits"] = kits
    summary["outliers"] = outliers(kits, rotation_rate)
    summary["signatures"] = signatures(kits)
    summary["encounters"] = encounter_blocks(data["encounters"])
    summary["catalog_revisions"] = sorted({g.get("catalog_revision") for g in data["groups"] if g.get("catalog_revision") is not None})
    summary["layouts"] = layout_blocks(data["encounters"])
    summary["stage_kinds"] = stage_kind_blocks(data["encounters"])
    summary["specials_by_stage"] = specials_by_stage(data["encounters"])
    summary["risk"] = risk_blocks(data["encounters"])
    summary["stalls"] = [{"party": r["party"], "seed": r["seed"], "reason": r["stall_reason"]} for r in runs if r["outcome"] == "Stall"]
    summary["long_fights"] = long_fights(data["encounters"])
    deaths = sorted((h for h in data["heroes"] if h["survived"] == "0"), key=lambda h: (h["party"], int(h["seed"]), int(h["slot"])))
    summary["deaths"] = {"count": len(deaths), "causes": dict(Counter(h["death_cause"] for h in deaths)),
                         "list": [{k: h[k] for k in ("party", "seed", "slot", "build", "death_stage", "death_cause", "outcome")} for h in deaths[:40]]}
    return summary


def parse_blockers(text):
    """'ResourceShort:12;OutOfRange:3' -> Counter; empty text -> empty Counter."""
    counts = Counter()
    for part in filter(None, text.split(";")):
        name, _, count = part.partition(":")
        if not name or not count.isdigit():
            raise ValueError(f"Bad blocker count {part!r}")
        counts[name] += int(count)
    return counts


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


def encounter_blocks(rows):
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


def fight_block(group):
    """Finished fights of `group`: count, defeats, rounds, HP lost (net of healing), HP taken (gross damage taken), both as a share of
    the party's HP at the start of the fight, and the melee share of party damage."""
    finished = [r for r in group if r["outcome"] in ("Victory", "Defeat")]
    damage = sum(number(r, "damage") for r in finished)
    return dict(rate(sum(1 for r in finished if r["outcome"] == "Defeat"), len(finished)), **{
        "rounds": mean([number(r, "rounds") for r in finished]),
        "hp_lost_fraction": mean([(number(r, "hp_start") - number(r, "hp_end")) / max(1, number(r, "hp_max")) for r in finished]),
        "hp_taken_fraction": mean([number(r, "damage_taken") / max(1, number(r, "hp_max")) for r in finished]),
        "melee_share": sum(number(r, "melee_damage") for r in finished) / damage if damage else 0.0,
    })


def specials_by_stage(rows):
    """Specials used per fight at each stage (#218 target P5, #187 item 3): every fight that started, whatever its outcome."""
    return {stage: {"fights": len(group), "specials_per_fight": mean([number(r, "specials_used") for r in group])}
            for stage, group in sorted(group_by(rows, "stage").items(), key=lambda item: int(item[0]))}


def layout_blocks(rows):
    """Per recipe and route (#188 target E2): fights, defeat share, rounds, HP lost, melee share of damage."""
    blocks = {}
    for (recipe, route), group in sorted(group_by_pair(rows, "recipe", "route").items(), key=lambda item: (int(item[0][0]), item[0][1])):
        stages = sorted({int(r["stage"]) for r in group})
        blocks[f"{recipe}-{route}"] = dict(fight_block(group), recipe=int(recipe), layout=LAYOUTS.get(int(recipe), f"recipe {recipe}"), route=route, stages=stages)
    return blocks


def stage_kind_blocks(rows):
    """Approach, pressure and finale per route (#188 target E1)."""
    groups = defaultdict(list)
    for r in rows:
        groups[(STAGE_KINDS.get(int(r["stage"]), "other"), r["route"])].append(r)
    order = {"approach": 0, "pressure": 1, "finale": 2}
    return {f"{kind}-{route}": dict(fight_block(group), kind=kind, route=route)
            for (kind, route), group in sorted(groups.items(), key=lambda item: (item[0][1], order.get(item[0][0], 3)))}


def risk_blocks(rows):
    """Fights by the risk word their stage and route show, and whether every higher word costs more HP and loses more often (#188 E3)."""
    groups = defaultdict(list)
    for r in rows:
        word = RISK.get((STAGE_KINDS.get(int(r["stage"]), ""), r["route"]))
        if word:
            groups[word].append(r)
    tiers = {word: dict(fight_block(groups[word]), risk=word) for word in RISK_ORDER if groups[word]}
    pairs = []
    words = [w for w in RISK_ORDER if w in tiers]
    for i, low in enumerate(words):
        for high in words[i + 1:]:
            pairs.append({"lower": low, "higher": high,
                          "costs_more": tiers[high]["hp_lost_fraction"] > tiers[low]["hp_lost_fraction"],
                          "takes_more": tiers[high]["hp_taken_fraction"] > tiers[low]["hp_taken_fraction"],
                          "loses_more": tiers[high]["rate"] > tiers[low]["rate"]})
    return {"tiers": tiers, "pairs": pairs, "ordered": all(p["costs_more"] and p["loses_more"] for p in pairs),
            "ordered_taken": all(p["takes_more"] and p["loses_more"] for p in pairs)}


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
    revisions = summary.get("catalog_revisions") or ["unrecorded"]
    lines += [f"Groups: {', '.join(summary['groups'])}. Policy version {summary['policy_version']}. Catalog revision {', '.join(str(r) for r in revisions)}. {o['n']} runs in {summary['seconds']:.1f} s.", "",
              f"Overall win rate {interval(o)}; {o['stalls']} stalls; {o['fights_won']:.2f} fights won per run; {o['rounds_per_fight']:.1f} rounds per fight; {o['deaths_per_run']:.2f} deaths per run.", "",
              f"Gates: policy errors {summary['gates']['policy_errors']}, illegal proposals {summary['gates']['illegal_proposals']}, passive ends {summary['gates']['passive_ends']}, protection mismatches {summary['gates']['protection_mismatches']}.", ""]
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
    lines += ["", "## Stages by route (fights 1-2 approach, 3-4 pressure, 5 finale; HP lost is net of healing, HP taken gross, both of the party's HP)", "",
              "| Stage | Route | Fights | Defeats | Rounds | HP lost | HP taken | Melee share of damage |", "| --- | --- | --- | --- | --- | --- | --- | --- |"]
    for e in summary["stage_kinds"].values():
        lines.append(f"| {e['kind']} | {e['route']} | {e['n']} | {100 * e['rate']:.1f}% | {e['rounds']:.1f} | {percent(e['hp_lost_fraction'])} | {percent(e['hp_taken_fraction'])} | {percent(e['melee_share'])} |")
    lines += ["", "## Specials per fight by stage", "", "| Stage | Fights | Specials per fight |", "| --- | --- | --- |"]
    for stage, e in summary["specials_by_stage"].items():
        lines.append(f"| {stage} | {e['fights']} | {e['specials_per_fight']:.2f} |")
    lines += ["", "## Layouts by route", "", "| Recipe | Layout | Route | Fights (stages) | Defeats | Rounds | HP lost | HP taken | Melee share of damage |", "| --- | --- | --- | --- | --- | --- | --- | --- | --- |"]
    for e in summary["layouts"].values():
        lines.append(f"| {e['recipe']} | {e['layout']} | {e['route']} | {e['n']} ({', '.join(str(s) for s in e['stages'])}) | {100 * e['rate']:.1f}% | {e['rounds']:.1f} | {percent(e['hp_lost_fraction'])} | {percent(e['hp_taken_fraction'])} | {percent(e['melee_share'])} |")
    risk = summary["risk"]
    lines += ["", "## Risk words (revision 9 table by stage and route)", "", "| Risk | Fights | Defeats | HP lost | HP taken | Rounds |", "| --- | --- | --- | --- | --- | --- |"]
    for t in risk["tiers"].values():
        lines.append(f"| {t['risk']} | {t['n']} | {100 * t['rate']:.1f}% | {percent(t['hp_lost_fraction'])} | {percent(t['hp_taken_fraction'])} | {t['rounds']:.1f} |")
    def verdict(ordered, key, what):
        def failing(p):
            return " and ".join(([] if p[key] else [f"{what} no more"]) + ([] if p["loses_more"] else ["loses no more often"]))
        return "yes" if ordered else "no, " + "; ".join(f"{p['higher']} vs {p['lower']} {failing(p)}" for p in risk["pairs"] if not (p[key] and p["loses_more"]))
    lines += ["", "Every higher word costs more HP (net) and loses more often: " + verdict(risk["ordered"], "costs_more", "costs") + ".",
              "", "Every higher word takes more HP (gross) and loses more often: " + verdict(risk["ordered_taken"], "takes_more", "takes") + "."]
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


# --------------------------------------------------------------------------------------------------------------------------------------
# Slice 5 (#218, docs/runs/gameplay-depth/13-progression-contract.md section 6): CSV comparisons and the paired statistics.

KEYS = {"runs.csv": ("party", "seed"), "encounters.csv": ("party", "seed", "stage"), "heroes.csv": ("party", "seed", "slot")}
# The columns this slice appends, after every existing one.
APPENDED = {"runs.csv": (), "encounters.csv": (), "heroes.csv": ("reward_cards", "reserve", "honed")}
BOOTSTRAP_SEED = 20261112
RESAMPLES = 10000
CLEAR_POINTS = 5.0
STRATEGIES = ("gear", "reserve", "honed")


def read_table(path):
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        return list(reader.fieldnames or []), list(reader)


def load_tables(folder):
    """{file: (header, {key: row})} merged over the groups under `folder`, and {group: group.json}. Refuses duplicate keys and headers that differ."""
    folder = Path(folder)
    groups = sorted(p.parent for p in folder.glob("*/group.json"))
    if not groups:
        raise ValueError(f"No balance groups (*/group.json) under {folder}")
    tables, metas = {}, {}
    for group in groups:
        metas[group.name] = json.loads((group / "group.json").read_text(encoding="utf-8"))
        for name, key in KEYS.items():
            header, rows = read_table(group / name)
            known = tables.setdefault(name, (header, {}))
            if known[0] != header:
                raise ValueError(f"{group.name}/{name}: header differs from the other groups")
            for row in rows:
                k = tuple(row[c] for c in key)
                if k in known[1]:
                    raise ValueError(f"{name}: duplicate row {k}")
                known[1][k] = row
    return tables, metas


def compare(base, candidate, mode, limit=20):
    """Contract 13 section 6 comparisons. mode "c1": candidate (this build's revision 9 before-run) against base (#214's retained revision 9
    after-run): every column of base's header equal; the appended columns hold only Gear claims. mode "c2": candidate (revision 10, gear) against
    base (the before-run): every column of all three files equal. Returns a list of differences; empty means the comparison passed."""
    if mode not in ("c1", "c2"):
        raise ValueError(f"Unknown comparison {mode}")
    (base_tables, base_meta), (cand_tables, cand_meta) = load_tables(base), load_tables(candidate)
    problems = []
    for name in KEYS:
        base_header, base_rows = base_tables[name]
        cand_header, cand_rows = cand_tables[name]
        if mode == "c1":
            if cand_header[:len(base_header)] != base_header or tuple(cand_header[len(base_header):]) != APPENDED[name]:
                problems.append(f"{name}: header is not the base header plus {list(APPENDED[name])}")
                continue
            columns = base_header
        else:
            if cand_header != base_header:
                problems.append(f"{name}: headers differ")
                continue
            columns = base_header
        for k in sorted(set(base_rows) - set(cand_rows)):
            problems.append(f"{name}: row {k} missing from the candidate")
        for k in sorted(set(cand_rows) - set(base_rows)):
            problems.append(f"{name}: extra row {k} in the candidate")
        for k in sorted(set(base_rows) & set(cand_rows)):
            for column in columns:
                if base_rows[k][column] != cand_rows[k][column]:
                    problems.append(f"{name} {k} {column}: {base_rows[k][column]!r} != {cand_rows[k][column]!r}")
    if mode == "c1" and not any("header" in p for p in problems):
        problems += gear_only(cand_tables)
    excluded = {"seconds", "policy_version", "reward_strategy"} if mode == "c1" else {"seconds", "catalog_revision"}
    if set(base_meta) != set(cand_meta):
        problems.append(f"group.json: groups differ ({sorted(base_meta)} vs {sorted(cand_meta)})")
    for group in sorted(set(base_meta) & set(cand_meta)):
        a, b = base_meta[group], cand_meta[group]
        for key in sorted((set(a) | set(b)) - excluded):
            if a.get(key) != b.get(key):
                problems.append(f"group.json {group} {key}: {a.get(key)!r} != {b.get(key)!r}")
        if mode == "c2":
            if a.get("reward_strategy") != "gear" or b.get("reward_strategy") != "gear":
                problems.append(f"group.json {group}: reward_strategy must be gear in both")
            if a.get("catalog_revision") == b.get("catalog_revision"):
                problems.append(f"group.json {group}: the catalog revisions should differ (9 -> 10)")
    return problems[:limit] if limit else problems


def cards_of(row):
    return [c for c in row.get("reward_cards", "").split(";") if c]


def gear_only(tables):
    """C1's appended columns: every hero holds G once per reward claimed and no flag; per run, one claim for each of victories 1-4."""
    problems = []
    runs = tables["runs.csv"][1]
    claims = defaultdict(int)
    for k, row in tables["heroes.csv"][1].items():
        cards = cards_of(row)
        if any(c != "G" for c in cards) or row.get("reserve") != "0" or row.get("honed") != "0":
            problems.append(f"heroes.csv {k}: not gear-only ({row.get('reward_cards')!r}, reserve {row.get('reserve')}, honed {row.get('honed')})")
        claims[k[:2]] += len(cards)
    for k, row in runs.items():
        if claims[k] != min(4, number(row, "encounters_won")):
            problems.append(f"runs.csv {k}: {claims[k]} claims for {row['encounters_won']} victories")
    return problems


def check_cards(folder, gear_folder):
    """The new fields under a card strategy: one card per claim, at most one R and one H, flags that agree with the cards, and end-of-run
    caps equal to the build caps (the paired gear run's) plus Reserve."""
    tables, _ = load_tables(folder)
    gear, _ = load_tables(gear_folder)
    problems, claims = [], defaultdict(int)
    for k, row in tables["heroes.csv"][1].items():
        cards = cards_of(row)
        claims[k[:2]] += len(cards)
        if any(c not in ("G", "R", "H") for c in cards) or cards.count("R") > 1 or cards.count("H") > 1:
            problems.append(f"heroes.csv {k}: cards {row['reward_cards']!r}")
        if row["reserve"] != ("1" if "R" in cards else "0") or row["honed"] != ("1" if "H" in cards else "0"):
            problems.append(f"heroes.csv {k}: flags disagree with the cards")
        base = gear["heroes.csv"][1].get(k)
        if base is None:
            problems.append(f"heroes.csv {k}: no paired gear row")
            continue
        extra = 1 if row["reserve"] == "1" else 0
        for res in RESOURCES:
            cap = number(base, f"{res}_cap")
            if number(row, f"{res}_cap") != (cap + extra if cap else 0):
                problems.append(f"heroes.csv {k}: {res}_cap {row[f'{res}_cap']} is not the build's {cap} plus Reserve")
    for k, row in tables["runs.csv"][1].items():
        if claims[k] != min(4, number(row, "encounters_won")):
            problems.append(f"runs.csv {k}: {claims[k]} claims for {row['encounters_won']} victories")
    return problems


def percentile(values, q):
    """Linear interpolation between closest ranks; with an infinite neighbour, the lower one for q < 0.5 and the upper otherwise."""
    ordered = sorted(values)
    position = q * (len(ordered) - 1)
    low, high = math.floor(position), math.ceil(position)
    a, b = ordered[low], ordered[high]
    if low == high or a == b:
        return a
    if not (math.isfinite(a) and math.isfinite(b)):
        return a if q < 0.5 else b
    return a + (b - a) * (position - low)


def resamples(seeds, count=RESAMPLES, seed=BOOTSTRAP_SEED):
    """The seed-cluster resamples: `count` draws of len(seeds) seeds with replacement, from one fixed random.Random."""
    import random
    rng = random.Random(seed)
    seeds = sorted(seeds)
    return [rng.choices(seeds, k=len(seeds)) for _ in range(count)]


def paired_difference(a, b, keys, draws):
    """Mean of (a - b) over the runs `keys` (party, seed), in points when a and b are 0/1, with its 95% percentile interval over the draws."""
    per_seed = defaultdict(lambda: [0.0, 0])
    for key in keys:
        cell = per_seed[key[1]]
        cell[0] += a[key] - b[key]
        cell[1] += 1
    total = sum(c[0] for c in per_seed.values()); n = sum(c[1] for c in per_seed.values())
    point = 100.0 * total / n if n else 0.0
    values = []
    for draw in draws:
        t = sum(per_seed[s][0] for s in draw if s in per_seed); m = sum(per_seed[s][1] for s in draw if s in per_seed)
        values.append(100.0 * t / m if m else 0.0)
    return {"d": point, "low": percentile(values, 0.025), "high": percentile(values, 0.975), "n": n}


def verdict(diff):
    """clear: interval excludes 0 and |d| >= 5 points; small: excludes 0, |d| < 5; tie: contains 0."""
    if diff["low"] > 0 or diff["high"] < 0:
        return "clear" if abs(diff["d"]) >= CLEAR_POINTS else "small"
    return "tie"


def threshold(interval, limit, above_is_met):
    """met / missed / inconclusive for an interval against a threshold (inconclusive when it contains the threshold)."""
    low, high = interval
    if low <= limit <= high:
        return "inconclusive"
    met = low > limit if above_is_met else high < limit
    return "met" if met else "missed"


def ratio_interval(num, den, draws, limit):
    """Ratio of two per-seed means (num and den map seed -> (sum, count)) with the sign-safe rule of contract 13 section 6. Returns the point
    outcome, the interval and the count of resamples decided by the sign rule."""
    def decide(n_sum, n_count, d_sum, d_count):
        if not n_count or not d_count:
            return None
        D, G = n_sum / n_count, d_sum / d_count
        if G > 0:
            return D / G if D > 0 else -math.inf
        return math.inf if D > 0 else -math.inf
    def totals(table, seeds):
        return sum(table[s][0] for s in seeds if s in table), sum(table[s][1] for s in seeds if s in table)
    point = decide(*totals(num, num.keys()), *totals(den, den.keys()))
    outcomes, signed = [], 0
    for draw in draws:
        value = decide(*totals(num, draw), *totals(den, draw))
        if value is None:
            value = -math.inf
        if not math.isfinite(value):
            signed += 1
        outcomes.append(value)
    low, high = percentile(outcomes, 0.025), percentile(outcomes, 0.975)
    return {"ratio": point, "low": low, "high": high, "sign_decided": signed, "limit": limit}


def hold(low, high, lower_limit=None, upper_limit=None):
    """A hold with an interval: met when the interval lies inside the limits, missed when it lies outside, borderline when it straddles one."""
    inside = (lower_limit is None or low >= lower_limit) and (upper_limit is None or high <= upper_limit)
    outside = (lower_limit is not None and high < lower_limit) or (upper_limit is not None and low > upper_limit)
    return "met" if inside else "missed" if outside else "borderline"


def load_measurement(folders):
    """The rows of one measurement, pooled over its batches (folders)."""
    merged = {"runs": [], "encounters": [], "heroes": [], "groups": []}
    for folder in folders:
        data = load(folder)
        for key in merged:
            merged[key] += data[key]
    return merged


def wins(data):
    return {run_key(r): 1 if r["outcome"] == "Victory" else 0 for r in data["runs"]}


def class_runs(data):
    """Rotation runs by class: a run counts once for each class it contains."""
    result = defaultdict(set)
    for h in data["heroes"]:
        if h["archetype"] == "rotation":
            result[h["class"]].add(run_key(h))
    return dict(sorted(result.items()))


def best_assignment(candidates, needed=2):
    """Whether each strategy can be the best for `needed` classes, giving each class (a list of possible strategies) to at most one."""
    classes = list(candidates)
    def search(i, counts):
        if all(counts[s] >= needed for s in STRATEGIES):
            return True
        if i == len(classes):
            return False
        for choice in list(candidates[classes[i]]) + [None]:
            if choice is not None:
                counts[choice] += 1
            if search(i + 1, counts):
                return True
            if choice is not None:
                counts[choice] -= 1
        return False
    return search(0, Counter())


def targets(measurements, draws=None):
    """Judge P1-P6 of contract 13 section 6. measurements: {"before", "gear", "reserve", "honed", "mixed"} -> pooled data. P1 (comparison C2)
    is passed in by the caller as measurements["c2"] (a list of differences)."""
    data = {name: measurements[name] for name in ("before", "gear", "reserve", "honed", "mixed")}
    won = {name: wins(d) for name, d in data.items()}
    keys = set(won["before"])
    for name, w in won.items():
        if set(w) != keys:
            raise ValueError(f"{name}: its runs are not the before-run's (party, seed) pairs")
    seeds = sorted({k[1] for k in keys})
    draws = draws if draws is not None else resamples(seeds)
    rotation = {run_key(r) for r in data["before"]["runs"] if r["archetype"] == "rotation"}
    result = {"seeds": len(seeds), "runs": len(keys), "resamples": len(draws)}
    # P2 and dominance: per class, every pair of single-card strategies.
    classes = class_runs(data["before"])
    per_class, best, optimistic = {}, Counter(), {}
    beaten = {s: [] for s in STRATEGIES}
    for cls, runs in classes.items():
        pairs = {}
        for a in STRATEGIES:
            for b in STRATEGIES:
                if a < b:
                    diff = paired_difference(won[a], won[b], runs, draws)
                    diff["verdict"] = verdict(diff)
                    pairs[f"{a}-{b}"] = diff
        def against(a, b):
            diff = pairs[f"{a}-{b}"] if a < b else pairs[f"{b}-{a}"]
            sign = 1 if a < b else -1
            return diff["verdict"], sign * diff["d"]
        winner, possible = None, []
        for s in STRATEGIES:
            others = [against(s, t) for t in STRATEGIES if t != s]
            if all(v == "clear" and d > 0 for v, d in others):
                winner = s
            if all(v == "tie" or d > 0 for v, d in others):
                possible.append(s)
            for t in STRATEGIES:
                if t != s and against(t, s)[0] == "clear" and against(t, s)[1] > 0:
                    beaten[s].append(cls)
        if winner:
            best[winner] += 1
        optimistic[cls] = [winner] if winner else possible
        per_class[cls] = {"pairs": pairs, "best": winner, "possible": possible, "runs": len(runs)}
    met = all(best[s] >= 2 for s in STRATEGIES)
    p2 = "met" if met else "inconclusive" if best_assignment(optimistic) else "missed"
    result["P2"] = {"verdict": p2, "best": {s: best[s] for s in STRATEGIES}, "classes": per_class}
    result["dominance"] = {s: sorted(set(beaten[s])) for s in STRATEGIES}
    result["non_dominance"] = all(beaten[s] for s in STRATEGIES)
    # P3: the largest paired gap between the rotation win rates of the three single-card strategies.
    gaps = {}
    for a in STRATEGIES:
        for b in STRATEGIES:
            if a < b:
                diff = paired_difference(won[a], won[b], rotation, draws)
                low, high = diff["low"], diff["high"]
                diff["verdict"] = "inconclusive" if (low <= 10 <= high or low <= -10 <= high) else "met" if -10 < low and high < 10 else "missed"
                gaps[f"{a}-{b}"] = diff
    verdicts = [g["verdict"] for g in gaps.values()]
    result["P3"] = {"verdict": "missed" if "missed" in verdicts else "inconclusive" if "inconclusive" in verdicts else "met",
                    "largest": max(abs(g["d"]) for g in gaps.values()), "pairs": gaps}
    # P4: the rise of mixed over the before-run, and the slice 3/4 holds on mixed.
    rise = paired_difference(won["mixed"], won["before"], rotation, draws)
    rise["verdict"] = threshold((rise["low"], rise["high"]), 8.0, above_is_met=False)
    holds = slice_holds(data["mixed"], draws)
    hold_verdicts = [h["verdict"] for h in holds.values()]
    p4 = "missed" if rise["verdict"] == "missed" or "missed" in hold_verdicts else \
        "inconclusive" if rise["verdict"] == "inconclusive" or "borderline" in hold_verdicts else "met"
    result["P4"] = {"verdict": p4, "rise": rise, "holds": holds}
    # P5: specials per fight in fights 4-5 under reserve against the before-run.
    def specials(d):
        table = defaultdict(lambda: [0.0, 0])
        for r in d["encounters"]:
            if r["stage"] in ("4", "5"):
                table[r["seed"]][0] += number(r, "specials_used"); table[r["seed"]][1] += 1
        return {s: tuple(v) for s, v in table.items()}
    p5 = ratio_interval(specials(data["reserve"]), specials(data["before"]), draws, 1.5)
    p5["verdict"] = threshold((p5["low"], p5["high"]), 1.5, above_is_met=True)
    result["P5"] = p5
    # P6: gates zero and no more stalls than the before-run (same seed same results is Project.Balance.SameSeedSameResults).
    def gates(d):
        return {k: sum(number(r, k) for r in d["runs"]) for k in ("policy_errors", "illegal_proposals", "passive_ends", "protection_mismatches")}
    stalls = {name: sum(1 for r in d["runs"] if r["outcome"] == "Stall") for name, d in data.items()}
    gate_totals = {name: gates(d) for name, d in data.items()}
    p6 = all(v == 0 for g in gate_totals.values() for v in g.values()) and all(stalls[n] <= stalls["before"] for n in stalls)
    result["P6"] = {"verdict": "met" if p6 else "missed", "gates": gate_totals, "stalls": stalls}
    c2 = measurements.get("c2")
    result["P1"] = {"verdict": "met" if c2 == [] else "missed" if c2 else "not run", "differences": c2 or []}
    result["rotation"] = {name: {"win_rate": sum(won[name][k] for k in rotation) / max(1, len(rotation))} for name in won}
    result["extra_batch_needed"] = any(result[p]["verdict"] == "inconclusive" for p in ("P2", "P3", "P4", "P5"))
    return result


def slice_holds(d, draws):
    """The slice 3/4 targets #188 met (contract 13 section 6), on one measurement."""
    rotation_runs = [r for r in d["runs"] if r["archetype"] == "rotation"]
    rot_wins = sum(1 for r in rotation_runs if r["outcome"] == "Victory")
    low, high = wilson(rot_wins, len(rotation_runs))
    holds = {"rotation_55_90": {"rate": rot_wins / max(1, len(rotation_runs)), "low": low, "high": high, "verdict": hold(low, high, 0.55, 0.90)}}
    parties = {}
    for party, rows in group_by([r for r in d["runs"] if r["archetype"] != "rotation"], "party").items():
        w = sum(1 for r in rows if r["outcome"] == "Victory")
        lo, hi = wilson(w, len(rows))
        parties[party] = {"rate": w / len(rows), "low": lo, "high": hi, "verdict": hold(lo, hi, 0.35, None)}
    party_verdicts = [p["verdict"] for p in parties.values()]
    holds["representatives_35"] = {"parties": parties, "verdict": "missed" if "missed" in party_verdicts else "borderline" if "borderline" in party_verdicts else "met"}
    # Dangerous at least 1.5x Guarded net HP lost, per fight, each with its own interval (report 12's measure: HP lost over the party's maximum).
    fights = {}
    for stage in ("1", "2", "3", "4", "5"):
        tables = {"dangerous": defaultdict(lambda: [0.0, 0]), "guarded": defaultdict(lambda: [0.0, 0])}
        for r in d["encounters"]:
            if r["stage"] == stage and r["outcome"] in ("Victory", "Defeat"):
                cell = tables[r["route"]][r["seed"]]
                cell[0] += (number(r, "hp_start") - number(r, "hp_end")) / max(1, number(r, "hp_max")); cell[1] += 1
        if not tables["dangerous"] or not tables["guarded"]:
            fights[stage] = {"verdict": "not evaluable (not met)"}
            continue
        r = ratio_interval({s: tuple(v) for s, v in tables["dangerous"].items()}, {s: tuple(v) for s, v in tables["guarded"].items()}, draws, 1.5)
        r["verdict"] = "met" if r["low"] >= 1.5 else "missed" if r["high"] < 1.5 else "borderline"
        fights[stage] = r
    fight_verdicts = [f["verdict"] for f in fights.values()]
    holds["dangerous_1_5x"] = {"fights": fights, "verdict": "met" if all(v == "met" for v in fight_verdicts) else
                               "borderline" if all(v in ("met", "borderline") for v in fight_verdicts) else "missed"}
    # The finale has the highest defeat share: finale share minus each other stage's, shares pooled over routes.
    share = defaultdict(lambda: defaultdict(lambda: [0, 0]))
    for r in d["encounters"]:
        if r["outcome"] in ("Victory", "Defeat"):
            cell = share[r["stage"]][r["seed"]]
            cell[0] += 1 if r["outcome"] == "Defeat" else 0; cell[1] += 1
    def rate_of(stage, seeds):
        t = sum(share[stage][s][0] for s in seeds if s in share[stage]); n = sum(share[stage][s][1] for s in seeds if s in share[stage])
        return t / n if n else 0.0
    diffs = {}
    all_seeds = sorted({r["seed"] for r in d["runs"]})
    for stage in ("1", "2", "3", "4"):
        point = rate_of("5", all_seeds) - rate_of(stage, all_seeds)
        values = [rate_of("5", draw) - rate_of(stage, draw) for draw in draws]
        lo, hi = percentile(values, 0.025), percentile(values, 0.975)
        diffs[stage] = {"d": point, "low": lo, "high": hi, "verdict": "met" if lo > 0 else "missed" if hi <= 0 else "borderline"}
    finale_verdicts = [v["verdict"] for v in diffs.values()]
    holds["finale_highest"] = {"stages": diffs, "verdict": "missed" if "missed" in finale_verdicts else "borderline" if "borderline" in finale_verdicts else "met"}
    return holds


def targets_markdown(result):
    def iv(x, unit=""):
        return f"{x['d']:+.1f}{unit} ({x['low']:+.1f} to {x['high']:+.1f})"
    lines = ["# Progression targets (contract 13 section 6)", "", f"{result['runs']} paired runs on {result['seeds']} seeds; {result['resamples']} seed-cluster resamples (bootstrap seed {BOOTSTRAP_SEED}).", ""]
    lines += [f"- P1 (C2, gear = revision 9): **{result['P1']['verdict']}**" + (f" — {len(result['P1']['differences'])} differences" if result["P1"]["differences"] else ""),
              f"- P2 (each single card best for 2+ classes): **{result['P2']['verdict']}** — best counts {result['P2']['best']}",
              f"- P3 (largest gap ≤ 10 points): **{result['P3']['verdict']}** — largest {result['P3']['largest']:.1f}",
              f"- P4 (mixed rise ≤ 8 points, holds): **{result['P4']['verdict']}** — rise {iv(result['P4']['rise'], ' pts')}",
              f"- P5 (reserve specials in fights 4-5 ≥ 1.5x): **{result['P5']['verdict']}** — ratio {result['P5']['ratio']:.2f} ({result['P5']['low']:.2f} to {result['P5']['high']:.2f})",
              f"- P6 (gates zero, stalls ≤ before): **{result['P6']['verdict']}** — stalls {result['P6']['stalls']}",
              f"- Non-dominance: **{'yes' if result['non_dominance'] else 'no'}** — classes where each strategy is clearly beaten: {result['dominance']}",
              f"- Extra batch needed: **{'yes' if result['extra_batch_needed'] else 'no'}**", ""]
    lines += ["## Rotation win rates", "", "| Measurement | Win rate |", "| --- | --- |"]
    lines += [f"| {name} | {100 * v['win_rate']:.1f}% |" for name, v in result["rotation"].items()]
    lines += ["", "## P3 gaps (points, paired)", "", "| Pair | d (95% CI) | Verdict |", "| --- | --- | --- |"]
    lines += [f"| {pair} | {iv(g)} | {g['verdict']} |" for pair, g in result["P3"]["pairs"].items()]
    lines += ["", "## P2 classes (rotation, points, paired)", "", "| Class | Runs | gear − honed | gear − reserve | honed − reserve | Best |", "| --- | --- | --- | --- | --- | --- |"]
    for cls, c in result["P2"]["classes"].items():
        p = c["pairs"]
        cells = [f"{iv(p[k])} {p[k]['verdict']}" for k in ("gear-honed", "gear-reserve", "honed-reserve")]
        lines.append(f"| {cls} | {c['runs']} | " + " | ".join(cells) + f" | {c['best'] or 'none (' + ', '.join(c['possible']) + ' possible)'} |")
    h = result["P4"]["holds"]
    lines += ["", "## P4 holds (mixed)", "", f"- Rotation 55–90%: {100 * h['rotation_55_90']['rate']:.1f}% ({100 * h['rotation_55_90']['low']:.1f}–{100 * h['rotation_55_90']['high']:.1f}%) {h['rotation_55_90']['verdict']}",
              f"- Every representative party ≥ 35%: {h['representatives_35']['verdict']} (" + ", ".join(f"{k} {100 * v['low']:.0f}%" for k, v in h["representatives_35"]["parties"].items()) + " lower bounds)",
              "- Dangerous ≥ 1.5x Guarded net HP lost, per fight: " + h["dangerous_1_5x"]["verdict"] + " (" +
              ", ".join(f"fight {s} {f.get('ratio', 0):.2f}x [{f.get('low', 0):.2f}, {f.get('high', 0):.2f}] {f['verdict']}, {f.get('sign_decided', 0)} sign-decided" for s, f in h["dangerous_1_5x"]["fights"].items()) + ")",
              "- Finale has the highest defeat share: " + h["finale_highest"]["verdict"] + " (" +
              ", ".join(f"vs fight {s} {100 * v['d']:+.1f} [{100 * v['low']:+.1f}, {100 * v['high']:+.1f}]" for s, v in h["finale_highest"]["stages"].items()) + ")"]
    return "\n".join(lines) + "\n"


def resolve_folder(folder, local_root=LOCAL_ROOT):
    """The result folder, relative paths taken from the checkout root; it must lie inside the checkout's .local/."""
    local = Path(local_root).resolve()
    path = Path(folder)
    path = (path if path.is_absolute() else local.parent / path).resolve()
    if path == local or not path.is_relative_to(local):
        raise ValueError(f"Result folder must be inside {local}/, not {path}")
    return path


def main(argv=None, local_root=LOCAL_ROOT):
    argv = list(sys.argv[1:] if argv is None else argv)
    if argv and argv[0] in ("compare", "check-cards", "targets"):
        return progression_main(argv, local_root)
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folder", help="Result folder inside .local/, e.g. .local/balance/<stamp> (relative to the checkout root)")
    args = parser.parse_args(argv)
    try:
        folder = resolve_folder(args.folder, local_root)
        summary = summarize(load(folder))
    except (ValueError, OSError, KeyError) as error:
        print(f"balance_report: {error}", file=sys.stderr)
        return 2
    (folder / "summary.json").write_text(json.dumps(summary, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    (folder / "summary.md").write_text(markdown(summary), encoding="utf-8")
    o = summary["overall"]
    print(f"{o['n']} runs, win rate {interval(o)}, {o['stalls']} stalls -> {folder / 'summary.md'}")
    return 0


def progression_main(argv, local_root=LOCAL_ROOT):
    """compare BASE CANDIDATE --mode c1|c2; check-cards FOLDER --gear GEAR; targets --before ... --gear ... --reserve ... --honed ... --mixed ... --out FOLDER.
    Every folder lies inside .local/. compare and check-cards exit 1 on any difference."""
    parser = argparse.ArgumentParser(prog="balance_report.py", description="Contract 13 section 6 comparisons and targets (#218)")
    sub = parser.add_subparsers(dest="command", required=True)
    c = sub.add_parser("compare"); c.add_argument("base"); c.add_argument("candidate"); c.add_argument("--mode", choices=("c1", "c2"), required=True)
    k = sub.add_parser("check-cards"); k.add_argument("folder"); k.add_argument("--gear", required=True)
    t = sub.add_parser("targets")
    for name in ("before", "gear", "reserve", "honed", "mixed"):
        t.add_argument(f"--{name}", nargs="+", required=True, help="one folder per batch; batches are pooled")
    t.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == "compare":
            problems = compare(resolve_folder(args.base, local_root), resolve_folder(args.candidate, local_root), args.mode, limit=0)
        elif args.command == "check-cards":
            problems = check_cards(resolve_folder(args.folder, local_root), resolve_folder(args.gear, local_root))
        else:
            folders = {name: [resolve_folder(f, local_root) for f in getattr(args, name)] for name in ("before", "gear", "reserve", "honed", "mixed")}
            measurements = {name: load_measurement(f) for name, f in folders.items()}
            c2 = []
            for before, gear in zip(folders["before"], folders["gear"]):
                c2 += compare(before, gear, "c2", limit=0)
            measurements["c2"] = c2
            result = targets(measurements)
            out = resolve_folder(args.out, local_root)
            out.mkdir(parents=True, exist_ok=True)
            (out / "targets.json").write_text(json.dumps(result, indent=1, sort_keys=True, default=str) + "\n", encoding="utf-8")
            (out / "targets.md").write_text(targets_markdown(result), encoding="utf-8")
            print(targets_markdown(result))
            return 0
    except (ValueError, OSError, KeyError) as error:
        print(f"balance_report: {error}", file=sys.stderr)
        return 2
    for problem in problems[:50]:
        print(problem)
    print(f"{args.command}: {'pass' if not problems else f'{len(problems)} differences'}")
    return 1 if problems else 0

if __name__ == "__main__":
    sys.exit(main())
