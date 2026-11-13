#include "Rules/SpellRules.h"
#include "Combat/CombatBattle.h"

namespace SpellTable
{
    // After Duration: Bard, Cleric, Druid, Paladin, Ranger, Sorcerer, Warlock, Wizard, Artificer.
    // Class tags verified against SRD 5.2.1 / PHB class lists for spells present in the catalog.
    static const FSpellFacts Spells[(int32)ESpell::Count] = {
        { TEXT("Fire Bolt"), 0, ESpellSchool::Evocation, TEXT("1 action"), 120, TEXT("120 ft"), true, true, false, false, TEXT("Instantaneous"), false, false, false, false, false, true, false, true, true, 1, 10, false, EDamageType::Fire, 0, 0, false, 0, false },
        { TEXT("Sacred Flame"), 0, ESpellSchool::Evocation, TEXT("1 action"), 60, TEXT("60 ft"), true, true, false, false, TEXT("Instantaneous"), false, true, false, false, false, false, false, false, false, 1, 8, false, EDamageType::Radiant, 0, 0, false, 0, false },
        { TEXT("Magic Missile"), 1, ESpellSchool::Evocation, TEXT("1 action"), 120, TEXT("120 ft"), true, true, false, false, TEXT("Instantaneous"), false, false, false, false, false, true, false, true, false, 3, 4, false, EDamageType::Force, 1, 0, false, 1, false },
        { TEXT("Cure Wounds"), 1, ESpellSchool::Abjuration, TEXT("1 action"), 0, TEXT("Touch"), true, true, false, false, TEXT("Instantaneous"), true, true, true, true, true, false, false, false, true, 2, 8, true, EDamageType::Force, 2, 0, false, 0, false },
        { TEXT("Guiding Bolt"), 1, ESpellSchool::Evocation, TEXT("1 action"), 120, TEXT("120 ft"), true, true, false, false, TEXT("1 round"), false, true, false, false, false, false, false, false, false, 4, 6, false, EDamageType::Radiant, 1, 0, false, 0, false },
        { TEXT("Burning Hands"), 1, ESpellSchool::Evocation, TEXT("1 action"), 0, TEXT("Self (15-ft cone)"), true, true, false, false, TEXT("Instantaneous"), false, false, false, false, false, true, false, true, false, 3, 6, false, EDamageType::Fire, 1, 0, false, 0, false },
        { TEXT("Healing Word"), 1, ESpellSchool::Abjuration, TEXT("1 bonus action"), 60, TEXT("60 ft"), true, false, false, false, TEXT("Instantaneous"), true, true, true, false, false, false, false, false, false, 2, 4, true, EDamageType::Force, 2, 0, false, 0, false },
        { TEXT("Thunderwave"), 1, ESpellSchool::Evocation, TEXT("1 action"), 0, TEXT("Self (15-ft cube)"), true, true, false, false, TEXT("Instantaneous"), true, false, true, false, false, true, false, true, false, 2, 8, false, EDamageType::Thunder, 1, 0, false, 0, false },
        { TEXT("Shield"), 1, ESpellSchool::Abjuration, TEXT("1 reaction"), 0, TEXT("Self"), true, true, false, false, TEXT("1 round"), false, false, false, false, false, true, false, true, false, 0, 0, false, EDamageType::Force, 0, 5, true, 0, false },
        { TEXT("Shield of Faith"), 1, ESpellSchool::Abjuration, TEXT("1 bonus action"), 60, TEXT("60 ft"), true, true, true, true, TEXT("Concentration, up to 10 minutes"), false, true, false, true, false, false, false, false, false, 0, 0, false, EDamageType::Force, 0, 2, false, 0, false },
        { TEXT("Guidance"), 0, ESpellSchool::Divination, TEXT("1 action"), 0, TEXT("Touch"), true, true, false, true, TEXT("Concentration, up to 1 minute"), false, true, true, false, false, false, false, false, true, 1, 4, false, EDamageType::Force, 0, 0, false, 0, false },
        { TEXT("Bless"), 1, ESpellSchool::Enchantment, TEXT("1 action"), 30, TEXT("30 ft"), true, true, true, true, TEXT("Concentration, up to 1 minute"), false, true, false, true, false, false, false, false, false, 1, 4, false, EDamageType::Force, 0, 0, false, 0, false },
        { TEXT("Mage Armor"), 1, ESpellSchool::Abjuration, TEXT("1 action"), 0, TEXT("Touch"), true, true, true, false, TEXT("8 hours"), false, false, false, false, false, true, false, true, false, 0, 0, false, EDamageType::Force, 0, 0, false, 0, false },
        { TEXT("Ray of Frost"), 0, ESpellSchool::Evocation, TEXT("1 action"), 60, TEXT("60 ft"), true, true, false, false, TEXT("Instantaneous"), false, false, false, false, false, true, false, true, true, 1, 8, false, EDamageType::Cold, 0, 0, false, 0, false },
        { TEXT("Shocking Grasp"), 0, ESpellSchool::Evocation, TEXT("1 action"), 0, TEXT("Touch"), true, true, false, false, TEXT("Instantaneous"), false, false, false, false, false, true, false, true, true, 1, 8, false, EDamageType::Lightning, 0, 0, false, 0, false },
        { TEXT("Light"), 0, ESpellSchool::Evocation, TEXT("1 action"), 0, TEXT("Touch"), true, false, true, false, TEXT("1 hour"), true, true, false, false, false, true, false, true, true, 0, 0, false, EDamageType::Force, 0, 0, false, 0, false },
        { TEXT("Inflict Wounds"), 1, ESpellSchool::Necromancy, TEXT("1 action"), 0, TEXT("Touch"), true, true, false, false, TEXT("Instantaneous"), false, true, false, false, false, false, false, false, false, 2, 10, false, EDamageType::Necrotic, 1, 0, false, 0, false },
        { TEXT("Bane"), 1, ESpellSchool::Enchantment, TEXT("1 action"), 30, TEXT("30 ft"), true, true, true, true, TEXT("Concentration, up to 1 minute"), true, true, false, false, false, false, false, false, false, 1, 4, false, EDamageType::Force, 0, 0, false, 0, false },
        { TEXT("Sanctuary"), 1, ESpellSchool::Abjuration, TEXT("1 bonus action"), 30, TEXT("30 ft"), true, true, true, false, TEXT("1 minute"), false, true, false, false, false, false, false, false, true, 0, 0, false, EDamageType::Force, 0, 0, false, 0, false },
        { TEXT("Feather Fall"), 1, ESpellSchool::Transmutation, TEXT("1 reaction"), 60, TEXT("60 ft"), true, false, true, false, TEXT("1 minute"), true, false, false, false, false, true, false, true, true, 0, 0, false, EDamageType::Force, 0, 0, false, 0, false },
        { TEXT("Detect Magic"), 1, ESpellSchool::Divination, TEXT("1 action"), 0, TEXT("Self"), true, true, false, true, TEXT("Concentration, up to 10 minutes"), true, true, true, true, true, true, false, true, true, 0, 0, false, EDamageType::Force, 0, 0, false, 0, true },
        { TEXT("Sleep"), 1, ESpellSchool::Enchantment, TEXT("1 action"), 60, TEXT("60 ft"), true, true, true, true, TEXT("Concentration, up to 1 minute"), true, false, false, false, false, true, false, true, false, 0, 0, false, EDamageType::Force, 0, 0, false, 0, false },
        { TEXT("Misty Step"), 2, ESpellSchool::Conjuration, TEXT("1 bonus action"), 0, TEXT("Self"), true, false, false, false, TEXT("Instantaneous"), false, false, false, false, false, true, true, true, false, 0, 0, false, EDamageType::Force, 0, 0, false, 0, false },
        { TEXT("Spiritual Weapon"), 2, ESpellSchool::Evocation, TEXT("1 bonus action"), 60, TEXT("60 ft"), true, true, false, true, TEXT("Concentration, up to 1 minute"), false, true, false, false, false, false, false, false, false, 1, 8, false, EDamageType::Force, 1, 0, false, 0, false },
        { TEXT("Scorching Ray"), 2, ESpellSchool::Evocation, TEXT("1 action"), 120, TEXT("120 ft"), true, true, false, false, TEXT("Instantaneous"), false, false, false, false, false, true, false, true, false, 6, 6, false, EDamageType::Fire, 2, 0, false, 0, false },
        { TEXT("Hold Person"), 2, ESpellSchool::Enchantment, TEXT("1 action"), 60, TEXT("60 ft"), true, true, true, true, TEXT("Concentration, up to 1 minute"), true, true, true, false, false, true, true, true, false, 0, 0, false, EDamageType::Force, 0, 0, false, 0, false },
        { TEXT("Aid"), 2, ESpellSchool::Abjuration, TEXT("1 action"), 30, TEXT("30 ft"), true, true, true, false, TEXT("8 hours"), false, true, false, true, false, false, false, false, true, 0, 0, false, EDamageType::Force, 0, 0, false, 5, false },
    };

