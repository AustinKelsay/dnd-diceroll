#include "Rules/AdvancementRules.h"
#include "Combat/CombatBattle.h"
#include "Rules/ProficiencyRules.h"
#include "Rules/SpellRules.h"

namespace AdvancementTable
{
    // SRD 5.2.1 cumulative XP to reach level N (index = level; [0] unused).
    static const int32 XpToReach[16] = {
        0,
        0, 300, 900, 2700, 6500,
        14000, 23000, 34000, 48000, 64000,
        85000, 100000, 120000, 140000, 165000
    };

    // Full-caster slot table reused for multiclass spellcaster level (L1-5 -> spell levels 1-3).
    static const int32 FullCaster[5][3] = { {2, 0, 0}, {3, 0, 0}, {4, 2, 0}, {4, 3, 0}, {4, 3, 2} };
}

int32 FClassLevels::Get(EAdventurerRole Role) const
{
    switch (Role)
    {
    case EAdventurerRole::Fighter: return Fighter;
    case EAdventurerRole::Rogue: return Rogue;
    case EAdventurerRole::Cleric: return Cleric;
    case EAdventurerRole::Wizard: return Wizard;
    case EAdventurerRole::Barbarian: return Barbarian;
    case EAdventurerRole::Monk: return Monk;
    case EAdventurerRole::Paladin: return Paladin;
    case EAdventurerRole::Ranger: return Ranger;
    case EAdventurerRole::Sorcerer: return Sorcerer;
    case EAdventurerRole::Warlock: return Warlock;
    case EAdventurerRole::Druid: return Druid;
    case EAdventurerRole::Bard: return Bard;
    case EAdventurerRole::Artificer: return Artificer;
    default: return 0;
    }
}

void FClassLevels::Set(EAdventurerRole Role, int32 Levels)
{
    const int32 V = FMath::Max(0, Levels);
    switch (Role)
    {
    case EAdventurerRole::Fighter: Fighter = V; break;
    case EAdventurerRole::Rogue: Rogue = V; break;
    case EAdventurerRole::Cleric: Cleric = V; break;
    case EAdventurerRole::Wizard: Wizard = V; break;
    case EAdventurerRole::Barbarian: Barbarian = V; break;
    case EAdventurerRole::Monk: Monk = V; break;
    case EAdventurerRole::Paladin: Paladin = V; break;
    case EAdventurerRole::Ranger: Ranger = V; break;
    case EAdventurerRole::Sorcerer: Sorcerer = V; break;
    case EAdventurerRole::Warlock: Warlock = V; break;
    case EAdventurerRole::Druid: Druid = V; break;
    case EAdventurerRole::Bard: Bard = V; break;
    case EAdventurerRole::Artificer: Artificer = V; break;
    default: break;
    }
}

int32 FClassLevels::CharacterLevel() const
{
    return Fighter + Rogue + Cleric + Wizard + Barbarian + Monk + Paladin + Ranger + Sorcerer + Warlock + Druid + Bard + Artificer;
}

TArray<EAdventurerRole> FClassLevels::ClassesTaken() const
{
    TArray<EAdventurerRole> Out;
    static const EAdventurerRole Order[] = {
        EAdventurerRole::Fighter, EAdventurerRole::Rogue, EAdventurerRole::Cleric, EAdventurerRole::Wizard,
        EAdventurerRole::Barbarian, EAdventurerRole::Monk, EAdventurerRole::Paladin, EAdventurerRole::Ranger,
        EAdventurerRole::Sorcerer, EAdventurerRole::Warlock, EAdventurerRole::Druid, EAdventurerRole::Bard,
        EAdventurerRole::Artificer
    };
    for (EAdventurerRole Role : Order)
    {
        if (Get(Role) > 0) { Out.Add(Role); }
    }
    return Out;
}

int32 FAdvancementRules::XpForLevel(int32 Level)
{
    return AdvancementTable::XpToReach[FMath::Clamp(Level, 1, XpTableCap)];
}

int32 FAdvancementRules::LevelFromTotalXp(int32 TotalXp)
{
    if (TotalXp < 0) { return 1; }
    int32 Level = 1;
    for (int32 Candidate = 2; Candidate <= XpTableCap; ++Candidate)
    {
        if (TotalXp >= AdvancementTable::XpToReach[Candidate]) { Level = Candidate; }
        else { break; }
    }
    return Level;
}

