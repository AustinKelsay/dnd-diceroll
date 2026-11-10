"""Tuning target metrics for one balance result folder (scratch helper for #184)."""
import csv, glob, json, sys, statistics as st, collections, math
d = sys.argv[1]
R = [r for f in sorted(glob.glob(d + '/*/runs.csv')) for r in csv.DictReader(open(f))]
E = [r for f in sorted(glob.glob(d + '/*/encounters.csv')) for r in csv.DictReader(open(f))]
H = [r for f in sorted(glob.glob(d + '/*/heroes.csv')) for r in csv.DictReader(open(f))]
def wr(rows): return sum(r['outcome'] == 'Victory' for r in rows) / max(1, len(rows))
rot = [r for r in R if r['archetype'] == 'rotation']
out = {}
out['rotation_win'] = wr(rot); out['all_win'] = wr(R)
out['stalls'] = sum(r['outcome'] == 'Stall' for r in R)
out['parties'] = {p: round(wr([r for r in R if r['party'] == p]), 3) for p in ['familiar','melee','ranged','support-poor','support-heavy','mixed','dup-fighter','dup-cleric','dup-wizard']}
won = [r for r in R if r['outcome'] == 'Victory']
out['victories_with_deaths'] = sum(int(r['deaths']) > 0 for r in won) / max(1, len(won))
out['deaths_per_run'] = st.fmean(int(r['deaths']) for r in R)
low = collections.defaultdict(list); lost = collections.defaultdict(list); defeats = collections.Counter(); fights = collections.Counter()
for e in E:
    if e['outcome'] not in ('Victory', 'Defeat'): continue
    k = (int(e['stage']), e['route']); fights[k] += 1
    low[k].append(int(e['hp_low']) / max(1, int(e['hp_max'])))
    lost[k].append((int(e['hp_start']) - int(e['hp_end'])) / max(1, int(e['hp_max'])))
    defeats[k] += e['outcome'] == 'Defeat'
out['stage'] = {f"{s}-{r}": {"n": fights[(s, r)], "low_mean": round(st.fmean(low[(s, r)]), 3), "below50": round(sum(x < 0.5 for x in low[(s, r)]) / len(low[(s, r)]), 3),
                 "hp_lost": round(st.fmean(lost[(s, r)]), 3), "defeat": round(defeats[(s, r)] / fights[(s, r)], 3)} for (s, r) in sorted(fights)}
st_def = {s: sum(defeats[(s, r)] for r in ('guarded', 'dangerous')) for s in range(1, 6)}
out['defeats_by_stage'] = st_def
out['defeat_share_by_stage'] = {s: round(st_def[s] / max(1, sum(fights[(s, r)] for r in ('guarded', 'dangerous'))), 3) for s in range(1, 6)}
out['dangerous_over_guarded_hp'] = {s: round(st.fmean(lost[(s, 'dangerous')]) / max(1e-9, st.fmean(lost[(s, 'guarded')])), 2) for s in range(1, 6) if lost[(s, 'guarded')] and lost[(s, 'dangerous')]}
melee = {'fighter', 'barbarian', 'monk', 'paladin'}
fc = collections.Counter((e['party'], e['seed']) for e in E)
def dpf(rows):
    n = sum(min(fc[(h['party'], h['seed'])], int(h['death_stage'])) if h['survived'] == '0' and int(h['death_stage']) else fc[(h['party'], h['seed'])] for h in rows)
    return sum(int(h['damage']) for h in rows) / max(1, n)
rh = [h for h in H if h['archetype'] == 'rotation']
m, o = dpf([h for h in rh if h['class'] in melee]), dpf([h for h in rh if h['class'] not in melee])
out['melee_dmg'] = round(m, 2); out['other_dmg'] = round(o, 2); out['melee_gap'] = round(1 - m / o, 3)
sp = collections.defaultdict(list)
for e in E: sp[int(e['stage'])].append(int(e['specials_used']))
out['specials_by_stage'] = {s: round(st.fmean(v), 2) for s, v in sorted(sp.items())}
out['rounds_per_fight'] = round(sum(int(e['rounds']) for e in E) / len(E), 2)
cls = collections.defaultdict(list)
for h in rh: cls[h['class']].append(h)
out['class_death'] = {c: round(sum(h['survived'] == '0' for h in v) / len(v), 3) for c, v in sorted(cls.items())}
out['class_win'] = {c: round(len({(h['party'], h['seed']) for h in v if h['outcome'] == 'Victory'}) / len({(h['party'], h['seed']) for h in v}), 3) for c, v in sorted(cls.items())}
print(json.dumps(out, indent=1))
