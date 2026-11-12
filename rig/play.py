#!/usr/bin/env python3
"""play.py SAVE SHOTDIR [--until PHASE/ENC] [--rounds N] : keyboard bot reading the checkpoint, driving the game with wtype only.
Arrow mapping at the default camera yaw (-90): Left=+x Right=-x Up=+y Down=-y."""
import json,sys,subprocess,time,os,hashlib,collections,argparse
ap=argparse.ArgumentParser(); ap.add_argument('save'); ap.add_argument('shots')
ap.add_argument('--stop-phase',default=''); ap.add_argument('--stop-enc',type=int,default=99)
ap.add_argument('--max-seconds',type=int,default=1500); ap.add_argument('--route',default='zx')
ap.add_argument('--shot-every',type=float,default=2.5); ap.add_argument('--gap',type=int,default=45)
ap.add_argument('--prefix',default='b'); ap.add_argument('--route2',default=''); ap.add_argument('--fast',action='store_true')
a=ap.parse_args()
os.makedirs(a.shots,exist_ok=True); T0=time.time(); N=[0]
def log(*x): print(f"[{time.time()-T0:6.1f}s]",*x,flush=True)
def load():
    for _ in range(15):
        try: return json.load(open(a.save))['Current']
        except Exception: time.sleep(0.2)
    return None
def sig(c): return hashlib.md5(json.dumps(c,sort_keys=True).encode()).hexdigest()
def keys(seq,gap=None):
    g=str(gap or a.gap); cmd=['wtype']
    for k in seq:
        if len(k)==1: cmd+=['-s',g,k]
        else: cmd+=['-s',g,'-k',k]
    subprocess.run(cmd,check=False)
def shot(tag,png=False):
    N[0]+=1; p=f"{a.shots}/{a.prefix}{N[0]:04d}-{tag}.{'png' if png else 'jpg'}"
    subprocess.run(['grim','-o','HEADLESS-1']+([] if png else ['-t','jpeg','-q','78'])+[p],check=False); return p
I=lambda u:u.get('ID',u.get('Id'))
def units(c): return c['Battle']['Encounter']['Combatants']
def active(c):
    b=c['Battle']; return next(u for u in units(c) if I(u)==b['TurnOrder'][b['TurnIndex']])
def alive(u): return u['Life']==0 and u['HP']>0
def bfs(c,me):
    e=c['Battle']['Encounter']; W,H=e['Width'],e['Height']
    blocked={(x['X'],x['Y']) for x in e['BlockedCells']}
    occ={(u['Cell']['X'],u['Cell']['Y']) for u in units(c) if I(u)!=I(me) and (u['Life']!=2)}
    start=(me['Cell']['X'],me['Cell']['Y']); dist={start:0}; q=[start]
    for cur in q:
        for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
            n=(cur[0]+dx,cur[1]+dy)
            if 0<=n[0]<W and 0<=n[1]<H and n not in blocked and n not in occ and n not in dist:
                dist[n]=dist[cur]+1; q.append(n)
    return dist
md=lambda p,q:abs(p[0]-q[0])+abs(p[1]-q[1])
def cell(u): return (u['Cell']['X'],u['Cell']['Y'])
BURST=[0]
def burst(tag,n=40):
    BURST[0]+=1; d=f"{a.shots}/burst{BURST[0]:03d}-{tag}"; os.makedirs(d,exist_ok=True)
    subprocess.Popen(['bash','-c',f'for i in $(seq -w 1 {n}); do grim -o HEADLESS-1 -t jpeg -q 85 {d}/f$i.jpg; done'])
def aim(c,mode,target,bu=None):
    e=c['Battle']['Encounter']; W,H=e['Width'],e['Height']; tx,ty=target
    seq=['Escape']+([mode] if mode else [])+['Left']*(W+2)+['Up']*(H+2)+['Right']*(W-1-tx)+['Down']*(H-1-ty)
    keys(['Tab']); time.sleep(0.5)
    keys(seq,gap=45)
    time.sleep(0.45)
    if bu: burst(bu)
    keys(['Return'],gap=60)
