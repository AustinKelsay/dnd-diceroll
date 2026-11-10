#!/usr/bin/env python3
"""Plays the party's turns through the real UI (wtype keys + a pointer wiggle), reading the checkpoint file the game writes after every accepted action.
Policy v1: attack the nearest conscious enemy (move toward it first, BFS over free cells), Cure Wounds on an ally under 50% HP when it is in reach, Rescue a downed adjacent ally.
usage: play.py SAVE LOGFILE MAXSECONDS [stop_after_encounters]"""
import json,sys,time,subprocess,os,collections
SAVE,LOG,MAXS=sys.argv[1],sys.argv[2],float(sys.argv[3]); STOP=int(sys.argv[4]) if len(sys.argv)>4 else 99
O='/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-193f04c2-6d221135861cb956/.local/onscreen'
PTR=O+'/ptr/ptr2'
env=dict(os.environ)
def load():
    for _ in range(10):
        try: return json.load(open(SAVE))['Current']
        except Exception: time.sleep(0.2)
def log(s):
    line=time.strftime('%H:%M:%S ')+s; print(line,flush=True); open(LOG,'a').write(line+'\n')
def keys(seq,gap=140):
    a=['wtype']
    for k in seq: a+=['-s',str(gap),'-k',k]
    subprocess.run(a,env=env)
