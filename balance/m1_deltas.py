"""M1 (#240) sweep comparison. Usage: python3 m1_deltas.py CHECKOUT identity|deltas
identity: .local/balance/m1-r10-before-<s> against m1-r10-after-<s>: every CSV byte-equal, group.json equal apart from seconds and policy_version.
deltas: m1-r11-<s> against m1-r10-after-<s>: overall rates and paired (party, seed) differences with balance_report's seed-cluster bootstrap."""
import json, sys, pathlib
from collections import defaultdict

root = pathlib.Path(sys.argv[1]).resolve()
sys.path.insert(0, str(root / "tools"))
import balance_report as br  # noqa: E402

STRATEGIES = ["gear", "reserve", "honed", "mixed"]
BAL = root / ".local" / "balance"


def identity():
    ok = True
    for s in STRATEGIES:
        a, b = BAL / f"m1-r10-before-{s}", BAL / f"m1-r10-after-{s}"
        groups = sorted(p.name for p in a.iterdir() if p.is_dir())
        if groups != sorted(p.name for p in b.iterdir() if p.is_dir()):
            print(s, "group sets differ"); ok = False; continue
        problems = []
        for g in groups:
            for f in ("runs.csv", "encounters.csv", "heroes.csv"):
                if (a / g / f).read_bytes() != (b / g / f).read_bytes():
                    problems.append(f"{g}/{f} differs")
            ma, mb = json.loads((a / g / "group.json").read_text()), json.loads((b / g / "group.json").read_text())
            for k in sorted(set(ma) | set(mb)):
                if k in ("seconds", "policy_version"):
                    continue
                if ma.get(k) != mb.get(k):
                    problems.append(f"{g}/group.json {k}: {ma.get(k)!r} != {mb.get(k)!r}")
            pv = (ma.get("policy_version"), mb.get("policy_version"))
        print(f"{s}: {len(groups)} groups, {'byte-identical CSVs' if not problems else problems[:10]}, policy_version {pv[0]} -> {pv[1]}")
        ok &= not problems
    return 0 if ok else 1


def per_run(data, field):
    return {br.run_key(r): float(r[field]) for r in data["runs"]}


def deltas():
    out = {}
    for s in STRATEGIES:
        base = br.load_measurement([BAL / f"m1-r10-after-{s}"])
        cand = br.load_measurement([BAL / f"m1-r11-{s}"])
        keys = sorted(set(br.run_key(r) for r in base["runs"]) & set(br.run_key(r) for r in cand["runs"]))
        draws = br.resamples(sorted({k[1] for k in keys}))
        row = {"runs": len(keys)}
        for name, data in (("r10", base), ("r11", cand)):
            runs = data["runs"]
            enc = data["encounters"]
            row[name] = {
                "win": sum(r["outcome"] == "Victory" for r in runs) / len(runs),
                "stalls": sum(r["outcome"] == "Stall" for r in runs),
                "deadlocks": sum("deadlock" in r.get("stall_reason", "") for r in runs),
                "deaths_per_run": sum(float(r["deaths"]) for r in runs) / len(runs),
                "rounds_per_fight": sum(float(e["rounds"]) for e in enc) / len(enc),
                "rounds_per_run": sum(float(r["rounds"]) for r in runs) / len(runs),
                "fights_won": sum(float(r["encounters_won"]) for r in runs) / len(runs),
                "gates": sum(int(r["policy_errors"]) + int(r["illegal_proposals"]) + int(r["passive_ends"]) + int(r["protection_mismatches"]) for r in runs),
            }
        w11, w10 = br.wins(cand), br.wins(base)
        row["win_d"] = br.paired_difference(w11, w10, keys, draws)
        d11, d10 = per_run(cand, "deaths"), per_run(base, "deaths")
        # deaths are counts 0..4: the paired difference is reported in deaths per 100 runs, divide by 100 for per run
        row["deaths_d"] = br.paired_difference(d11, d10, keys, draws)
        row["changed_outcome"] = sum(w11[k] != w10[k] for k in keys)
        # per stage: losses at each fight
        for name, data in (("r10", base), ("r11", cand)):
            lost = defaultdict(int)
            for r in data["runs"]:
                if r["outcome"] == "Defeat":
                    lost[int(r["encounters_won"]) + 1] += 1
            row[name]["defeats_by_fight"] = {k: lost[k] for k in sorted(lost)}
        out[s] = row
        p = lambda x: f"{100 * x:.1f}%"
        print(f"{s}: runs {len(keys)} | win {p(row['r10']['win'])} -> {p(row['r11']['win'])}, paired {row['win_d']['d']:+.1f} pts ({row['win_d']['low']:+.1f} to {row['win_d']['high']:+.1f}) {br.verdict(row['win_d'])}"
              f" | deaths/run {row['r10']['deaths_per_run']:.3f} -> {row['r11']['deaths_per_run']:.3f} (paired {row['deaths_d']['d'] / 100:+.3f}, {row['deaths_d']['low'] / 100:+.3f} to {row['deaths_d']['high'] / 100:+.3f})"
              f" | rounds/fight {row['r10']['rounds_per_fight']:.2f} -> {row['r11']['rounds_per_fight']:.2f} | rounds/run {row['r10']['rounds_per_run']:.1f} -> {row['r11']['rounds_per_run']:.1f}"
              f" | stalls {row['r10']['stalls']} ({row['r10']['deadlocks']} deadlocks) -> {row['r11']['stalls']} ({row['r11']['deadlocks']}) | gates {row['r10']['gates']} -> {row['r11']['gates']}"
              f" | outcome changed in {row['changed_outcome']} runs | defeats by fight {row['r10']['defeats_by_fight']} -> {row['r11']['defeats_by_fight']}")
    (BAL / "m1-deltas.json").write_text(json.dumps(out, indent=1, sort_keys=True) + "\n")
    return 0


sys.exit(identity() if sys.argv[2] == "identity" else deltas())
