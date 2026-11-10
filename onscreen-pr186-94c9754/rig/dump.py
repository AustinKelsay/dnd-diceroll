import json,sys
d=json.load(open(sys.argv[1])); cur=d['Current']
print('rev',cur['Progress']['CatalogRevision'],'phase',cur['Progress']['Phase'],'done',cur['Progress']['EncountersCompleted'])
for x in cur['Battle']['Encounter']['Combatants']:
    hp=['HP','MaxHP']
    print(x['Name'].ljust(9),x['Role'].ljust(8),'side',x['Side'],'HP',{k:x[k] for k in hp},'AC',x['ArmorClass'],'atk',x['AttackBonus'],'d',x['DamageDie'],'+',x['DamageBonus'],'rng',x['Range'],'cell',(x['Cell']['X'],x['Cell']['Y']))