    // Full casters (Bard, Cleric, Druid, Sorcerer, Wizard): character levels 1-5.
    static const int32 FullCaster[5][3] = { {2, 0, 0}, {3, 0, 0}, {4, 2, 0}, {4, 3, 0}, {4, 3, 2} };
    // Half casters (Paladin, Ranger) in SRD 5.2.1 pp. 53 and 58: two 1st-level slots
    // at level 1; 2nd-level slots from 5. Distinct from the Multiclass Spellcaster table.
    static const int32 HalfCaster[5][3] = { {2, 0, 0}, {2, 0, 0}, {3, 0, 0}, {3, 0, 0}, {4, 2, 0} };
    // Artificer: half-caster that starts at level 1.
    static const int32 ArtificerCaster[5][3] = { {2, 0, 0}, {2, 0, 0}, {3, 0, 0}, {3, 0, 0}, {4, 2, 0} };
    // Warlock pact: count and slot level by character level (slots live only at MaxSlotLevel).
    // L1: 1x1st; L2: 2x1st; L3-4: 2x2nd; L5: 2x3rd.
    static const int32 WarlockPactCount[5] = { 1, 2, 2, 2, 2 };
    static const int32 WarlockPactLevel[5] = { 1, 1, 2, 2, 3 };
}

const FSpellFacts& FSpellRules::Facts(ESpell Spell)
{
    return SpellTable::Spells[FMath::Clamp((int32)Spell, 0, (int32)ESpell::Count - 1)];
}

