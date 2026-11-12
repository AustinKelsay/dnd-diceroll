#!/usr/bin/env python3
"""drv.py SAVE state | prep MODE X Y | end
prep: Escape, mode key, first arrow, cursor to (X,Y) relative to the active unit's cell, NO confirm (the caller fires Return inside a burst)."""
import json,sys,subprocess,os,time
SAVE=sys.argv[1]; cmd=sys.argv[2]
def I(x): return x.get('Id',x.get('ID'))
def load():
    for _ in range(10):
        try: return json.load(open(SAVE))['Current']
        except Exception: time.sleep(0.2)
def active(c):
    b=c['Battle']; e=b['Encounter']; aid=b['TurnOrder'][b['TurnIndex']]; return next(x for x in e['Combatants'] if I(x)==aid)
def keys(seq,gap=140):
    a=['wtype']
    for k in seq: a+=['-s',str(gap),'-k',k]
    subprocess.run(a)
def summary(c):
    b=c['Battle']; e=b['Encounter']
    return f"round {b.get('Round',b.get('round','?'))} turn {active(c)['Role']}({I(active(c))}) move {b['MovementLeft']} act {b['bActionAvailable']} bonus {b['bBonusActionAvailable']} | "+' '.join(f"{I(x)}:{x['Role'][:4]}{x['HP']}/{x['MaxHP']}{'L'+str(x['Life']) if x['Life'] else ''}@{x['Cell']['X']},{x['Cell']['Y']}" for x in e['Combatants'])+f" phase {c['Progress']['Phase']}"
if cmd=='state': print(summary(load()))
elif cmd=='prep':
    mode=sys.argv[3]; tx,ty=int(sys.argv[4]),int(sys.argv[5]); c=load(); e=c['Battle']['Encounter']; W,H=e['Width'],e['Height']
    seq=['Escape',mode]+['Left']*(W+2)+['Up']*(H+2)+['Right']*(W-1-tx)+['Down']*(H-1-ty)
    keys(seq,gap=int(os.environ.get('GAP','45'))); print('prepped',mode,tx,ty)
elif cmd=='rel':
    mode=sys.argv[3]; tx,ty=int(sys.argv[4]),int(sys.argv[5]); a=active(load()); ux,uy=a['Cell']['X'],a['Cell']['Y']
    dx,dy=tx-ux,ty-uy
    seq=['Escape',mode,'Left' if dx<=0 else 'Left']  # first arrow only reveals the cursor
    seq+=(['Left']*(dx-1) if dx>0 else ['Right']*(-dx)) if False else []
    # reveal consumed one Left: undo it if dx<=0 by pressing Right once more
    nav=(['Left']*dx if dx>0 else ['Right']*(-dx))+(['Up']*dy if dy>0 else ['Down']*(-dy))
    seq=['Escape',mode,'Left']  # reveal only
    seq+=nav
    keys(seq,gap=int(os.environ.get('GAP','90'))); print('rel',mode,'from',ux,uy,'to',tx,ty)
elif cmd=='end': keys(['Escape','space'])
