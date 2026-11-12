import sys,re,calendar,time,glob,json
sys.path.insert(0,'/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-59ab0e19-b83c5ecca4681b4a/tools')
import perf
run=sys.argv[1]; base=f'/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-59ab0e19-b83c5ecca4681b4a/.local/onscreen/run/{run}'
csv=glob.glob(base+'/runtime/Saved/Profiling/CSV/*.csv')[0]
frames=perf.parse_csv(open(csv,encoding='utf-8-sig',errors='replace').read())
log=open(base+'/game.log',errors='replace').read()
def ts(s):
    m=re.match(r'\[(\d+)\.(\d+)\.(\d+)-(\d+)\.(\d+)\.(\d+):(\d+)\]',s)
    y,mo,d,h,mi,se,ms=map(int,m.groups()); return calendar.timegm((y,mo,d,h,mi,se))+ms/1000
ev=[(ts(l),int(re.search(r'Frame (\d+)\]',l).group(1))) for l in log.splitlines() if 'CSVEvent "GC"' in l]
st=[ts(l) for l in log.splitlines() if 'CSVEvent "Cmd: stat fps"' in l or 'CSVEvent "Cmd: r.vsync 0" [Frame 1]' in l][0]
cum=[0.0]
for f in frames: cum.append(cum[-1]+f.frame/1000.0)
# calibrate CSV cumulative frame time to wall clock with the log's CSVEvent GC timestamps (piecewise linear, slope 1 past the last)
anch=[(0.0,st)]+[(cum[min(fr,len(cum)-1)],e) for e,fr in ev if fr>0]
def wall(c):
    for (c0,w0),(c1,w1) in zip(anch,anch[1:]):
        if c<=c1: return w0+(c-c0)*(w1-w0)/(c1-c0)
    c0,w0=anch[-1]; return w0+(c-c0)
rows=[(wall(cum[i]),f) for i,f in enumerate(frames)]
print('frames',len(frames),'span %.1f s'%cum[-1],'anchors',[(round(a,2),round(b-st,2)) for a,b in anch])
marks={}
for l in open(base+'/marks.log'):
    _,n,v=l.split(); marks[n]=float(v)
wins=[a.split(':') for a in sys.argv[2:]]
P=perf.percentile
out={}
for n,s,e in wins:
    s=marks[s]; e=marks[e]
    sel=[f for tt,f in rows if s<=tt<=e]
    if not sel: print(n,'no frames'); continue
    ft=[f.frame for f in sel]; gp=[f.gpu for f in sel]; gt=[f.game for f in sel]
    r=dict(frames=len(sel),seconds=round(e-s,1),frame_p50=P(ft,50),frame_p95=P(ft,95),frame_max=max(ft),gpu_p50=P(gp,50),gpu_p95=P(gp,95),gpu_max=max(gp),game_p50=P(gt,50),game_p95=P(gt,95),game_max=max(gt))
    out[n]=r
    print(f"{n}: frames={len(sel)} ({e-s:.1f}s) Frame p50={P(ft,50):.2f} p95={P(ft,95):.2f} max={max(ft):.2f} | GPU p50={P(gp,50):.2f} p95={P(gp,95):.2f} max={max(gp):.2f} | GT p50={P(gt,50):.2f} p95={P(gt,95):.2f} max={max(gt):.2f}")
json.dump(out,open(base+'/windows.json','w'),indent=1)