def wait_change(s0,t=3.5):
    t1=time.time()
    while time.time()-t1<t:
        c=load()
        if c and sig(c)!=s0: return c
        time.sleep(0.25)
    return None
fails=collections.defaultdict(int); lastshot=[0]
def periodic(tag):
    if time.time()-lastshot[0]>=a.shot_every: lastshot[0]=time.time(); shot(tag)
def los(c,A,B):
    bl={(x['X'],x['Y']) for x in c['Battle']['Encounter']['BlockedCells']}
    fx,fy=A; dx,dy=B[0]-A[0],B[1]-A[1]; nx,ny=abs(dx),abs(dy); sx=(dx>0)-(dx<0); sy=(dy>0)-(dy<0); ix=iy=0
    while ix<nx or iy<ny:
        d=(1+2*ix)*ny-(1+2*iy)*nx
        if d==0:
            if (fx+sx,fy) in bl or (fx,fy+sy) in bl: return False
            fx+=sx; fy+=sy; ix+=1; iy+=1
        elif d<0: fx+=sx; ix+=1
        else: fy+=sy; iy+=1
        if (fx,fy) in bl and (ix<nx or iy<ny or (fx,fy)!=B): return False
    return True
def pathdist(c,goal,me=None):
    e=c['Battle']['Encounter']; W,H=e['Width'],e['Height']
    bl={(x['X'],x['Y']) for x in e['BlockedCells']}
    bl|={(u['Cell']['X'],u['Cell']['Y']) for u in units(c) if u['Life']!=2 and (u['Cell']['X'],u['Cell']['Y'])!=goal and (me is None or I(u)!=I(me))}
    dist={goal:0}; q=[goal]
    for cur in q:
        for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
            n=(cur[0]+dx,cur[1]+dy)
            if 0<=n[0]<W and 0<=n[1]<H and n not in bl and n not in dist: dist[n]=dist[cur]+1; q.append(n)
    return dist
def hero_turn(c):
    me=active(c); b=c['Battle']; foes=[u for u in units(c) if u['Side']==1 and alive(u)]
    allies=[u for u in units(c) if u['Side']==0]
    if not foes: return False
    tkey=(b.get('Round',b.get('round')),I(me)); mc=cell(me)
    if b['bActionAvailable'] and me.get('Potions',0)>0:
        pt=[u for u in allies if u['Life']==0 and 0<u['HP']<0.55*u['MaxHP'] and md(mc,cell(u))<=1]
        if pt:
            t=min(pt,key=lambda u:u['HP']/u['MaxHP'])
            if fails[(tkey,'p',cell(t))]<4: return ('p',cell(t),'potion')
    if b['bActionAvailable']:
        for al in allies:
            if al['Life']==1 and md(mc,cell(al))<=1 and fails[(tkey,'r',cell(al))]<4:
                return ('r',cell(al),'rescue')
        if me['ClassId']=='cleric' and me['SpellSlots']>0:
            for al in sorted([u for u in allies if alive(u) and u['HP']<0.5*u['MaxHP']],key=lambda u:u['HP']):
                if md(mc,cell(al))<=max(me['Range'],1) and los(c,mc,cell(al)) and fails[(tkey,'f',cell(al))]<4:
                    return ('f',cell(al),'heal')
        inr=[f for f in foes if md(mc,cell(f))<=me['Range'] and los(c,mc,cell(f))]
        for f in sorted(inr,key=lambda f:(f['HP'],md(mc,cell(f)))):
            mode='f' if (me['ClassId']=='wizard' and me['SpellSlots']>0) else 'a'
            if fails[(tkey,mode,cell(f))]<4: return (mode,cell(f),'attack')
            if mode=='f' and fails[(tkey,'a',cell(f))]<4: return ('a',cell(f),'attack')
    if b['MovementLeft']>0:
        dist=bfs(c,me)
        dn=[al for al in allies if al['Life']==1]
        goal=dn[0] if dn else min(foes,key=lambda f:md(mc,cell(f)))
        pd=pathdist(c,cell(goal),me); here=pd.get(mc,99)
        want=1 if dn else (1 if me['Range']<=1 else min(me['Range'],3))
        cands=[(pd.get(p,99),d,p) for p,d in dist.items() if 0<d<=b['MovementLeft'] and fails[(tkey,'m',p)]<2]
        pool=[x for x in cands if x[0]<here and (x[0]>=want or True)]
        if pool:
            # ranged units stop at standoff 'want' if reachable (and have LOS), else closest
            ok=[x for x in pool if x[0]>=want and (dn or me['Range']<=1 or los(c,x[2],cell(goal)))]
            if me['Range']>1 and not dn:
                ok.sort(key=lambda x:(x[0]-want if x[0]>=want else 99, x[1]))
                if ok and ok[0][0]-want<=1: return ('a',ok[0][2],'move')
            pool.sort(key=lambda x:(x[0],x[1]))
            return ('a',pool[0][2],'move')
    return None
