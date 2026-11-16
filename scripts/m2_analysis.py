"""M2 balance analysis (#241): the contract's metrics for one step, from Balance.Baseline. result folders. Python 3.11+, standard library only.

usage: python3 -I m2_analysis.py CHECKOUT OUT_JSON --ref r10/{gear,reserve,honed,mixed} --step r11-x/{gear,reserve,honed,mixed}
Each of --ref and --step is a folder prefix under CHECKOUT/.local/balance/ whose four strategy folders are <prefix>-<strategy>.
"""
import json
import statistics
import sys
from collections import defaultdict
from pathlib import Path

CHECKOUT = Path(sys.argv[1]).resolve()
sys.path.insert(0, str(CHECKOUT / "tools"))
import balance_report as br  # noqa: E402

STRATEGIES = ("gear", "reserve", "honed", "mixed")


def load(prefix):
    return {s: br.load(CHECKOUT / ".local" / "balance" / f"{prefix}-{s}") for s in STRATEGIES}


def metrics(d, draws):
    runs, enc, heroes = d["runs"], d["encounters"], d["heroes"]
    won = sum(1 for r in runs if r["outcome"] == "Victory")
    rot = [r for r in runs if r["archetype"] == "rotation"]
    rot_won = sum(1 for r in rot if r["outcome"] == "Victory")
    fought = [e for e in enc if e["outcome"] in ("Victory", "Defeat", "Stall")]
    rounds = sorted(br.number(e, "rounds") for e in fought)
    by_stage = defaultdict(lambda: [0, 0, 0.0, 0])
    for e in fought:
        c = by_stage[e["stage"]]
        c[0] += e["outcome"] == "Defeat"; c[1] += 1; c[2] += br.number(e, "specials_used"); c[3] += br.number(e, "revives")
    slot = defaultdict(lambda: [0, 0, 0, 0])
    for h in heroes:
        c = slot[h["slot"]]
        c[0] += br.number(h, "damage_taken"); c[1] += h["survived"] == "0"; c[2] += br.number(h, "downs"); c[3] += 1
    tot = [sum(c[i] for c in slot.values()) for i in range(3)]
    classes = defaultdict(lambda: [0, 0, 0, 0])  # rotation runs won/total (each run once per class), hero deaths/heroes
    seen = set()
    outcome = {br.run_key(r): r["outcome"] for r in runs}
    for h in heroes:
        if h["archetype"] != "rotation":
            continue
        c = classes[h["class"]]
        c[2] += h["survived"] == "0"; c[3] += 1
        key = (h["class"],) + br.run_key(h)
        if key not in seen:
            seen.add(key); c[0] += outcome[br.run_key(h)] == "Victory"; c[1] += 1
    class_rates = {k: {"win": v[0] / v[1], "runs": v[1], "death": v[2] / v[3]} for k, v in sorted(classes.items())}
    rates = [v["win"] for v in class_rates.values()]
    holds = br.slice_holds(d, draws)
    redowns = None
    return {
        "runs": len(runs), "win": won / len(runs), "rotation_win": rot_won / len(rot),
        "stalls": sum(1 for r in runs if r["outcome"] == "Stall"),
        "deadlocks": sum(1 for r in runs if "deadlock" in r.get("stall_reason", "")),
        "gates": {k: sum(br.number(r, k) for r in runs) for k in ("policy_errors", "illegal_proposals", "passive_ends", "protection_mismatches")},
        "deaths_per_run": sum(br.number(r, "deaths") for r in runs) / len(runs),
        "rounds_per_fight": statistics.fmean(rounds), "rounds_p99": rounds[int(0.99 * (len(rounds) - 1))], "rounds_max": rounds[-1],
        "fights_over_15": sum(1 for x in rounds if x > 15),
        "stages": {s: {"defeat_share": c[0] / c[1], "defeats": c[0], "fights": c[1], "specials": c[2] / c[1], "revives": c[3] / c[1]} for s, c in sorted(by_stage.items())},
        "slots": {s: {"damage_share": c[0] / tot[0], "death_share": c[1] / max(1, tot[1]), "down_share": c[2] / max(1, tot[2])} for s, c in sorted(slot.items())},
        "classes": class_rates, "class_spread": max(rates) - min(rates), "class_median": statistics.median(rates),
        "holds": {k: v["verdict"] for k, v in holds.items()}, "holds_detail": holds,
    }