GC=None; MODE=False; LASTID=None
def wiggle():
    subprocess.run([PTR,'1920','1080','m','960','80','s','120','m','1000','90'],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
def state():
    c=load(); b=c['Battle']; e=b['Encounter']; return c,b,e
def active(b,e):
    aid=b['TurnOrder'][b['TurnIndex']]; return next(x for x in e['Combatants'] if x['ID']==aid)
def manh(a,b): return abs(a['X']-b['X'])+abs(a['Y']-b['Y'])
def summary(e): return ' '.join(f"{x['Name'][:5]}{x['HP']}/{x['MaxHP']}{'L'+str(x['Life']) if x['Life'] else ''}" for x in e['Combatants'])
def nav(frm,to):
    dx,dy=to['X']-frm['X'],to['Y']-frm['Y']
    seq=['Left']*dx if dx>0 else ['Right']*(-dx)
    seq+= ['Up']*dy if dy>0 else ['Down']*(-dy)
    return seq
def reach(e,b,me,occupied):
    blocked={(c['X'],c['Y']) for c in e['BlockedCells']} if e['BlockedCells'] and isinstance(e['BlockedCells'][0],dict) else set()
    start=(me['Cell']['X'],me['Cell']['Y']); budget=b['MovementLeft']; seen={start:0}; q=collections.deque([start])
    while q:
        p=q.popleft()
        if seen[p]>=budget: continue
        for d in((1,0),(-1,0),(0,1),(0,-1)):
            n=(p[0]+d[0],p[1]+d[1])
            if n in seen or n in blocked or n in occupied or not(0<=n[0]<e['Width'] and 0<=n[1]<e['Height']): continue
            seen[n]=seen[p]+1; q.append(n)
    return seen
def act(mode_key, tgt):
    """mode_key: a/f/r/p. Moves the keyboard cursor from where it is (tracked) to tgt and presses Enter."""
    global GC,MODE
    seq=[mode_key]
    if not MODE: seq.append('Left'); MODE=True   # first arrow only shows the cursor on its cell
    seq+=nav(GC,tgt)+['Return']
    keys(seq); GC={'X':tgt['X'],'Y':tgt['Y']}
def sig(c): return json.dumps(c['Battle']['TurnIndex'])+str(c['Battle']['round'])+str(c['Battle']['MovementLeft'])+str(c['Battle']['bActionAvailable'])+str(c['Battle']['bBonusActionAvailable'])+summary(c['Battle']['Encounter'])
t0=time.time(); start_done=load()['Progress']['EncountersCompleted']; last_round=None
log('start '+summary(load()['Battle']['Encounter'])+f" done={start_done} rev={load()['Progress']['CatalogRevision']}")
while time.time()-t0<MAXS:
    c,b,e=state()
    if c['Progress']['Phase']!=1 or c['Progress']['EncountersCompleted']>=start_done+STOP:
        log(f"STOP phase={c['Progress']['Phase']} done={c['Progress']['EncountersCompleted']} | {summary(e)}"); break
    me=active(b,e)
    if b['round']!=last_round: last_round=b['round']; log(f"ROUND {b['round']} | {summary(e)}")
    if me['Side']!=0:
        time.sleep(2.0); continue
    if me['Life']!=0: keys(['space']); time.sleep(2); continue
    if me['ID']!=LASTID:
        LASTID=me['ID']; GC={'X':me['Cell']['X'],'Y':me['Cell']['Y']}
    before=sig(c)
    foes=[x for x in e['Combatants'] if x['Side']==1 and x['Life']==0]
    allies=[x for x in e['Combatants'] if x['Side']==0 and x['ID']!=me['ID']]
    if not foes: time.sleep(2); continue
    occ={(x['Cell']['X'],x['Cell']['Y']) for x in e['Combatants'] if x['Life']!=2 or x['Side']==0}
    done=False
    # rescue
    down=[x for x in allies if x['Life']==1 and manh(x['Cell'],me['Cell'])<=1] if b['bActionAvailable'] else []
    if down:
        act('r',down[0]['Cell']); log(f"{me['Name']} Rescue {down[0]['Name']}"); time.sleep(3.5); done=True
    if not done and me['Role'] in('Cleric',) and b['bActionAvailable'] and me.get('KitUses',0)>0:
        hurt=[x for x in allies if x['Life']==0 and x['HP']*2<x['MaxHP'] and manh(x['Cell'],me['Cell'])<=me['Range']]
        if hurt:
            act('f',hurt[0]['Cell']); log(f"{me['Name']} Cure Wounds {hurt[0]['Name']}"); time.sleep(3.5); done=True
    if not done:
        foe=min(foes,key=lambda f:manh(f['Cell'],me['Cell']))
        d=manh(foe['Cell'],me['Cell'])
        if d>me['Range'] and b['MovementLeft']>0:
            cells=reach(e,b,me,occ)
            tgt=min(cells,key=lambda p:(max(0,abs(p[0]-foe['Cell']['X'])+abs(p[1]-foe['Cell']['Y'])-me['Range']),cells[p]))
            if tgt!=(me['Cell']['X'],me['Cell']['Y']):
                act('a',{'X':tgt[0],'Y':tgt[1]}); log(f"{me['Name']} move {me['Cell']['X']},{me['Cell']['Y']} -> {tgt}"); time.sleep(4.5)
                c2,b2,e2=state(); me2=active(b2,e2)
                if sig(c2)==before: log('  move not accepted'); keys(['space']); time.sleep(2); continue
                if b2['bActionAvailable'] and me2['ID']==me['ID']:
                    foe=min([x for x in e2['Combatants'] if x['Side']==1 and x['Life']==0] or [foe],key=lambda f:manh(f['Cell'],me2['Cell']))
                    if manh(foe['Cell'],me2['Cell'])<=me2['Range']:
                        act('a',foe['Cell']); log(f"{me['Name']} attack {foe['Name']}"); time.sleep(3.5)
        elif d<=me['Range'] and b['bActionAvailable']:
            act('a',foe['Cell']); log(f"{me['Name']} attack {foe['Name']}"); time.sleep(3.5)
    c3,b3,e3=state()
    if active(b3,e3)['ID']==me['ID'] and c3['Progress']['Phase']==1:
        keys(['space']); log(f"{me['Name']} end turn | {summary(e3)}"); time.sleep(2.5)
log('end elapsed %.0fs'%(time.time()-t0))
