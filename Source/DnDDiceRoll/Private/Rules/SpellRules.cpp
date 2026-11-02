#include "Rules/SpellRules.h"
#include "Combat/CombatBattle.h"
#include "Rules/LevelRules.h"

namespace SpellTable
{
    // Name, Level, School, Time, Feet, RangeText, V, S, M, Duration, Cleric, Wizard, Count, Sides, Healing, Type, Upcast
    static const FSpellFacts Spells[(int32)ESpell::Count] = {
        { TEXT("Fire Bolt"), 0, ESpellSchool::Evocation, TEXT("1 action"), 120, TEXT("120 ft"), true, true, false, TEXT("Instantaneous"), false, true, 1, 10, false, EDamageType::Fire, 0 },
        { TEXT("Sacred Flame"), 0, ESpellSchool::Evocation, TEXT("1 action"), 60, TEXT("60 ft"), true, true, false, TEXT("Instantaneous"), true, false, 1, 8, false, EDamageType::Radiant, 0 },
        { TEXT("Magic Missile"), 1, ESpellSchool::Evocation, TEXT("1 action"), 120, TEXT("120 ft"), true, true, false, TEXT("Instantaneous"), false, true, 3, 4, false, EDamageType::Force, 1 },
        { TEXT("Cure Wounds"), 1, ESpellSchool::Evocation, TEXT("1 action"), 0, TEXT("Touch"), true, true, false, TEXT("Instantaneous"), true, false, 1, 8, true, EDamageType::Force, 1 },
        { TEXT("Guiding Bolt"), 1, ESpellSchool::Evocation, TEXT("1 action"), 120, TEXT("120 ft"), true, true, false, TEXT("1 round"), true, false, 4, 6, false, EDamageType::Radiant, 1 },
        { TEXT("Burning Hands"), 1, ESpellSchool::Evocation, TEXT("1 action"), 0, TEXT("Self (15-ft cone)"), true, true, false, TEXT("Instantaneous"), false, true, 3, 6, false, EDamageType::Fire, 1 },
        { TEXT("Healing Word"), 1, ESpellSchool::Evocation, TEXT("1 bonus action"), 60, TEXT("60 ft"), true, false, false, TEXT("Instantaneous"), true, false, 1, 4, true, EDamageType::Force, 1 },
        { TEXT("Thunderwave"), 1, ESpellSchool::Evocation, TEXT("1 action"), 0, TEXT("Self (15-ft cube)"), true, true, false, TEXT("Instantaneous"), false, true, 2, 8, false, EDamageType::Thunder, 1 },
    };
    // Rows: character level 1-5; columns: spell level 1-3.
    static const int32 FullCaster[5][3] = { {2, 0, 0}, {3, 0, 0}, {4, 2, 0}, {4, 3, 0}, {4, 3, 2} };
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
    switch (Role)
    {
    case EAdventurerRole::Bard: case EAdventurerRole::Cleric: case EAdventurerRole::Druid: case EAdventurerRole::Sorcerer: case EAdventurerRole::Wizard:
        return SpellTable::FullCaster[FMath::Min(CharacterLevel, 5) - 1][SpellLevel - 1];
    case EAdventurerRole::Paladin: case EAdventurerRole::Ranger:
        // SRD half casters: no Spellcasting before class level 2; their table equals the full-caster row at ceil(level / 2).
        return CharacterLevel < 2 ? 0 : SpellTable::FullCaster[FMath::Min((CharacterLevel + 1) / 2, 5) - 1][SpellLevel - 1];
    case EAdventurerRole::Warlock:
    {
        // SRD Pact Magic (levels 1-5): 1/2/2/2/2 slots, all of slot level 1/1/2/2/3.
        const int32 PactLevel = CharacterLevel >= 5 ? 3 : CharacterLevel >= 3 ? 2 : 1;
        return SpellLevel == PactLevel ? (CharacterLevel >= 2 ? 2 : 1) : 0;
    }
    default:
        return 0;
    }
}

int32 FSpellRules::MaxSlotLevel(EAdventurerRole Role, int32 CharacterLevel)
{
    int32 Best = 0;
    for (int32 SpellLevel = 1; SpellLevel <= MaxSpellLevel; ++SpellLevel) { if (SlotsFor(Role, CharacterLevel, SpellLevel) > 0) { Best = SpellLevel; } }
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
        return SpellLevel > MaxSpellLevel ? 0 : FLevelRules::MulticlassSlots(FLevelRules::CasterLevel(Primary, PrimaryLevels) + FLevelRules::CasterLevel(Secondary, SecondaryLevels), SpellLevel);
    }
    return bSecondary ? SlotsFor(Secondary, SecondaryLevels, SpellLevel) : SlotsFor(Primary, PrimaryLevels, SpellLevel);
}

bool FSpellRules::OnClassList(EAdventurerRole Role, ESpell Spell)
{
    const FSpellFacts& F = Facts(Spell);
    return (Role == EAdventurerRole::Cleric && F.bCleric) || (Role == EAdventurerRole::Wizard && F.bWizard);
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