def reward_phase(c):
    keys(['Escape']); time.sleep(0.3)
def main():
    global lastarch
    lastarch=None
    last_phase=None; enc_seen=-1; stuck=0; lastsig=None; lastchange=time.time()
    while time.time()-T0<a.max_seconds:
        c=load()
        if not c: time.sleep(1); continue
        P=c['Progress']; ph=P['Phase']; encs=P['EncountersCompleted']
        s=sig(c)
        if s!=lastsig: lastsig=s; lastchange=time.time()
        if (ph,encs)!=last_phase:
            log('PHASE',ph,'encounters done',encs); last_phase=(ph,encs); time.sleep(1.5); shot(f'phase{ph}e{encs}',True)
        if a.stop_phase and str(ph)==a.stop_phase and encs>=a.stop_enc: log('stop at',ph,encs); return
        if ph==0:   # route
            ch=a.route[encs%len(a.route)]; keys([ch]); time.sleep(3.0)
        elif ph==1:
            b=c['Battle']
            if b.get('Phase',1)!=1 and False: pass
            me=active(c)
            if me['Side']!=0:
                if me['Role']=='Archer' and lastarch!=(b.get('Round',b.get('round')),I(me)): lastarch=(b.get('Round',b.get('round')),I(me)); burst('arrow',45)
                periodic('enemy'); time.sleep(0.8); continue
            plan=hero_turn(c)
            if plan is None:
                log('end turn',me['Name']); s0=s; keys(['Escape','space']); 
                if not wait_change(s0,4): log('end turn not accepted?'); time.sleep(1)
                else: time.sleep(0.6)
                continue
            mode,tgt,why=plan; tkey=(b.get('Round',b.get('round')),I(me))
            log(f"R{b.get('Round',b.get('round'))} {me['Name']}({me['ClassId']}) {why} {mode}->{tgt} mv{b['MovementLeft']} act{b['bActionAvailable']}")
            shot('pre-'+why)
            aim(c,mode,tgt,bu=('spell' if (mode=='f' and why=='attack') else ('hit-'+me['ClassId']) if why=='attack' else None)); nc=wait_change(s,3.5)
            if nc is None:
                fails[(tkey,mode if why!='move' else 'm',tgt)]+=1; log('  not accepted'); time.sleep(0.8)
            else:
                time.sleep(0.5); shot('act-'+why); time.sleep(0.9); shot('act2-'+why)
        elif ph==2:   # reward
            keys(['Escape']); time.sleep(0.4); shot('reward',True)
            for sl in ('1','2','3','4'):
                keys([sl]); time.sleep(0.25); keys(['l']); time.sleep(0.6)
            time.sleep(1.5)
            c2=load()
            if c2 and c2['Progress']['Phase']==2:
                stuck+=1; log('reward still open',stuck); shot('reward-stuck',True)
                if stuck>3: log('STUCK at reward'); return
        elif ph==3:   # recovery/camp
            shot('camp',True); keys(['c']); time.sleep(2.5)
            c2=load()
            if c2 and c2['Progress']['Phase']==3: keys(['v']); time.sleep(2.0)
        elif ph in(4,5):
            log('ENDING phase',ph); time.sleep(3); shot('ending',True); return
        else: time.sleep(1)
        if time.time()-lastchange>400: log('no state change for 120s'); shot('stall',True); return
    log('time limit')
main()