const TCHAR* FSpellRules::SchoolName(ESpellSchool School)
{
    static const TCHAR* Names[] = { TEXT("abjuration"), TEXT("conjuration"), TEXT("divination"), TEXT("enchantment"), TEXT("evocation"), TEXT("illusion"), TEXT("necromancy"), TEXT("transmutation") };
    return Names[FMath::Clamp((int32)School, 0, 7)];
}

int32 FSpellRules::SlotsFor(EAdventurerRole Role, int32 CharacterLevel, int32 SpellLevel)
{
    if (CharacterLevel < 1 || SpellLevel < 1 || SpellLevel > MaxSpellLevel) { return 0; }
    const int32 Row = FMath::Clamp(CharacterLevel, 1, EngineLevelCap) - 1;
    switch (Role)
    {
    case EAdventurerRole::Bard:
    case EAdventurerRole::Cleric:
    case EAdventurerRole::Druid:
    case EAdventurerRole::Sorcerer:
    case EAdventurerRole::Wizard:
        return SpellTable::FullCaster[Row][SpellLevel - 1];
    case EAdventurerRole::Paladin:
    case EAdventurerRole::Ranger:
        return SpellTable::HalfCaster[Row][SpellLevel - 1];
    case EAdventurerRole::Artificer:
        return SpellTable::ArtificerCaster[Row][SpellLevel - 1];
    case EAdventurerRole::Warlock:
        return SpellTable::WarlockPactLevel[Row] == SpellLevel ? SpellTable::WarlockPactCount[Row] : 0;
    default:
        return 0;
    }
}

