#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"
#include "Rules/CoinRules.h"

/** SRD carrying capacity plus the optional variant encumbrance rule (off by default). Weights in pounds. */
struct DNDDICEROLL_API FEncumbranceRules
{
    static bool VariantEnabledByDefault();
    static int32 CarryingCapacity(int32 Strength);
    static int32 PushDragLift(int32 Strength);
    /** Base rule: over capacity drops speed to 1 cell; over push/drag/lift cannot move. Variant: over 5 x STR -2 cells, over 10 x STR -4 cells. */
    static int32 SpeedCells(int32 BaseSpeedCells, int32 WeightPounds, int32 Strength, bool bVariant);
    /** Variant only: heavily encumbered (over 10 x STR) rolls Str/Dex/Con checks, attacks and saves at disadvantage. */
    static ERollMode PhysicalRollMode(int32 WeightPounds, int32 Strength, bool bVariant);
    /** Item weight, rounded up to whole pounds. */
    static int32 Weight(EShopItem Item);
};