int32 FAdvancementRules::XpToNextLevel(int32 CurrentLevel, int32 TotalXp)
{
    const int32 L = FMath::Clamp(CurrentLevel, 1, XpTableCap);
    if (L >= XpTableCap) { return 0; }
    return FMath::Max(0, AdvancementTable::XpToReach[L + 1] - TotalXp);
}

int32 FAdvancementRules::PendingLevelUps(int32 CurrentCharacterLevel, int32 TotalXp)
{
    const int32 FromXp = LevelFromTotalXp(TotalXp);
    const int32 Current = FMath::Clamp(CurrentCharacterLevel, 1, XpTableCap);
    return FMath::Max(0, FromXp - Current);
}

int32 FAdvancementRules::HitDieSides(EAdventurerRole Role)
{
    switch (Role)
    {
    case EAdventurerRole::Barbarian: return 12;
    case EAdventurerRole::Fighter:
    case EAdventurerRole::Paladin:
    case EAdventurerRole::Ranger: return 10;
    case EAdventurerRole::Artificer:
    case EAdventurerRole::Bard:
    case EAdventurerRole::Cleric:
    case EAdventurerRole::Druid:
    case EAdventurerRole::Monk:
    case EAdventurerRole::Rogue:
    case EAdventurerRole::Warlock: return 8;
    case EAdventurerRole::Sorcerer:
    case EAdventurerRole::Wizard: return 6;
    default: return 0;
    }
}

int32 FAdvancementRules::MaxHpAtLevel1(EAdventurerRole Role, int32 ConstitutionModifier)
{
    const int32 Die = HitDieSides(Role);
    if (Die <= 0) { return 1; }
    return FMath::Max(1, Die + ConstitutionModifier);
}

int32 FAdvancementRules::HpGainFromRoll(int32 HitDieRoll, int32 ConstitutionModifier)
{
    return FMath::Max(1, HitDieRoll + ConstitutionModifier);
}

int32 FAdvancementRules::HpGainForClassLevel(EAdventurerRole Role, int32 HitDieRoll, int32 ConstitutionModifier)
{
    const int32 Sides = HitDieSides(Role);
    if (Sides <= 0) { return 1; }
    const int32 Clamped = FMath::Clamp(HitDieRoll, 1, Sides);
    return HpGainFromRoll(Clamped, ConstitutionModifier);
}

int32 FAdvancementRules::ProficiencyBonus(int32 CharacterLevel)
{
    return FProficiencyRules::ProficiencyBonus(CharacterLevel);
}

int32 FAdvancementRules::SubclassChoiceLevel(EAdventurerRole Role)
{
    return Role == EAdventurerRole::None ? 0 : 3;
}

bool FAdvancementRules::OffersSubclassAt(EAdventurerRole Role, int32 NewClassLevel)
{
    const int32 Choice = SubclassChoiceLevel(Role);
    return Choice > 0 && NewClassLevel == Choice;
}

bool FAdvancementRules::OffersAsiAt(int32 NewCharacterLevel)
{
    static const int32 AsiLevels[] = { 4, 8, 12, 16, 19 };
    for (int32 Asi : AsiLevels)
    {
        if (Asi == NewCharacterLevel && Asi <= XpTableCap) { return true; }
    }
    return false;
}

int32 FAdvancementRules::CantripsKnown(EAdventurerRole Role, int32 ClassLevel)
{
    const int32 L = FMath::Clamp(ClassLevel, 1, EngineLevelCap);
    switch (Role)
    {
    case EAdventurerRole::Bard: return L >= 4 ? 3 : 2;
    case EAdventurerRole::Cleric:
    case EAdventurerRole::Druid: return 3;
    case EAdventurerRole::Sorcerer: return L >= 4 ? 5 : 4;
    case EAdventurerRole::Warlock: return L >= 4 ? 3 : 2;
    case EAdventurerRole::Wizard: return L >= 4 ? 4 : 3;
    case EAdventurerRole::Artificer: return 2;
    default: return 0;
    }
}

