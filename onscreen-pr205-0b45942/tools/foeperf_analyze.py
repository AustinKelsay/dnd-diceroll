#!/usr/bin/env python3
"""foeperf_analyze.py RUNDIR : windows of a standalone foe-fixture perf session (times.txt wall clocks; CSV frame 0 ~ process start)."""
import sys, glob, json
sys.path.insert(0, '/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-641aa874-26d2aed68b01116a/tools')
import perf
d = sys.argv[1]
t = {}
for l in open(d + '/times.txt'):
    k, v = l.strip().split('='); t[k] = float(v)
csvs = sorted(glob.glob(d + '/runtime/**/*.csv', recursive=True))
print('csv', csvs)
frames = perf.parse_csv(open(csvs[-1]).read())
cum = []; s = 0.0
for f in frames:
    s += f.frame / 1000.0; cum.append(s)
# CSV frame 0 is the first engine frame; the wall clock T0 is the process launch (a few seconds earlier). Anchor RESUME to the biggest hitch after 10 s.
big = max((i for i in range(len(frames)) if cum[i] > 8), key=lambda i: frames[i].frame)
anchor = cum[big]; print('resume hitch frame', big, 'at', round(anchor, 1), 's', round(frames[big].frame, 1), 'ms')
off = anchor - (t['RESUME'] - t['T0'])
def win(a, b, name):
    sel = [f for f, c in zip(frames, cum) if a + off + 0 <= c <= b + off]
    r = perf.summarize(sel); r['window'] = name; return r
H0 = t['HOLD_START'] - t['T0']; H1 = t['HOLD_END'] - t['T0']; P0 = t['PLAY_START'] - t['T0']; P1 = t['PLAY_END'] - t['T0']
out = {'hold': win(H0, H1, 'hold'), 'play': win(P0, P1, 'play'), 'hold+play': win(H0, P1, 'hold+play'), 'resume_hitch_ms': round(frames[big].frame, 1)}
for k in ('hold', 'play', 'hold+play'):
    r = out[k]; print(k, r['frames'], 'frames', 'frame p95', r['frame_p95_ms'], 'gpu p95', r['gpu_p95_ms'], 'game p95', r['game_p95_ms'], 'max', r['frame_max_ms'])
sel = [f for f, c in zip(frames, cum) if H0 + off <= c <= P1 + off]
out['top_gpu'] = perf.top_gpu_stats(sel, 6)
json.dump(out, open(d + '/foeperf.json', 'w'), indent=1)
