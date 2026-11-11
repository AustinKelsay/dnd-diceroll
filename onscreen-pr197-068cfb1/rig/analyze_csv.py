import sys,datetime,calendar
sys.path.insert(0,'/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-89abfd45-4e2f806269b1eac1/tools')
import perf
csvp,cap_start_epoch=sys.argv[1],float(sys.argv[2])
frames=perf.parse_csv(open(csvp,encoding='utf-8-sig',errors='replace').read())
marks={}
for a in sys.argv[3:]:
    n,s,e=a.split(':'); marks[n]=(float(s),float(e))
t=0.0; rows=[]
for f in frames:
    t+=f.frame/1000.0; rows.append((cap_start_epoch+t,f))
print('frames',len(frames),'span s %.1f'%t)
for n,(s,e) in marks.items():
    sel=[f for ts,f in rows if s<=ts<=e]
    if not sel: print(n,'no frames'); continue
    P=lambda xs,q: perf.percentile(xs,q)
    ft=[f.frame for f in sel]; gp=[f.gpu for f in sel]; gt=[f.game for f in sel]
    print(f"{n}: frames={len(sel)} FrameTime p50={P(ft,50):.2f} p95={P(ft,95):.2f} max={max(ft):.2f} | GPU p50={P(gp,50):.2f} p95={P(gp,95):.2f} max={max(gp):.2f} | GameThread p50={P(gt,50):.2f} p95={P(gt,95):.2f} max={max(gt):.2f}")
