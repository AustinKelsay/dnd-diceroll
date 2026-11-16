"""usage: python3 -I quick.py CHECKOUT folder-name ...  (folders under .local/balance; one strategy each)"""
import sys
from pathlib import Path
CHECKOUT = Path(sys.argv[1]).resolve()
sys.path.insert(0, str(Path(__file__).resolve().parent)); sys.argv_saved = sys.argv
sys.argv = [sys.argv[0], str(CHECKOUT), "/dev/null"]
import m2_analysis as m  # noqa
br = m.br
for name in sys.argv_saved[2:]:
    d = br.load(CHECKOUT / ".local" / "balance" / name)
    seeds = sorted({r["seed"] for r in d["runs"]}); draws = br.resamples(seeds, count=200)
    x = m.metrics(d, draws)
    rows = []
    for f in sorted((CHECKOUT / ".local" / "balance" / name).glob("*/rescues.csv")): rows += br.read_csv(f)
    rv = sum(br.number(r, "revives") for r in rows); q = sum(br.number(r, "quick_redowns") for r in rows); rd = sum(br.number(r, "redowns") for r in rows)
    st = x["stages"]; sl = x["slots"]; c = x["classes"]
    print(f"{name:22} win {100*x['win']:.1f} rot {100*x['rotation_win']:.1f} d/run {x['deaths_per_run']:.2f} rnd {x['rounds_per_fight']:.2f} >15 {x['fights_over_15']} max {x['rounds_max']} stalls {x['stalls']}"
          f" | def% " + "/".join(f"{100*st[s]['defeat_share']:.1f}" for s in "12345") +
          f" | spc5 {st['5']['specials']:.2f} rev5 {st['5']['revives']:.2f} quick {q}/{rv}={q/max(1,rv):.3f} redown {rd/max(1,rv):.2f}"
          f" | slot dmg " + "/".join(f"{100*sl[s]['damage_share']:.0f}" for s in "1234") + " death " + "/".join(f"{100*sl[s]['death_share']:.0f}" for s in "1234") +
          f" | monk {100*(c['monk']['win']-x['class_median']):+.1f} pal {100*(c['paladin']['win']-x['class_median']):+.1f} med {100*x['class_median']:.1f} | holds {','.join(k[:4]+':'+v[:4] for k,v in x['holds'].items())}")
