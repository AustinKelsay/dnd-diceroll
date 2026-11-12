#!/usr/bin/env python3
"""fx.py SRC OUT [op ...]   SRC = a Saved/CaptureFixtures name. ops:
 order=1,2,3,4,20,21 | turn=ID | ID=x,y | down=ID | hp=ID:N | maxhp=ID:N | hero=ID:BUILDID:SRCFIXTURE  (replace hero ID by the BuildId record from SRCFIXTURE) | build=ID:BUILDID (id only)
 Copies a capture fixture, edits it, writes .local/onscreen/fixtures/OUT/current-v1.json"""
import json,sys,os,copy
W=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','..'))
src,out=sys.argv[1],sys.argv[2]
def ld(n): return json.load(open(f'{W}/Saved/CaptureFixtures/{n}/current-v1.json'))
d=ld(src); P=d['Current']['Progress']; b=d['Current']['Battle']; e=b['Encounter']; C={x['Id']:x for x in e['Combatants']}
blocked={(c['X'],c['Y']) for c in e['BlockedCells']}
for a in sys.argv[3:]:
    k,v=a.split('=',1)
    if k=='order':
        b['TurnOrder']=[int(i) for i in v.split(',')]; b['TurnIndex']=0
        for n,i in enumerate(b['TurnOrder']): C[i]['Initiative']=max(18-2*n,1+C[i]['InitiativeBonus'])
    elif k=='turn': b['TurnIndex']=b['TurnOrder'].index(int(v))
    elif k=='down': x=C[int(v)]; x['HP']=0; x['Life']=1; x['RescueTurnsLeft']=2
    elif k=='dead': x=C[int(v)]; x['HP']=0; x['Life']=2; x['RescueTurnsLeft']=0
    elif k=='alive': x=C[int(v)]; x['Life']=0; x['HP']=x['MaxHP']; x['RescueTurnsLeft']=0
    elif k=='hp': i,n=v.split(':'); C[int(i)]['HP']=int(n)
    elif k=='maxhp': i,n=v.split(':'); C[int(i)]['MaxHP']=int(n); C[int(i)]['HP']=int(n)
    elif k=='hero':
        i,bid,sf=v.split(':'); i=int(i); s=ld(sf)['Current']['Battle']['Encounter']['Combatants']
        r=copy.deepcopy(next(c for c in s if c['BuildId']==bid)); old=C[i]
        for f in ('Id','Name','Cell','Side','Initiative'): r[f]=old[f]
        idx=e['Combatants'].index(old); e['Combatants'][idx]=r; C[i]=r
        for p in P['Party']:
            if p['Id']==i: p['BuildId']=bid
    else:
        x,y=map(int,v.split(',')); assert (x,y) not in blocked,(k,x,y,'blocked'); C[int(k)]['Cell']={'X':x,'Y':y}
cells=[(x['Cell']['X'],x['Cell']['Y']) for x in e['Combatants']]; assert len(set(cells))==len(cells),cells
b['MovementLeft']=4; b['bActionAvailable']=True; b['bBonusActionAvailable']=True
os.makedirs(f'{W}/.local/onscreen/fixtures/{out}',exist_ok=True)
json.dump(d,open(f'{W}/.local/onscreen/fixtures/{out}/current-v1.json','w'))
print(out,'size',e['Width'],e['Height'],'blocked',sorted(blocked)); print(' '.join(f"{i}:{c['Role'][:4]}@{c['Cell']['X']},{c['Cell']['Y']}L{c['Life']}r{c['Range']}" for i,c in C.items()),b['TurnOrder'],b['TurnIndex'])
