#include "Rules/LevelRules.h"
#include "Combat/CombatBattle.h"

namespace
{
const int32 XpTable[FLevelRules::MaxLevel] = {0, 300, 900, 2700, 6500, 14000, 23000, 34000, 48000, 64000,
    85000, 100000, 120000, 140000, 165000, 195000, 225000, 265000, 305000, 355000};
const int32 SlotTable[20][9] = {
    {2,0,0,0,0,0,0,0,0}, {3,0,0,0,0,0,0,0,0}, {4,2,0,0,0,0,0,0,0}, {4,3,0,0,0,0,0,0,0}, {4,3,2,0,0,0,0,0,0},
    {4,3,3,0,0,0,0,0,0}, {4,3,3,1,0,0,0,0,0}, {4,3,3,2,0,0,0,0,0}, {4,3,3,3,1,0,0,0,0}, {4,3,3,3,2,0,0,0,0},
    {4,3,3,3,2,1,0,0,0}, {4,3,3,3,2,1,0,0,0}, {4,3,3,3,2,1,1,0,0}, {4,3,3,3,2,1,1,0,0}, {4,3,3,3,2,1,1,1,0},
    {4,3,3,3,2,1,1,1,0}, {4,3,3,3,2,1,1,1,1}, {4,3,3,3,3,1,1,1,1}, {4,3,3,3,3,2,1,1,1}, {4,3,3,3,3,2,2,1,1}};
}

int32 FLevelRules::XpForLevel(int32 Level) { return XpTable[FMath::Clamp(Level, 1, MaxLevel) - 1]; }

int32 FLevelRules::LevelForXp(int32 Xp)
{
    int32 Level = 1;
    while (Level < MaxLevel && Xp >= XpTable[Level]) { ++Level; }
    return Level;
}

int32 FLevelRules::HitDieSides(EAdventurerRole Role)
{
    switch (Role)
    {
    case EAdventurerRole::Barbarian: return 12;
    case EAdventurerRole::Fighter: case EAdventurerRole::Paladin: case EAdventurerRole::Ranger: return 10;
    case EAdventurerRole::Sorcerer: case EAdventurerRole::Wizard: return 6;
    case EAdventurerRole::None: return 0;
    default: return 8;
    }
}

int32 FLevelRules::AverageLevelHitPoints(EAdventurerRole Role, int32 ConstitutionModifier)
{
    return FMath::Max(1, HitDieSides(Role) / 2 + 1 + ConstitutionModifier);
}

int32 FLevelRules::MaxHitPoints(EAdventurerRole Primary, int32 PrimaryLevels, EAdventurerRole Secondary, int32 SecondaryLevels, int32 ConstitutionModifier)
{
    if (PrimaryLevels < 1) { return 0; }
    int32 Total = FMath::Max(1, HitDieSides(Primary) + ConstitutionModifier);
    Total += (PrimaryLevels - 1) * AverageLevelHitPoints(Primary, ConstitutionModifier);
    if (Secondary != EAdventurerRole::None) { Total += FMath::Max(0, SecondaryLevels) * AverageLevelHitPoints(Secondary, ConstitutionModifier); }
    return Total;
}

bool FLevelRules::IsAsiLevel(EAdventurerRole Role, int32 ClassLevel)
{
    if (ClassLevel == 4 || ClassLevel == 8 || ClassLevel == 12 || ClassLevel == 16 || ClassLevel == 19) { return true; }
    if (Role == EAdventurerRole::Fighter && (ClassLevel == 6 || ClassLevel == 14)) { return true; }
    return Role == EAdventurerRole::Rogue && ClassLevel == 10;
}

int32 FLevelRules::AsiCount(EAdventurerRole Role, int32 ClassLevel)
{
    int32 Count = 0;
    for (int32 L = 1; L <= ClassLevel; ++L) { Count += IsAsiLevel(Role, L) ? 1 : 0; }
    return Count;
}

