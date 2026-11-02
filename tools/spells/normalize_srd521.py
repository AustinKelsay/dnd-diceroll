"""Normalize 5e-bits src/2024/en/5e-SRD-Spells.json (SRD 5.2.1 content) into the record shape gen_spell_catalog.py reads.
Save type, half-on-save, area, healing dice and cantrip tiers are parsed from the SRD 5.2.1 spell text itself.
Usage: python normalize_srd521.py 5e-SRD-Spells-2024.json out.json"""
import json,re,sys
AB={'Strength':'str','Dexterity':'dex','Constitution':'con','Intelligence':'int','Wisdom':'wis','Charisma':'cha'}
out=[]
for s in json.load(open(sys.argv[1])):
    t=s['description']+' '+(s.get('higher_level') or '')
    ct=s['casting_time']
    ct='1 action' if ct=='Action' else '1 bonus action' if ct.startswith('Bonus Action') else '1 reaction' if ct.startswith('Reaction') else ct
    r=dict(index=s['index'],name=s['name'],level=s['level'],school=s['school'],classes=s['classes'],subclasses=[],casting_time=ct,
           ritual=s['ritual'],range=s['range'],components=s['components'],duration=s['duration'],concentration=s['concentration'],desc=[t])
    if s.get('attack_type'): r['attack_type']=s['attack_type']
    m=re.search(r'(Strength|Dexterity|Constitution|Intelligence|Wisdom|Charisma) saving throw',s['description'])
    if m and not r.get('attack_type'):
        r['dc']={'dc_type':{'index':AB[m[1]]},'dc_success':'half' if re.search(r'half as much damage',t) else 'none'}
    m=re.search(r'(\d+)-foot[-\w]*(?:,? \d+-foot[-\w]*)*\s+(Sphere|Cone|Cube|Line|Cylinder|Emanation)',s['description'])
    if m: r['area_of_effect']={'type':'sphere' if m[2]=='Emanation' else m[2].lower(),'size':int(m[1])}
    lvl=s['level']
    if s.get('damage'):
        d=s['damage'];src=d['damage_at_slot_level'];base=src.get(str(lvl)) or list(src.values())[0]
        types=set(x.lower() for x in re.findall(r'\d+d\d+(?: \+ \w+)? (\w+) damage',s['description']))
        dm={'damage_type':d['damage_type']}
        if lvl==0:
            n,f=base.split('d');dm['damage_at_character_level']={str(k):'%dd%s'%(int(n)*i,f) for i,k in enumerate([1,5,11,17],1)}
        else:
            n,f=base.split('d',1);f=f.split('+')[0].strip()
            inc=re.search(r'increases by (\d+)d(\d+) for each (?:spell )?slot level above',t)
            dm['damage_at_slot_level']={str(k):'%dd%s'%(int(n)+((k-lvl)*int(inc[1]) if inc and inc[2]==f else 0),f) for k in range(lvl,10)}
        r['damage']=[dm]+([{'extra':True}] if len(types)>1 else [])
    elif s['index']=='magic-missile':
        # Three darts of 1d4 + 1 Force, one more dart per slot level above 1: stored as darts x d4 (+1 per dart).
        r['damage']=[{'damage_type':{'index':'force','name':'Force'},'damage_at_slot_level':{str(k):'%dd4 + %d'%(k+2,k+2) for k in range(1,10)}}]
    elif lvl>0 and re.search(r'(\w+) damage equal to (\d+)d(\d+)',s['description']):
        m=re.search(r'(\w+) damage equal to (\d+)d(\d+)',s['description'])
        inc=re.search(r'increases by (\d+)d(\d+) for (?:every|each) (?:\w+ )?(?:spell )?slot levels? above',t)
        step=2 if inc and 'every slot level' in t and 'two' in t[inc.start()-60:inc.end()+20] else 1
        r['damage']=[{'damage_type':{'index':m[1].lower(),'name':m[1]},'damage_at_slot_level':{str(k):'%dd%s%s'%(int(m[2])+(((k-lvl)//step)*int(inc[1]) if inc else 0),m[3],' + MOD' if 'd'+m[3]+' plus your spellcasting ability modifier' in s['description'] else '') for k in range(lvl,10)}}]
    else:
        m=re.search(r'regains?\s+(?:a number of\s+)?Hit Points equal to (\d+)d(\d+)( plus your spellcasting ability modifier)?',s['description'])
        if m and lvl>0:
            inc=re.search(r'healing increases by (\d+)d(\d+) for each spell slot level above',t)
            r['heal_at_slot_level']={str(k):'%dd%s%s'%(int(m[1])+((k-lvl)*int(inc[1]) if inc else 0),m[2],' + MOD' if m[3] else '') for k in range(lvl,10)}
    out.append(r)
json.dump(out,open(sys.argv[2],'w'),indent=1)
print(len(out))
