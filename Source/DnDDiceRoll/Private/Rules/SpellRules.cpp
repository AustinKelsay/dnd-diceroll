#include "Rules/SpellRules.h"
#include "Combat/CombatBattle.h"

namespace SpellTable
{
    // Name, Level, School, Time, Feet, RangeText, V, S, M, Conc, Duration, Cleric, Wizard, Count, Sides, Healing, Type, Upcast, AcBonus, NegateMM
    // Verified against SRD 5.2.1 spell entries (healing school/dice updated from 5.1).
    static const FSpellFacts Spells[(int32)ESpell::Count] = {
        { TEXT("Fire Bolt"), 0, ESpellSchool::Evocation, TEXT("1 action"), 120, TEXT("120 ft"), true, true, false, false, TEXT("Instantaneous"), false, true, 1, 10, false, EDamageType::Fire, 0, 0, false },
        { TEXT("Sacred Flame"), 0, ESpellSchool::Evocation, TEXT("1 action"), 60, TEXT("60 ft"), true, true, false, false, TEXT("Instantaneous"), true, false, 1, 8, false, EDamageType::Radiant, 0, 0, false },
        { TEXT("Magic Missile"), 1, ESpellSchool::Evocation, TEXT("1 action"), 120, TEXT("120 ft"), true, true, false, false, TEXT("Instantaneous"), false, true, 3, 4, false, EDamageType::Force, 1, 0, false },
        { TEXT("Cure Wounds"), 1, ESpellSchool::Abjuration, TEXT("1 action"), 0, TEXT("Touch"), true, true, false, false, TEXT("Instantaneous"), true, false, 2, 8, true, EDamageType::Force, 2, 0, false },
        { TEXT("Guiding Bolt"), 1, ESpellSchool::Evocation, TEXT("1 action"), 120, TEXT("120 ft"), true, true, false, false, TEXT("1 round"), true, false, 4, 6, false, EDamageType::Radiant, 1, 0, false },
        { TEXT("Burning Hands"), 1, ESpellSchool::Evocation, TEXT("1 action"), 0, TEXT("Self (15-ft cone)"), true, true, false, false, TEXT("Instantaneous"), false, true, 3, 6, false, EDamageType::Fire, 1, 0, false },
        { TEXT("Healing Word"), 1, ESpellSchool::Abjuration, TEXT("1 bonus action"), 60, TEXT("60 ft"), true, false, false, false, TEXT("Instantaneous"), true, false, 2, 4, true, EDamageType::Force, 2, 0, false },
        { TEXT("Thunderwave"), 1, ESpellSchool::Evocation, TEXT("1 action"), 0, TEXT("Self (15-ft cube)"), true, true, false, false, TEXT("Instantaneous"), false, true, 2, 8, false, EDamageType::Thunder, 1, 0, false },
        // Shield: reaction when hit or targeted by Magic Missile; +5 AC until start of next turn; no concentration; MM deals no damage.
        { TEXT("Shield"), 1, ESpellSchool::Abjuration, TEXT("1 reaction"), 0, TEXT("Self"), true, true, false, false, TEXT("1 round"), false, true, 0, 0, false, EDamageType::Force, 0, 5, true },
        // Shield of Faith: concentration up to 10 minutes; +2 AC (Cleric).
        { TEXT("Shield of Faith"), 1, ESpellSchool::Abjuration, TEXT("1 bonus action"), 60, TEXT("60 ft"), true, true, true, true, TEXT("Concentration, up to 10 minutes"), true, false, 0, 0, false, EDamageType::Force, 0, 2, false },
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
    if ((Role != EAdventurerRole::Cleric && Role != EAdventurerRole::Wizard) || CharacterLevel < 1 || SpellLevel < 1 || SpellLevel > MaxSpellLevel) { return 0; }
    return SpellTable::FullCaster[FMath::Min(CharacterLevel, 5) - 1][SpellLevel - 1];
}

int32 FSpellRules::MaxSlotLevel(EAdventurerRole Role, int32 CharacterLevel)
{
    int32 Best = 0;
    for (int32 SpellLevel = 1; SpellLevel <= MaxSpellLevel; ++SpellLevel) { if (SlotsFor(Role, CharacterLevel, SpellLevel) > 0) { Best = SpellLevel; } }
    return Best;
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