bool FLevelRules::ApplyAsi(FAbilityScores& Scores, EAbility First, EAbility Second, FString& Error)
{
    FAbilityScores Next = Scores;
    auto Raise = [&Next](EAbility A, int32 By)
    {
        int32* Slots[] = {&Next.Strength, &Next.Dexterity, &Next.Constitution, &Next.Intelligence, &Next.Wisdom, &Next.Charisma};
        *Slots[static_cast<int32>(A)] += By;
    };
    if (First == Second) { Raise(First, 2); } else { Raise(First, 1); Raise(Second, 1); }
    for (int32 I = 0; I < FAbilityRules::AbilityCount; ++I)
    {
        if (Next.Get(static_cast<EAbility>(I)) > 20) { Error = TEXT("An Ability Score Improvement cannot raise a score above 20."); return false; }
    }
    Scores = Next;
    return true;
}

bool FLevelRules::MeetsMulticlassPrerequisite(EAdventurerRole Role, const FAbilityScores& S)
{
    switch (Role)
    {
    case EAdventurerRole::Barbarian: return S.Strength >= 13;
    case EAdventurerRole::Bard: case EAdventurerRole::Sorcerer: case EAdventurerRole::Warlock: return S.Charisma >= 13;
    case EAdventurerRole::Cleric: case EAdventurerRole::Druid: return S.Wisdom >= 13;
    case EAdventurerRole::Fighter: return S.Strength >= 13 || S.Dexterity >= 13;
    case EAdventurerRole::Monk: case EAdventurerRole::Ranger: return S.Dexterity >= 13 && S.Wisdom >= 13;
    case EAdventurerRole::Paladin: return S.Strength >= 13 && S.Charisma >= 13;
    case EAdventurerRole::Rogue: return S.Dexterity >= 13;
    case EAdventurerRole::Wizard: case EAdventurerRole::Artificer: return S.Intelligence >= 13;
    default: return false;
    }
}

bool FLevelRules::CanMulticlass(EAdventurerRole From, EAdventurerRole To, const FAbilityScores& Scores, FString& Error)
{
    if (From == To || To == EAdventurerRole::None) { Error = TEXT("Choose a different class to multiclass into."); return false; }
    if (!MeetsMulticlassPrerequisite(From, Scores)) { Error = TEXT("Your current class's multiclass prerequisite (13+) is not met."); return false; }
    if (!MeetsMulticlassPrerequisite(To, Scores)) { Error = TEXT("The new class's multiclass prerequisite (13+) is not met."); return false; }
    return true;
}

FString FLevelRules::MulticlassProficiencies(EAdventurerRole Role)
{
    switch (Role)
    {
    case EAdventurerRole::Barbarian: return TEXT("Shields, simple weapons, martial weapons");
    case EAdventurerRole::Bard: return TEXT("Light armor, one skill, one musical instrument");
    case EAdventurerRole::Cleric: case EAdventurerRole::Druid: return TEXT("Light armor, medium armor, shields");
    case EAdventurerRole::Fighter: case EAdventurerRole::Paladin: return TEXT("Light armor, medium armor, shields, simple weapons, martial weapons");
    case EAdventurerRole::Monk: return TEXT("Simple weapons, shortswords");
    case EAdventurerRole::Ranger: return TEXT("Light armor, medium armor, shields, simple weapons, martial weapons, one skill");
    case EAdventurerRole::Rogue: return TEXT("Light armor, one skill, thieves' tools");
    case EAdventurerRole::Warlock: return TEXT("Light armor, simple weapons");
    case EAdventurerRole::Artificer: return TEXT("Light armor, medium armor, shields, thieves' tools, tinker's tools");
    default: return TEXT("None");
    }
}

int32 FLevelRules::CasterLevel(EAdventurerRole Role, int32 ClassLevels)
{
    switch (Role)
    {
    case EAdventurerRole::Bard: case EAdventurerRole::Cleric: case EAdventurerRole::Druid: case EAdventurerRole::Sorcerer: case EAdventurerRole::Wizard: return ClassLevels;
    case EAdventurerRole::Paladin: case EAdventurerRole::Ranger: return ClassLevels / 2;
    case EAdventurerRole::Artificer: return (ClassLevels + 1) / 2;
    default: return 0;
    }
}

int32 FLevelRules::MulticlassSlots(int32 CasterLevel, int32 SpellLevel)
{
    if (CasterLevel < 1 || SpellLevel < 1 || SpellLevel > 9) { return 0; }
    return SlotTable[FMath::Min(CasterLevel, MaxLevel) - 1][SpellLevel - 1];
}