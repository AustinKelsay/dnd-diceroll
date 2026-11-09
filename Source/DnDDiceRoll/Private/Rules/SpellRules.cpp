#include "Rules/SpellRules.h"
#include "Combat/CombatBattle.h"

namespace SpellTable
{
    // Name, Level, School, Time, Feet, RangeText, V, S, M, Conc, Duration, Cleric, Wizard, Count, Sides, Healing, Type, Upcast, AcBonus, NegateMM, FlatBonus, Ritual
    // Verified against SRD 5.2.1 spell entries (Silvarian / Bible5e / SRD text).
    static const FSpellFacts Spells[(int32)ESpell::Count] = {
        { TEXT("Fire Bolt"), 0, ESpellSchool::Evocation, TEXT("1 action"), 120, TEXT("120 ft"), true, true, false, false, TEXT("Instantaneous"), false, true, 1, 10, false, EDamageType::Fire, 0, 0, false, 0, false },
        { TEXT("Sacred Flame"), 0, ESpellSchool::Evocation, TEXT("1 action"), 60, TEXT("60 ft"), true, true, false, false, TEXT("Instantaneous"), true, false, 1, 8, false, EDamageType::Radiant, 0, 0, false, 0, false },
        { TEXT("Magic Missile"), 1, ESpellSchool::Evocation, TEXT("1 action"), 120, TEXT("120 ft"), true, true, false, false, TEXT("Instantaneous"), false, true, 3, 4, false, EDamageType::Force, 1, 0, false, 1, false },
        { TEXT("Cure Wounds"), 1, ESpellSchool::Abjuration, TEXT("1 action"), 0, TEXT("Touch"), true, true, false, false, TEXT("Instantaneous"), true, false, 2, 8, true, EDamageType::Force, 2, 0, false, 0, false },
        { TEXT("Guiding Bolt"), 1, ESpellSchool::Evocation, TEXT("1 action"), 120, TEXT("120 ft"), true, true, false, false, TEXT("1 round"), true, false, 4, 6, false, EDamageType::Radiant, 1, 0, false, 0, false },
        { TEXT("Burning Hands"), 1, ESpellSchool::Evocation, TEXT("1 action"), 0, TEXT("Self (15-ft cone)"), true, true, false, false, TEXT("Instantaneous"), false, true, 3, 6, false, EDamageType::Fire, 1, 0, false, 0, false },
        { TEXT("Healing Word"), 1, ESpellSchool::Abjuration, TEXT("1 bonus action"), 60, TEXT("60 ft"), true, false, false, false, TEXT("Instantaneous"), true, false, 2, 4, true, EDamageType::Force, 2, 0, false, 0, false },
        { TEXT("Thunderwave"), 1, ESpellSchool::Evocation, TEXT("1 action"), 0, TEXT("Self (15-ft cube)"), true, true, false, false, TEXT("Instantaneous"), false, true, 2, 8, false, EDamageType::Thunder, 1, 0, false, 0, false },
        // Shield: reaction when hit or targeted by Magic Missile; +5 AC until start of next turn; no concentration.
        { TEXT("Shield"), 1, ESpellSchool::Abjuration, TEXT("1 reaction"), 0, TEXT("Self"), true, true, false, false, TEXT("1 round"), false, true, 0, 0, false, EDamageType::Force, 0, 5, true, 0, false },
        { TEXT("Shield of Faith"), 1, ESpellSchool::Abjuration, TEXT("1 bonus action"), 60, TEXT("60 ft"), true, true, true, true, TEXT("Concentration, up to 10 minutes"), true, false, 0, 0, false, EDamageType::Force, 0, 2, false, 0, false },
        { TEXT("Guidance"), 0, ESpellSchool::Divination, TEXT("1 action"), 0, TEXT("Touch"), true, true, false, true, TEXT("Concentration, up to 1 minute"), true, false, 1, 4, false, EDamageType::Force, 0, 0, false, 0, false },
        { TEXT("Bless"), 1, ESpellSchool::Enchantment, TEXT("1 action"), 30, TEXT("30 ft"), true, true, true, true, TEXT("Concentration, up to 1 minute"), true, false, 1, 4, false, EDamageType::Force, 0, 0, false, 0, false },
        { TEXT("Mage Armor"), 1, ESpellSchool::Abjuration, TEXT("1 action"), 0, TEXT("Touch"), true, true, true, false, TEXT("8 hours"), false, true, 0, 0, false, EDamageType::Force, 0, 0, false, 0, false },
        { TEXT("Ray of Frost"), 0, ESpellSchool::Evocation, TEXT("1 action"), 60, TEXT("60 ft"), true, true, false, false, TEXT("Instantaneous"), false, true, 1, 8, false, EDamageType::Cold, 0, 0, false, 0, false },
        { TEXT("Shocking Grasp"), 0, ESpellSchool::Evocation, TEXT("1 action"), 0, TEXT("Touch"), true, true, false, false, TEXT("Instantaneous"), false, true, 1, 8, false, EDamageType::Lightning, 0, 0, false, 0, false },
        // Light: Cleric and Wizard; V M; 1 hour; no concentration.
        { TEXT("Light"), 0, ESpellSchool::Evocation, TEXT("1 action"), 0, TEXT("Touch"), true, false, true, false, TEXT("1 hour"), true, true, 0, 0, false, EDamageType::Force, 0, 0, false, 0, false },
        // Inflict Wounds (5.2.1): Constitution save, 2d10 Necrotic half on success (was melee attack 3d10 in 5.1).
        { TEXT("Inflict Wounds"), 1, ESpellSchool::Necromancy, TEXT("1 action"), 0, TEXT("Touch"), true, true, false, false, TEXT("Instantaneous"), true, false, 2, 10, false, EDamageType::Necrotic, 1, 0, false, 0, false },
        { TEXT("Bane"), 1, ESpellSchool::Enchantment, TEXT("1 action"), 30, TEXT("30 ft"), true, true, true, true, TEXT("Concentration, up to 1 minute"), true, false, 1, 4, false, EDamageType::Force, 0, 0, false, 0, false },
        { TEXT("Sanctuary"), 1, ESpellSchool::Abjuration, TEXT("1 bonus action"), 30, TEXT("30 ft"), true, true, true, false, TEXT("1 minute"), true, false, 0, 0, false, EDamageType::Force, 0, 0, false, 0, false },
        { TEXT("Feather Fall"), 1, ESpellSchool::Transmutation, TEXT("1 reaction"), 60, TEXT("60 ft"), true, false, true, false, TEXT("1 minute"), false, true, 0, 0, false, EDamageType::Force, 0, 0, false, 0, false },
        // Detect Magic: ritual; concentration up to 10 minutes; Cleric and Wizard.
        { TEXT("Detect Magic"), 1, ESpellSchool::Divination, TEXT("1 action"), 0, TEXT("Self"), true, true, false, true, TEXT("Concentration, up to 10 minutes"), true, true, 0, 0, false, EDamageType::Force, 0, 0, false, 0, true },
        // Sleep (5.2.1): Wis save in 5-ft sphere, concentration 1 minute (was HP pool, no save, no concentration, 90 ft in 5.1).
        { TEXT("Sleep"), 1, ESpellSchool::Enchantment, TEXT("1 action"), 60, TEXT("60 ft"), true, true, true, true, TEXT("Concentration, up to 1 minute"), false, true, 0, 0, false, EDamageType::Force, 0, 0, false, 0, false },
        { TEXT("Misty Step"), 2, ESpellSchool::Conjuration, TEXT("1 bonus action"), 0, TEXT("Self"), true, false, false, false, TEXT("Instantaneous"), false, true, 0, 0, false, EDamageType::Force, 0, 0, false, 0, false },
        // Spiritual Weapon (5.2.1): NOW concentration (was not in 5.1); bonus action; 1d8 Force.
        { TEXT("Spiritual Weapon"), 2, ESpellSchool::Evocation, TEXT("1 bonus action"), 60, TEXT("60 ft"), true, true, false, true, TEXT("Concentration, up to 1 minute"), true, false, 1, 8, false, EDamageType::Force, 1, 0, false, 0, false },
        // Scorching Ray: 3 rays; DieCount/DieSides store total dice 6d6; upcast +2d6 per slot (one ray).
        { TEXT("Scorching Ray"), 2, ESpellSchool::Evocation, TEXT("1 action"), 120, TEXT("120 ft"), true, true, false, false, TEXT("Instantaneous"), false, true, 6, 6, false, EDamageType::Fire, 2, 0, false, 0, false },
        { TEXT("Hold Person"), 2, ESpellSchool::Enchantment, TEXT("1 action"), 60, TEXT("60 ft"), true, true, true, true, TEXT("Concentration, up to 1 minute"), true, true, 0, 0, false, EDamageType::Force, 0, 0, false, 0, false },
        // Aid: +5 max/current HP (FlatBonus); 8 hours; not concentration.
        { TEXT("Aid"), 2, ESpellSchool::Abjuration, TEXT("1 action"), 30, TEXT("30 ft"), true, true, true, false, TEXT("8 hours"), true, false, 0, 0, false, EDamageType::Force, 0, 0, false, 5, false },
    };
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
