#!/usr/bin/env python3
"""fx.py SRC OUT order=1,2,3,4,20,21 turn=ID id=x,y ... [down=ID] [hp=ID:N]
Copy a capture fixture, move units, set the turn order, optionally down a hero."""
import json,sys,shutil,os
src,out=sys.argv[1],sys.argv[2]
d=json.load(open(f'/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-07f94f24-db431b77301bf2cd/Saved/CaptureFixtures/{src}/current-v1.json'))
b=d['Current']['Battle']; e=b['Encounter']; C={x['Id']:x for x in e['Combatants']}
blocked={(c['X'],c['Y']) for c in e['BlockedCells']}
for a in sys.argv[3:]:
    k,v=a.split('=')
    if k=='order': b['TurnOrder']=[int(i) for i in v.split(',')]; b['TurnIndex']=0
    elif k=='turn': b['TurnIndex']=b['TurnOrder'].index(int(v))
    elif k=='down': x=C[int(v)]; x['HP']=0; x['Life']=1; x['RescueTurnsLeft']=2
    elif k=='hp': i,n=v.split(':'); C[int(i)]['HP']=int(n)
    else:
        x,y=map(int,v.split(',')); assert (x,y) not in blocked,(k,x,y,'blocked'); C[int(k)]['Cell']={'X':x,'Y':y}
cells=[(x['Cell']['X'],x['Cell']['Y']) for x in e['Combatants']]; assert len(set(cells))==len(cells),cells
b['MovementLeft']=4; b['bActionAvailable']=True; b['bBonusActionAvailable']=True
os.makedirs(f'/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-07f94f24-db431b77301bf2cd/.local/onscreen/fixtures/{out}',exist_ok=True)
json.dump(d,open(f'/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-07f94f24-db431b77301bf2cd/.local/onscreen/fixtures/{out}/current-v1.json','w'))
print(out,'blocked',sorted(blocked)); print(' '.join(f"{i}@{c['Cell']['X']},{c['Cell']['Y']}L{c['Life']}" for i,c in C.items()),b['TurnOrder'],b['TurnIndex'])
