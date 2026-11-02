#include "Rules/AbilityRules.h"
#include "Rules/MonsterRules.h"
#include "Combat/CombatBattle.h"

int32 FAbilityScores::Get(EAbility Ability) const
{
    switch (Ability)
    {
    case EAbility::Strength: return Strength;
    case EAbility::Dexterity: return Dexterity;
    case EAbility::Constitution: return Constitution;
    case EAbility::Intelligence: return Intelligence;
    case EAbility::Wisdom: return Wisdom;
    default: return Charisma;
    }
}

bool FAbilityScores::IsValid() const
{
    for (int32 Index = 0; Index < FAbilityRules::AbilityCount; ++Index)
    {
        const int32 Score = Get(static_cast<EAbility>(Index));
        if (Score < FAbilityRules::MinScore || Score > FAbilityRules::MaxScore) { return false; }
    }
    return true;
}

int32 FAbilityRules::Modifier(int32 Score)
{
    // Integer floor division that also rounds toward negative infinity for scores below 10.
    const int32 Delta = Score - 10;
    return Delta >= 0 ? Delta / 2 : -((1 - Delta) / 2);
}

const TCHAR* FAbilityRules::AbilityName(EAbility Ability)
{
    static const TCHAR* Names[] = {TEXT("Strength"), TEXT("Dexterity"), TEXT("Constitution"), TEXT("Intelligence"), TEXT("Wisdom"), TEXT("Charisma")};
    return Names[FMath::Min<int32>(static_cast<int32>(Ability), AbilityCount - 1)];
}

const TCHAR* FAbilityRules::AbilityAbbreviation(EAbility Ability)
{
    static const TCHAR* Names[] = {TEXT("STR"), TEXT("DEX"), TEXT("CON"), TEXT("INT"), TEXT("WIS"), TEXT("CHA")};
    return Names[FMath::Min<int32>(static_cast<int32>(Ability), AbilityCount - 1)];
}

FAbilityScores FAbilityRules::PrebuiltScores(EAdventurerRole Role)
{
    auto Make = [](int32 Str, int32 Dex, int32 Con, int32 Int, int32 Wis, int32 Cha)
    {
        FAbilityScores Scores;
        Scores.Strength = Str; Scores.Dexterity = Dex; Scores.Constitution = Con;
        Scores.Intelligence = Int; Scores.Wisdom = Wis; Scores.Charisma = Cha;
        return Scores;
    };
    switch (Role)
    {
    case EAdventurerRole::Fighter: return Make(16, 12, 15, 8, 13, 10);
    case EAdventurerRole::Rogue: return Make(8, 16, 13, 12, 10, 14);
    case EAdventurerRole::Cleric: return Make(13, 12, 14, 10, 16, 8);
    case EAdventurerRole::Wizard: return Make(8, 12, 14, 16, 13, 10);
    // Remaining classes: key ability 16 (+3, so +5 to hit at level 1) and DEX/CON/WIS chosen to fit each profile's AC.
    case EAdventurerRole::Barbarian: return Make(16, 12, 16, 8, 10, 8);
    case EAdventurerRole::Monk: return Make(10, 16, 14, 8, 12, 10);
    case EAdventurerRole::Paladin: return Make(16, 10, 14, 8, 10, 14);
    case EAdventurerRole::Ranger: return Make(10, 16, 14, 8, 14, 10);
    case EAdventurerRole::Druid: return Make(10, 12, 14, 8, 16, 10);
    case EAdventurerRole::Bard: return Make(8, 14, 14, 10, 10, 16);
    case EAdventurerRole::Sorcerer: return Make(8, 12, 14, 10, 10, 16);
    case EAdventurerRole::Warlock: return Make(8, 12, 14, 10, 10, 16);
    case EAdventurerRole::Artificer: return Make(10, 12, 14, 16, 12, 8);
    default: return FAbilityScores();
    }
}

FAbilityScores FAbilityRules::EnemyScores(const FString& Role)
{
    if (const FMonsterStatBlock* Block = FMonsterRules::Find(Role)) { return Block->Scores; } // SRD 5.1 stat block foes
    // Legacy generic bandit-style foes; archers and scouts lean on Dexterity. Original values, not a stat block copy.
    FAbilityScores Scores;
    Scores.Strength = 12; Scores.Dexterity = 12; Scores.Constitution = 12;
    if (Role.Contains(TEXT("Archer")) || Role.Contains(TEXT("Scout"))) { Scores.Strength = 10; Scores.Dexterity = 14; Scores.Wisdom = 12; }
    return Scores;
}

EAbility FAbilityRules::AttackAbility(EAdventurerRole Role)
{
    switch (Role)
    {
    case EAdventurerRole::Rogue: return EAbility::Dexterity;
    case EAdventurerRole::Cleric: return EAbility::Wisdom;
    case EAdventurerRole::Wizard: return EAbility::Intelligence;
    case EAdventurerRole::Monk:
    case EAdventurerRole::Ranger: return EAbility::Dexterity;
    case EAdventurerRole::Druid: return EAbility::Wisdom;
    case EAdventurerRole::Bard:
    case EAdventurerRole::Sorcerer:
    case EAdventurerRole::Warlock: return EAbility::Charisma;
    case EAdventurerRole::Artificer: return EAbility::Intelligence;
    default: return EAbility::Strength;
    }
}
