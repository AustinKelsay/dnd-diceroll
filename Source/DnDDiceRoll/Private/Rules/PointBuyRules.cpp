#include "Rules/PointBuyRules.h"
#include "Combat/CombatBattle.h"

int32 FPointBuyRules::Cost(int32 Score)
{
    // SRD 5.2.1 p. 20, Ability Score Point Costs.
    static const int32 Costs[] = {0, 1, 2, 3, 4, 5, 7, 9};
    return Score < MinScore || Score > MaxScore ? INDEX_NONE : Costs[Score - MinScore];
}

int32 FPointBuyRules::TotalCost(const FAbilityScores& Scores)
{
    int32 Total = 0;
    for (int32 Index = 0; Index < FAbilityRules::AbilityCount; ++Index)
    {
        const int32 Points = Cost(Scores.Get(static_cast<EAbility>(Index)));
        if (Points == INDEX_NONE) { return INDEX_NONE; }
        Total += Points;
    }
    return Total;
}

int32 FPointBuyRules::Remaining(const FAbilityScores& Scores)
{
    const int32 Total = TotalCost(Scores);
    return Total == INDEX_NONE ? INDEX_NONE : Budget - Total;
}

bool FPointBuyRules::Validate(const FAbilityScores& Scores, FString& Error)
{
    const int32 Total = TotalCost(Scores);
    if (Total == INDEX_NONE) { Error = TEXT("Point-buy scores must be 8..15 before bonuses."); return false; }
    if (Total > Budget) { Error = FString::Printf(TEXT("Point buy spends %d of %d points."), Total, Budget); return false; }
    Error.Reset();
    return true;
}

int32 FPointBuyRules::RaiseCost(const FAbilityScores& Scores, EAbility Ability)
{
    const int32 Score = Scores.Get(Ability);
    const int32 Now = Cost(Score), Next = Cost(Score + 1);
    return Now == INDEX_NONE || Next == INDEX_NONE ? INDEX_NONE : Next - Now;
}

bool FPointBuyRules::CanRaise(const FAbilityScores& Scores, EAbility Ability)
{
    const int32 Step = RaiseCost(Scores, Ability), Left = Remaining(Scores);
    return Step != INDEX_NONE && Left != INDEX_NONE && Step <= Left;
}

bool FPointBuyRules::CanLower(const FAbilityScores& Scores, EAbility Ability)
{
    const int32 Score = Scores.Get(Ability);
    return Score > MinScore && Score <= MaxScore;
}

bool FPointBuyRules::Raise(FAbilityScores& Scores, EAbility Ability)
{
    if (!CanRaise(Scores, Ability)) { return false; }
    Set(Scores, Ability, Scores.Get(Ability) + 1);
    return true;
}

bool FPointBuyRules::Lower(FAbilityScores& Scores, EAbility Ability)
{
    if (!CanLower(Scores, Ability)) { return false; }
    Set(Scores, Ability, Scores.Get(Ability) - 1);
    return true;
}

int32 FPointBuyRules::ModifierForecast(const FAbilityScores& Scores, EAbility Ability)
{
    return FAbilityRules::Modifier(Scores.Get(Ability));
}

FAbilityScores FPointBuyRules::Baseline()
{
    FAbilityScores Scores;
    for (int32 Index = 0; Index < FAbilityRules::AbilityCount; ++Index) { Set(Scores, static_cast<EAbility>(Index), MinScore); }
    return Scores;
}

FAbilityScores FPointBuyRules::Recommended(EAdventurerRole Role)
{
    auto Make = [](int32 Str, int32 Dex, int32 Con, int32 Int, int32 Wis, int32 Cha)
    {
        FAbilityScores S;
        S.Strength = Str; S.Dexterity = Dex; S.Constitution = Con; S.Intelligence = Int; S.Wisdom = Wis; S.Charisma = Cha;
        return S;
    };
    // SRD 5.2.1 p. 20, Standard Array by Class.
    switch (Role)
    {
    case EAdventurerRole::Barbarian: return Make(15, 13, 14, 10, 12, 8);
    case EAdventurerRole::Bard: return Make(8, 14, 12, 13, 10, 15);
    case EAdventurerRole::Cleric: return Make(14, 8, 13, 10, 15, 12);
    case EAdventurerRole::Druid: return Make(8, 12, 14, 13, 15, 10);
    case EAdventurerRole::Fighter: return Make(15, 14, 13, 8, 10, 12);
    case EAdventurerRole::Monk: return Make(12, 15, 13, 10, 14, 8);
    case EAdventurerRole::Paladin: return Make(15, 10, 13, 8, 12, 14);
    case EAdventurerRole::Ranger: return Make(12, 15, 13, 8, 14, 10);
    case EAdventurerRole::Rogue: return Make(12, 15, 13, 14, 10, 8);
    case EAdventurerRole::Sorcerer: return Make(10, 13, 14, 8, 12, 15);
    case EAdventurerRole::Warlock: return Make(8, 14, 13, 12, 10, 15);
    case EAdventurerRole::Wizard: return Make(8, 12, 13, 15, 14, 10);
    default: return Baseline();
    }
}

bool FPointBuyRules::IsSrdClass(EAdventurerRole Role)
{
    return Role != EAdventurerRole::None && Role != EAdventurerRole::Artificer && static_cast<uint8>(Role) <= static_cast<uint8>(EAdventurerRole::Bard);
}

void FPointBuyRules::Set(FAbilityScores& Scores, EAbility Ability, int32 Value)
{
    switch (Ability)
    {
    case EAbility::Strength: Scores.Strength = Value; break;
    case EAbility::Dexterity: Scores.Dexterity = Value; break;
    case EAbility::Constitution: Scores.Constitution = Value; break;
    case EAbility::Intelligence: Scores.Intelligence = Value; break;
    case EAbility::Wisdom: Scores.Wisdom = Value; break;
    default: Scores.Charisma = Value; break;
    }
}
