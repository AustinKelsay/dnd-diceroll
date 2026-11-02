import json,re,sys
# Usage: python gen_spell_catalog.py 5e-SRD-Spells.json Source/DnDDiceRoll/Private/Rules/SpellCatalogData.inl spell_table.md
d=json.load(open(sys.argv[1]))
d.sort(key=lambda s:(s['level'],s['name']))
CLS=['bard','cleric','druid','paladin','ranger','sorcerer','warlock','wizard']
SUB=['lore','land','fiend','life','devotion']
SCH=['abjuration','conjuration','divination','enchantment','evocation','illusion','necromancy','transmutation']
DT=['acid','bludgeoning','cold','fire','force','lightning','necrotic','piercing','poison','psychic','radiant','slashing','thunder']
COND=['blinded','charmed','deafened','frightened','grappled','incapacitated','invisible','paralyzed','petrified','poisoned','prone','restrained','stunned','unconscious']
AREA={None:'None','cone':'Cone','cube':'Cube','cylinder':'Cylinder','line':'Line','sphere':'Sphere'}
ABIL={'str':'Strength','dex':'Dexterity','con':'Constitution','int':'Intelligence','wis':'Wisdom','cha':'Charisma'}
# Hand-modelled modifier spells (SRD 5.2.1 text): AC bonus / roll die / speed
MOD={'Bless':('RollBonusD4',0),'Bane':('RollPenaltyD4',0),'Shield':('AcBonus',5),'Shield of Faith':('AcBonus',2),
 'Mage Armor':('AcBase',13),'Barkskin':('AcFloor',16),'Longstrider':('Speed',10),'Haste':('AcBonus',2),'Slow':('AcBonus',-2),
 'Aid':('MaxHp',5),'Heroism':('TempHpPerTurn',0),'Protection from Evil and Good':('Ward',0),'Sanctuary':('Ward',0),
 'Blur':('AttackerDisadvantage',0),'Mirror Image':('AttackerDisadvantage',0),'Faerie Fire':('AttackerAdvantage',0),'Hunter\'s Mark':('BonusDamageD6',0),
 'Hex':('BonusDamageD6',0),'Divine Favor':('BonusDamageD4',0),'Enlarge/Reduce':('BonusDamageD4',0),'Magic Weapon':('WeaponBonus',1),'Spiritual Weapon':(None,0)}
def esc(s): return s.replace('\\','\\\\').replace('"','\\"')
def dice(v):
    v=v.split(' OR ')[-1] if False else v.split(' OR ')[0]
    m=re.fullmatch(r'(?:(\d+)d(\d+))?\s*(?:\+?\s*(\d+|MOD))?',v.strip())
    if not m: return None
    return (int(m[1] or 0),int(m[2] or 0),m[3])