int32 FAdvancementRules::SpellsKnownOrPrepared(EAdventurerRole Role, int32 ClassLevel, int32 SpellcastingAbilityModifier)
{
    const int32 L = FMath::Clamp(ClassLevel, 1, EngineLevelCap);
    switch (Role)
    {
    case EAdventurerRole::Bard:
    {
        static const int32 Known[] = { 0, 4, 5, 6, 7, 8 };
        return Known[L];
    }
    case EAdventurerRole::Ranger:
    {
        static const int32 Known[] = { 0, 0, 2, 3, 3, 4 };
        return Known[L];
    }
    case EAdventurerRole::Sorcerer:
    case EAdventurerRole::Warlock:
    {
        static const int32 Known[] = { 0, 2, 3, 4, 5, 6 };
        return Known[L];
    }
    case EAdventurerRole::Cleric:
    case EAdventurerRole::Druid:
    case EAdventurerRole::Paladin:
    case EAdventurerRole::Wizard:
    case EAdventurerRole::Artificer:
        return FMath::Max(1, SpellcastingAbilityModifier + L);
    default:
        return 0;
    }
}

bool FAdvancementRules::MeetsMulticlassPrerequisite(EAdventurerRole Role, int32 Str, int32 Dex, int32 Con, int32 Intelligence, int32 Wis, int32 Cha)
{
    (void)Con;
    switch (Role)
    {
    case EAdventurerRole::Barbarian: return Str >= MulticlassAbilityPrerequisite;
    case EAdventurerRole::Bard: return Cha >= MulticlassAbilityPrerequisite;
    case EAdventurerRole::Cleric: return Wis >= MulticlassAbilityPrerequisite;
    case EAdventurerRole::Druid: return Wis >= MulticlassAbilityPrerequisite;
    case EAdventurerRole::Fighter: return Str >= MulticlassAbilityPrerequisite || Dex >= MulticlassAbilityPrerequisite;
    case EAdventurerRole::Monk: return Dex >= MulticlassAbilityPrerequisite && Wis >= MulticlassAbilityPrerequisite;
    case EAdventurerRole::Paladin: return Str >= MulticlassAbilityPrerequisite && Cha >= MulticlassAbilityPrerequisite;
    case EAdventurerRole::Ranger: return Dex >= MulticlassAbilityPrerequisite && Wis >= MulticlassAbilityPrerequisite;
    case EAdventurerRole::Rogue: return Dex >= MulticlassAbilityPrerequisite;
    case EAdventurerRole::Sorcerer: return Cha >= MulticlassAbilityPrerequisite;
    case EAdventurerRole::Warlock: return Cha >= MulticlassAbilityPrerequisite;
    case EAdventurerRole::Wizard: return Intelligence >= MulticlassAbilityPrerequisite;
    case EAdventurerRole::Artificer: return Intelligence >= MulticlassAbilityPrerequisite;
    default: return false;
    }
}

TArray<FString> FAdvancementRules::MulticlassProficienciesGained(EAdventurerRole NewClass)
{
    // SRD multiclass proficiency columns (weapons/armor/tools/skills abbreviated as labels for menus).
    TArray<FString> Out;
    switch (NewClass)
    {
    case EAdventurerRole::Barbarian:
        Out.Add(TEXT("Shields")); Out.Add(TEXT("Simple weapons")); Out.Add(TEXT("Martial weapons")); break;
    case EAdventurerRole::Bard:
        Out.Add(TEXT("Light armor")); Out.Add(TEXT("One skill")); Out.Add(TEXT("One musical instrument")); break;
    case EAdventurerRole::Cleric:
        Out.Add(TEXT("Light armor")); Out.Add(TEXT("Medium armor")); Out.Add(TEXT("Shields")); break;
    case EAdventurerRole::Druid:
        Out.Add(TEXT("Light armor")); Out.Add(TEXT("Medium armor")); Out.Add(TEXT("Shields (nonmetal)")); break;
    case EAdventurerRole::Fighter:
        Out.Add(TEXT("Light armor")); Out.Add(TEXT("Medium armor")); Out.Add(TEXT("Shields"));
        Out.Add(TEXT("Simple weapons")); Out.Add(TEXT("Martial weapons")); break;
    case EAdventurerRole::Monk:
        Out.Add(TEXT("Simple weapons")); Out.Add(TEXT("Shortswords")); break;
    case EAdventurerRole::Paladin:
        Out.Add(TEXT("Light armor")); Out.Add(TEXT("Medium armor")); Out.Add(TEXT("Shields"));
        Out.Add(TEXT("Simple weapons")); Out.Add(TEXT("Martial weapons")); break;
    case EAdventurerRole::Ranger:
        Out.Add(TEXT("Light armor")); Out.Add(TEXT("Medium armor")); Out.Add(TEXT("Shields"));
        Out.Add(TEXT("Simple weapons")); Out.Add(TEXT("Martial weapons")); Out.Add(TEXT("One skill")); break;
    case EAdventurerRole::Rogue:
        Out.Add(TEXT("Light armor")); Out.Add(TEXT("One skill")); Out.Add(TEXT("Thieves' tools")); break;
    case EAdventurerRole::Sorcerer:
        break;
    case EAdventurerRole::Warlock:
        Out.Add(TEXT("Light armor")); Out.Add(TEXT("Simple weapons")); break;
    case EAdventurerRole::Wizard:
        break;
    case EAdventurerRole::Artificer:
        Out.Add(TEXT("Light armor")); Out.Add(TEXT("Medium armor")); Out.Add(TEXT("Shields"));
        Out.Add(TEXT("Thieves' tools")); Out.Add(TEXT("Tinker's tools")); break;
    default:
        break;
    }
    return Out;
}

