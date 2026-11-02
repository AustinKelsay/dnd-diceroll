#include "Rules/SpellRules.h"
#include "Combat/CombatBattle.h"
#include "Rules/LevelRules.h"
#include "Rules/SpellCatalogRules.h"

namespace SpellTable
{
    // Name, Level, School, Time, Feet, RangeText, V, S, M, Duration, Cleric, Wizard, Count, Sides, Healing, Type, Upcast
    static const FSpellFacts Spells[(int32)ESpell::Count] = {
        { TEXT("Fire Bolt"), 0, ESpellSchool::Evocation, TEXT("1 action"), 120, TEXT("120 ft"), true, true, false, TEXT("Instantaneous"), false, true, 1, 10, false, EDamageType::Fire, 0 },
        { TEXT("Sacred Flame"), 0, ESpellSchool::Evocation, TEXT("1 action"), 60, TEXT("60 ft"), true, true, false, TEXT("Instantaneous"), true, false, 1, 8, false, EDamageType::Radiant, 0 },
        { TEXT("Magic Missile"), 1, ESpellSchool::Evocation, TEXT("1 action"), 120, TEXT("120 ft"), true, true, false, TEXT("Instantaneous"), false, true, 3, 4, false, EDamageType::Force, 1 },
        { TEXT("Cure Wounds"), 1, ESpellSchool::Abjuration, TEXT("1 action"), 0, TEXT("Touch"), true, true, false, TEXT("Instantaneous"), true, false, 2, 8, true, EDamageType::Force, 2 }, // SRD 5.2.1: 2d8 + mod, +2d8 per slot level
        { TEXT("Guiding Bolt"), 1, ESpellSchool::Evocation, TEXT("1 action"), 120, TEXT("120 ft"), true, true, false, TEXT("1 round"), true, false, 4, 6, false, EDamageType::Radiant, 1 },
        { TEXT("Burning Hands"), 1, ESpellSchool::Evocation, TEXT("1 action"), 0, TEXT("Self (15-ft cone)"), true, true, false, TEXT("Instantaneous"), false, true, 3, 6, false, EDamageType::Fire, 1 },
        { TEXT("Healing Word"), 1, ESpellSchool::Abjuration, TEXT("1 bonus action"), 60, TEXT("60 ft"), true, false, false, TEXT("Instantaneous"), true, false, 2, 4, true, EDamageType::Force, 2 }, // SRD 5.2.1: 2d4 + mod, +2d4 per slot level
        { TEXT("Thunderwave"), 1, ESpellSchool::Evocation, TEXT("1 action"), 0, TEXT("Self (15-ft cube)"), true, true, false, TEXT("Instantaneous"), false, true, 2, 8, false, EDamageType::Thunder, 1 },
    };
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
    if (CharacterLevel < 1 || SpellLevel < 1 || SpellLevel > TableSpellLevel) { return 0; }
    CharacterLevel = FMath::Min(CharacterLevel, FLevelRules::MaxLevel);
    switch (Role)
    {
    case EAdventurerRole::Bard: case EAdventurerRole::Cleric: case EAdventurerRole::Druid: case EAdventurerRole::Sorcerer: case EAdventurerRole::Wizard:
        // The SRD full-caster table is the multiclass spellcaster table (one 20x9 data table in FLevelRules).
        return FLevelRules::MulticlassSlots(CharacterLevel, SpellLevel);
    case EAdventurerRole::Paladin: case EAdventurerRole::Ranger:
        // SRD half casters: no Spellcasting before class level 2; their table equals the full-caster row at ceil(level / 2).
        return CharacterLevel < 2 ? 0 : FLevelRules::MulticlassSlots((CharacterLevel + 1) / 2, SpellLevel);
    case EAdventurerRole::Warlock:
    {
        // SRD Pact Magic table (levels 1-20): slot level min(ceil(L/2), 5); 1 slot at 1, 2 at 2-10, 3 at 11-16, 4 at 17-20.
        const int32 PactLevel = FMath::Min((CharacterLevel + 1) / 2, 5);
        const int32 Count = CharacterLevel >= 17 ? 4 : CharacterLevel >= 11 ? 3 : CharacterLevel >= 2 ? 2 : 1;
        return SpellLevel == PactLevel ? Count : 0;
    }
    default:
        return 0;
    }
}

int32 FSpellRules::MaxSlotLevel(EAdventurerRole Role, int32 CharacterLevel)
{
    int32 Best = 0;
    for (int32 SpellLevel = 1; SpellLevel <= TableSpellLevel; ++SpellLevel) { if (SlotsFor(Role, CharacterLevel, SpellLevel) > 0) { Best = SpellLevel; } }
    return Best;
}

int32 FSpellRules::HeroSlotsFor(EAdventurerRole Primary, int32 PrimaryLevels, EAdventurerRole Secondary, int32 SecondaryLevels, int32 SpellLevel)
{
    // SRD multiclass spellcaster: with Spellcasting from two classes, add caster levels and read the multiclass table;
    // otherwise the one casting class uses its own table at its own class level. Pact Magic stays separate.
    const bool bPrimary = Primary != EAdventurerRole::Warlock && SlotsFor(Primary, PrimaryLevels, 1) > 0;
    const bool bSecondary = Secondary != EAdventurerRole::Warlock && SlotsFor(Secondary, SecondaryLevels, 1) > 0;
    if (bPrimary && bSecondary)
    {
        return SpellLevel > TableSpellLevel ? 0 : FLevelRules::MulticlassSlots(FLevelRules::CasterLevel(Primary, PrimaryLevels) + FLevelRules::CasterLevel(Secondary, SecondaryLevels), SpellLevel);
    }
    return bSecondary ? SlotsFor(Secondary, SecondaryLevels, SpellLevel) : SlotsFor(Primary, PrimaryLevels, SpellLevel);
}

bool FSpellRules::OnClassList(EAdventurerRole Role, ESpell Spell)
{
    // SRD 5.2.1 class spell lists from the full catalog (Bard, Cleric, Druid, Paladin, Ranger, Sorcerer, Warlock, Wizard).
    const FSrdSpell* Srd = FSpellCatalogRules::Find(Facts(Spell).Name);
    return Srd && FSpellCatalogRules::OnClassList(Role, *Srd);
}

int32 FSpellRules::UpcastDiceCount(ESpell Spell, int32 SlotLevel)
{
    const FSpellFacts& F = Facts(Spell);
    if (F.Level == 0) { return F.DieCount; }
    if (SlotLevel < F.Level || SlotLevel > TableSpellLevel) { return 0; }
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
    const FSrdSpell* Srd = FSpellCatalogRules::Find(F.Name);
    const FString Classes = Srd ? FSpellCatalogRules::ClassListText(Srd->Classes) : FString();
    return FString::Printf(TEXT("%s | %s | %s | %s | %s | %s"), *LevelText, *Classes, F.CastingTime, F.RangeText, *FString::Join(Parts, TEXT(", ")), F.Duration);
}
