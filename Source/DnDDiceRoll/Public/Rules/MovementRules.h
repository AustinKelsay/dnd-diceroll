
#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"

/** Plain dice: sum of Count rolls of a die with Sides faces, stepping the same FRandomStream as combat. */
struct DNDDICEROLL_API FDiceRules
{
    static int32 Roll(int32 Count, int32 Sides, int32& RandomState);
};

/** SRD falling, prone, crawling, forced movement and difficult terrain on the grid (1 cell = 5 ft). */
struct DNDDICEROLL_API FMovementRules
{
    /**
     * Falling damage dice. House rule (locked for this project, differs from raw SRD 5.2.1):
     * the first 15 ft are free, then 1d6 per full 10 ft beyond that (round down), max 20d6.
     * Raw SRD 5.2.1 is 1d6 per 10 ft from the first foot, still capped at 20d6.
     */
    static int32 FallingDice(int32 Feet);
    static int32 FallDamage(int32 Feet, int32& RandomState);
    /** Taking any falling damage leaves the creature prone. */
    static bool LandsProne(int32 Feet);
    /** Standing up costs half the speed, rounded up. */
    static int32 StandUpCost(int32 SpeedCells);
    /** Crawling costs one extra cell per cell. */
    static int32 CrawlCost(int32 Cells);
    /** Difficult terrain costs one extra cell per cell (SRD). */
    static int32 DifficultTerrainCost(int32 Cells);
    /** A push stops in the last free cell before a blocker. */
    static int32 PushDistance(int32 RequestedCells, int32 FreeCellsBeforeBlocker);
    /** Attacks against a prone target: advantage within 5 ft, disadvantage farther away. */
    static ERollMode AttackAgainstProne(int32 DistanceCells);
};
