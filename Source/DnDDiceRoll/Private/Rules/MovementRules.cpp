
#include "Rules/MovementRules.h"

int32 FDiceRules::Roll(int32 Count, int32 Sides, int32& RandomState)
{
    int32 Sum = 0;
    for (int32 Index = 0; Index < Count && Sides > 0; ++Index)
    {
        FRandomStream Random(RandomState);
        Sum += Random.RandRange(1, Sides);
        RandomState = Random.GetCurrentSeed();
    }
    return Sum;
}

int32 FMovementRules::FallingDice(int32 Feet)
{
    // House rule: first 15 ft free, then 1d6 per full 10 ft (max 20d6). Raw SRD starts at the first foot.
    const int32 ChargedFeet = FMath::Max(0, Feet - 15);
    return FMath::Clamp(ChargedFeet / 10, 0, 20);
}

int32 FMovementRules::FallDamage(int32 Feet, int32& RandomState)
{
    return FDiceRules::Roll(FallingDice(Feet), 6, RandomState);
}

bool FMovementRules::LandsProne(int32 Feet)
{
    return FallingDice(Feet) > 0;
}

int32 FMovementRules::StandUpCost(int32 SpeedCells)
{
    return (FMath::Max(0, SpeedCells) + 1) / 2;
}

int32 FMovementRules::CrawlCost(int32 Cells)
{
    return FMath::Max(0, Cells) * 2;
}

int32 FMovementRules::DifficultTerrainCost(int32 Cells)
{
    return FMath::Max(0, Cells) * 2;
}

int32 FMovementRules::PushDistance(int32 RequestedCells, int32 FreeCellsBeforeBlocker)
{
    return FMath::Clamp(RequestedCells, 0, FMath::Max(0, FreeCellsBeforeBlocker));
}

ERollMode FMovementRules::AttackAgainstProne(int32 DistanceCells)
{
    return DistanceCells <= 1 ? ERollMode::Advantage : ERollMode::Disadvantage;
}