rows=[];doc=[]
for s in d:
    lvl=s['level'];ct=s['casting_time']
    cast={'1 action':'Action','1 bonus action':'BonusAction','1 reaction':'Reaction'}.get(ct,'Extended')
    r=s['range'];m=re.match(r'(\d+) (feet|mile|miles)',r)
    feet=int(m[1])*(5280 if 'mile' in m[2] else 1) if m else (5 if r=='Touch' else 0)
    comps=s['components']
    mask=' | '.join('SpellClass::'+c.capitalize() for c in CLS if any(x['index']==c for x in s['classes'])) or '0'
    subs=', '.join(x['name'] for x in s['subclasses'] if x['index']!='lore')
    aoe=s.get('area_of_effect') or {}
    dc=s.get('dc');atk=s.get('attack_type')
    cnt=sides=0;flat=0;addmod=False;dtype='Force';table=[0]*10;effect='Utility';multi=len(s.get('damage',[]))>1;heal=False
    dm=(s.get('damage') or [None])[0]
    src=None
    if dm and ('damage_at_slot_level' in dm or 'damage_at_character_level' in dm):
        dtype=dm['damage_type']['name'] if 'damage_type' in dm else 'Force'
        src=dm.get('damage_at_slot_level') or dm.get('damage_at_character_level');cant='damage_at_character_level' in dm
    elif 'heal_at_slot_level' in s:
        src=s['heal_at_slot_level'];cant=False;heal=True
    if src:
        parsed={int(k):dice(v) for k,v in src.items()}
        if all(parsed.values()):
            first=parsed[min(parsed)];sides=first[1];flat=first[2];addmod=flat=='MOD';flat=0 if addmod or flat is None else int(flat)
            if cant:
                for i,k in enumerate([1,5,11,17]): table[i]=parsed.get(k,parsed[max(x for x in parsed if x<=k)])[0]
            else:
                for k,v in parsed.items(): table[k]=v[0]
            cnt=table[0] if cant else table[lvl]
            effect='Healing' if heal else 'Damage'
    conds=[c for c in COND if re.search(r'\b(be|is|becomes|become|are|knocked|falls?|it is|target is)\s+(\w+\s+)?'+c+r'\b|\b(have|has|gains?|giving it) the '+c+r' condition',' '.join(s.get('desc',[])),re.I)]
    if effect=='Utility' and conds and dc: effect='Condition'
    modk=MOD.get(s['name'])
    if effect=='Utility' and modk and modk[0]: effect='Modifier'
    res='Save' if dc and dc['dc_type']['index'] in ABIL else ('MeleeAttack' if atk=='melee' else 'RangedAttack' if atk=='ranged' else ('Automatic' if effect in('Damage','Healing','Modifier') else 'None'))
    impl='Mechanics' if effect!='Utility' and not multi else ('Partial' if multi else 'DataOnly')
    sv=ABIL[dc['dc_type']['index']] if dc and dc['dc_type']['index'] in ABIL else 'Dexterity'
    half='true' if dc and dc.get('dc_success')=='half' else 'false'
    condmask=' | '.join('(1u << %d)'%COND.index(c) for c in conds) or '0'
    mk,mv=(modk if modk and modk[0] else ('None',0))
    rows.append('    { TEXT("%s"), TEXT("%s"), %d, ESpellSchool::%s, %s, TEXT("%s"), ESpellCastTime::%s, TEXT("%s"), %s, %d, TEXT("%s"), %s, %s, %s, %s, TEXT("%s"), ESpellArea::%s, %d, ESpellResolution::%s, EAbility::%s, %s, ESpellEffect::%s, %d, %d, %s, EDamageType::%s, { %s }, %s, ESpellModifier::%s, %d, ESpellImplementation::%s },'%(
        s['index'],esc(s['name']),lvl,s['school']['name'],mask,esc(subs),cast,ct,'true' if s['ritual'] else 'false',feet,r,
        *('true' if c in comps else 'false' for c in 'VSM'),'true' if s['concentration'] else 'false',esc(s['duration']),AREA[aoe.get('type')],aoe.get('size',0),
        res,sv,half,effect,sides,flat,'true' if addmod else 'false',dtype,', '.join(map(str,table)),condmask,mk,mv,impl))
    doc.append((s['name'],lvl,s['school']['name'],', '.join(x['name'] for x in s['classes']),subs,('Bonus' if cast=='BonusAction' else cast)+(' (ritual)' if s['ritual'] else ''),'Yes' if s['concentration'] else '',impl))
open(sys.argv[2],'w').write('''// GENERATED by tools/spells/gen_spell_catalog.py from the SRD 5.2.1 spell list (Wizards of the Coast, CC BY 4.0; machine-readable via 5e-bits/5e-database src/2024, MIT), normalized by normalize_srd521.py.
// Do not edit by hand. Rows are sorted by level, then name. Fields follow FSrdSpell in Rules/SpellCatalogRules.h.
'''+'\n'.join(rows)+'\n')
md=['| Spell | Lvl | School | Classes | Subclass lists | Cast | Conc. | Status |','| --- | --- | --- | --- | --- | --- | --- | --- |']
md+=['| %s | %s | %s | %s | %s | %s | %s | %s |'%(n,'C' if l==0 else l,sc,c,sb,ca,co,im) for n,l,sc,c,sb,ca,co,im in doc]
open(sys.argv[3],'w').write('\n'.join(md)+'\n')
from collections import Counter
print(len(rows));print(sorted(Counter((x[1],x[7]) for x in doc).items()))