int32 FAdvancementRules::MulticlassSpellcasterLevel(const FClassLevels& Levels)
{
    // SRD: full casters contribute fully; Paladin/Ranger half (floor); Artificer half rounded up; Warlock excluded (pact).
    const int32 Full = Levels.Bard + Levels.Cleric + Levels.Druid + Levels.Sorcerer + Levels.Wizard;
    const int32 HalfFloor = (Levels.Paladin + Levels.Ranger) / 2;
    const int32 Art = (Levels.Artificer + 1) / 2; // round up
    return Full + HalfFloor + Art;
}

int32 FAdvancementRules::MulticlassSlotsFor(const FClassLevels& Levels, int32 SpellLevel)
{
    if (SpellLevel < 1 || SpellLevel > FSpellRules::MaxSpellLevel) { return 0; }
    const int32 CasterLevel = MulticlassSpellcasterLevel(Levels);
    if (CasterLevel < 1) { return 0; }
    const int32 Row = FMath::Clamp(CasterLevel, 1, FSpellRules::EngineLevelCap) - 1;
    return AdvancementTable::FullCaster[Row][SpellLevel - 1];
}

FClassAuraColor FAdvancementRules::ClassColor(EAdventurerRole Role)
{
    // Candlelit low-poly tints per class (RGB).
    switch (Role)
    {
    case EAdventurerRole::Fighter: return { 200, 80, 60 };      // ember red
    case EAdventurerRole::Rogue: return { 120, 200, 90 };       // green
    case EAdventurerRole::Cleric: return { 240, 220, 120 };     // holy gold
    case EAdventurerRole::Wizard: return { 90, 140, 255 };      // arcane blue
    case EAdventurerRole::Barbarian: return { 220, 100, 40 };   // rage orange
    case EAdventurerRole::Monk: return { 180, 240, 255 };       // pale sky
    case EAdventurerRole::Paladin: return { 255, 210, 90 };     // bright gold
    case EAdventurerRole::Ranger: return { 70, 160, 90 };       // forest
    case EAdventurerRole::Sorcerer: return { 200, 90, 220 };    // magenta
    case EAdventurerRole::Warlock: return { 140, 70, 200 };     // violet
    case EAdventurerRole::Druid: return { 100, 180, 70 };       // leaf
    case EAdventurerRole::Bard: return { 255, 140, 180 };       // rose
    case EAdventurerRole::Artificer: return { 180, 160, 100 };  // brass
    default: return { 255, 200, 80 };
    }
}

FClassAuraColor FAdvancementRules::BlendClassColors(const FClassLevels& Levels)
{
    const TArray<EAdventurerRole> Taken = Levels.ClassesTaken();
    if (Taken.Num() == 0) { return { 255, 200, 80 }; }
    if (Taken.Num() == 1) { return ClassColor(Taken[0]); }
    int32 SumR = 0, SumG = 0, SumB = 0, Weight = 0;
    for (EAdventurerRole Role : Taken)
    {
        const int32 W = Levels.Get(Role);
        const FClassAuraColor C = ClassColor(Role);
        SumR += int32(C.R) * W;
        SumG += int32(C.G) * W;
        SumB += int32(C.B) * W;
        Weight += W;
    }
    if (Weight <= 0) { return { 255, 200, 80 }; }
    return { uint8(SumR / Weight), uint8(SumG / Weight), uint8(SumB / Weight) };
}