def rescue_metrics(prefix, strategy):
    """rescues.csv (party, seed, stage, revives, redowns, quick_redowns), when the harness wrote it."""
    folder = CHECKOUT / ".local" / "balance" / f"{prefix}-{strategy}"
    rows = []
    for f in sorted(folder.glob("*/rescues.csv")):
        rows += br.read_csv(f)
    if not rows:
        return None
    revives = sum(br.number(r, "revives") for r in rows)
    return {"fights": len(rows), "revives": revives, "redowns": sum(br.number(r, "redowns") for r in rows),
            "quick_redowns": sum(br.number(r, "quick_redowns") for r in rows),
            "quick_share": sum(br.number(r, "quick_redowns") for r in rows) / max(1, revives)}


def main():
    out = Path(sys.argv[2])
    args = sys.argv[3:]
    ref = args[args.index("--ref") + 1]
    step = args[args.index("--step") + 1]
    R, S = load(ref), load(step)
    seeds = sorted({r["seed"] for r in S["mixed"]["runs"]})
    draws = br.resamples(seeds)
    result = {"ref": ref, "step": step, "ref_metrics": {}, "step_metrics": {}, "paired": {}}
    for s in STRATEGIES:
        result["ref_metrics"][s] = metrics(R[s], draws)
        result["step_metrics"][s] = metrics(S[s], draws)
        result["step_metrics"][s]["rescue"] = rescue_metrics(step, s)
        a, b = br.wins(S[s]), br.wins(R[s])
        result["paired"][s] = br.paired_difference(a, b, set(a) & set(b), draws)
    # Cards (#228): P2 per class, P3 gaps, via the contract 13 statistics; P4's before is the reference mixed (revision 10).
    t = br.targets({"before": R["mixed"], "gear": S["gear"], "reserve": S["reserve"], "honed": S["honed"], "mixed": S["mixed"]}, draws)
    result["cards"] = {"P2": t["P2"], "P3": t["P3"], "P4_rise": t["P4"]["rise"], "dominance": t["dominance"], "non_dominance": t["non_dominance"],
                       "P5_reserve_vs_ref": {k: t["P5"][k] for k in ("ratio", "low", "high")}}
    out.write_text(json.dumps(result, indent=1, sort_keys=True, default=str) + "\n", encoding="utf-8")
    m = result["step_metrics"]["mixed"]
    print(f"step {step} vs {ref}")
    for s in STRATEGIES:
        x, y, p = result["ref_metrics"][s], result["step_metrics"][s], result["paired"][s]
        print(f"  {s:8} win {100*x['win']:.1f} -> {100*y['win']:.1f} (paired {p['d']:+.1f} [{p['low']:+.1f},{p['high']:+.1f}])  rot {100*y['rotation_win']:.1f}"
              f"  deaths/run {x['deaths_per_run']:.2f} -> {y['deaths_per_run']:.2f}  rounds {x['rounds_per_fight']:.2f} -> {y['rounds_per_fight']:.2f}"
              f" p99 {y['rounds_p99']} max {y['rounds_max']} >15 {y['fights_over_15']}  stalls {y['stalls']} gates {sum(y['gates'].values())}")
    print("  mixed defeat share by fight:", {k: f"{100*v['defeat_share']:.1f}%" for k, v in m["stages"].items()}, "holds", m["holds"])
    print("  mixed specials per fight:", {k: round(v["specials"], 2) for k, v in m["stages"].items()}, " revives/fight:", {k: round(v["revives"], 2) for k, v in m["stages"].items()})
    print("  mixed slot shares (damage/death/down):", {k: f"{100*v['damage_share']:.0f}/{100*v['death_share']:.0f}/{100*v['down_share']:.0f}" for k, v in m["slots"].items()})
    print("  mixed classes:", {k: f"{100*v['win']:.1f}/{100*v['death']:.0f}" for k, v in m["classes"].items()}, f"median {100*m['class_median']:.1f} spread {100*m['class_spread']:.1f}")
    print("  rescue:", m["rescue"])
    c = result["cards"]
    print("  P2 best:", c["P2"]["best"], "verdict", c["P2"]["verdict"], " P3 largest", round(c["P3"]["largest"], 1), c["P3"]["verdict"],
          " rise", round(c["P4_rise"]["d"], 1), " non-dominance", c["non_dominance"])
    for cls, v in c["P2"]["classes"].items():
        p = v["pairs"]
        print(f"    {cls:10} gear-honed {p['gear-honed']['d']:+5.1f} {p['gear-honed']['verdict']:5}  gear-reserve {p['gear-reserve']['d']:+5.1f} {p['gear-reserve']['verdict']:5}"
              f"  honed-reserve {p['honed-reserve']['d']:+5.1f} {p['honed-reserve']['verdict']:5}  best {v['best']}")


if __name__ == "__main__":
    main()
