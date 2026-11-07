#include "Rules/AttackRules.h"

TArray<FAttackRollSource> FAttackRules::WithRangedAdjacency(const TArray<FAttackRollSource>& Sources, bool bRangedAttack, bool bAdjacentThreat)
{
    TArray<FAttackRollSource> Result = Sources;
    if (bRangedAttack && bAdjacentThreat) { Result.Add({TEXT("Adjacent hostile"), ERollMode::Disadvantage}); }
    return Result;
}

ERollMode FAttackRules::CombineMode(const TArray<FAttackRollSource>& Sources)
{
    int32 Advantages = 0, Disadvantages = 0;
    for (const FAttackRollSource& Source : Sources)
    {
        Advantages += Source.Mode == ERollMode::Advantage ? 1 : 0;
        Disadvantages += Source.Mode == ERollMode::Disadvantage ? 1 : 0;
    }
    return FCheckRules::CombineMode(Advantages, Disadvantages);
}

int32 FAttackRules::SuccessOutOf400(int32 Modifier, int32 ArmorClass, const TArray<FAttackRollSource>& Sources)
{
    const int32 Faces = FMath::Clamp(21 - (ArmorClass - Modifier), 1, 19);
    switch (CombineMode(Sources))
    {
    case ERollMode::Advantage: return 400 - (20 - Faces) * (20 - Faces);
    case ERollMode::Disadvantage: return Faces * Faces;
    default: return Faces * 20;
    }
}

int32 FAttackRules::SuccessPercent(int32 Modifier, int32 ArmorClass, const TArray<FAttackRollSource>& Sources)
{
    return (SuccessOutOf400(Modifier, ArmorClass, Sources) + 2) / 4;
}

FAttackOutcome FAttackRules::Resolve(int32 Modifier, int32 ArmorClass, const TArray<FAttackRollSource>& Sources, int32& RandomState)
{
    FAttackOutcome Outcome;
    Outcome.Sources = Sources;
    Outcome.Roll = FCheckRules::Resolve(Modifier, ArmorClass, CombineMode(Sources), RandomState);
    Outcome.bCritical = Outcome.Roll.Kept == 20;
    Outcome.Roll.bSuccess = Outcome.bCritical || (Outcome.Roll.Kept != 1 && Outcome.Roll.Total >= ArmorClass);
    return Outcome;
}