int32 FSpellRules::MaxSlotLevel(EAdventurerRole Role, int32 CharacterLevel)
{
    if (Role == EAdventurerRole::Warlock)
    {
        if (CharacterLevel < 1) { return 0; }
        return SpellTable::WarlockPactLevel[FMath::Clamp(CharacterLevel, 1, EngineLevelCap) - 1];
    }
    int32 Best = 0;
    for (int32 SpellLevel = 1; SpellLevel <= MaxSpellLevel; ++SpellLevel)
    {
        if (SlotsFor(Role, CharacterLevel, SpellLevel) > 0) { Best = SpellLevel; }
    }
    return Best;
}

bool FSpellRules::OnClassList(EAdventurerRole Role, ESpell Spell)
{
    const FSpellFacts& F = Facts(Spell);
    switch (Role)
    {
    case EAdventurerRole::Bard: return F.bBard;
    case EAdventurerRole::Cleric: return F.bCleric;
    case EAdventurerRole::Druid: return F.bDruid;
    case EAdventurerRole::Paladin: return F.bPaladin;
    case EAdventurerRole::Ranger: return F.bRanger;
    case EAdventurerRole::Sorcerer: return F.bSorcerer;
    case EAdventurerRole::Warlock: return F.bWarlock;
    case EAdventurerRole::Wizard: return F.bWizard;
    case EAdventurerRole::Artificer: return F.bArtificer;
    default: return false;
    }
}

TArray<ESpell> FSpellRules::SpellsOnList(EAdventurerRole Role, int32 MaxSpellLevelFilter)
{
    TArray<ESpell> Out;
    for (int32 I = 0; I < (int32)ESpell::Count; ++I)
    {
        const ESpell Spell = static_cast<ESpell>(I);
        const FSpellFacts& F = Facts(Spell);
        if (OnClassList(Role, Spell) && F.Level <= MaxSpellLevelFilter) { Out.Add(Spell); }
    }
    return Out;
}

int32 FSpellRules::UpcastDiceCount(ESpell Spell, int32 SlotLevel)
{
    const FSpellFacts& F = Facts(Spell);
    if (F.Level == 0) { return F.DieCount; }
    if (SlotLevel < F.Level || SlotLevel > MaxSpellLevel) { return 0; }
    return F.DieCount + F.UpcastDicePerLevel * (SlotLevel - F.Level);
}

FString FSpellRules::Summary(ESpell Spell)
{
    const FSpellFacts& F = Facts(Spell);
    static const TCHAR* Ordinals[] = { TEXT(""), TEXT("1st"), TEXT("2nd"), TEXT("3rd") };
    const FString LevelText = F.Level == 0 ? FString::Printf(TEXT("%s cantrip"), SchoolName(F.School)) :
        FString::Printf(TEXT("%s-level %s"), Ordinals[FMath::Clamp(F.Level, 1, 3)], SchoolName(F.School));
    TArray<FString> Parts;
    if (F.bVerbal) { Parts.Add(TEXT("V")); }
    if (F.bSomatic) { Parts.Add(TEXT("S")); }
    if (F.bMaterial) { Parts.Add(TEXT("M")); }
    return FString::Printf(TEXT("%s | %s | %s | %s | %s"), *LevelText, F.CastingTime, F.RangeText, *FString::Join(Parts, TEXT(", ")), F.Duration);
}
