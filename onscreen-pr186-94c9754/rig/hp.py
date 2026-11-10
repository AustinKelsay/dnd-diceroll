import json,sys
d=json.load(open(sys.argv[1])); cur=d['Current']; e=cur['Battle']['Encounter']
a=[(x['Name'],x['HP'],x['MaxHP'],x['Life'],x.get('RescueTurnsLeft',0)) for x in e['Combatants']]
rnd=cur['Battle'].get('Round','?')
print('phase',cur['Progress']['Phase'],'done',cur['Progress']['EncountersCompleted'],'| '+' '.join(f"{n[:5]}{h}/{m}{'(L'+str(l)+')' if l else ''}" for n,h,m,l,r in a))
