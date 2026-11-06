#pragma once

#include "CoreMinimal.h"

struct DNDDICEROLL_API FDeathSaveTally
{
    int32 Successes = 0;
    int32 Failures = 0;
};

enum class EDeathState : uint8 { Dying, Stable, Dead, Revived };

/** SRD death saves, instant death and temporary hit points. */
struct DNDDICEROLL_API FDeathSaveRules
{
    /** 10+ succeeds, a 1 counts as two failures, a 20 regains 1 hit point (tally resets). Three of either ends it. */
    static EDeathState ApplySave(FDeathSaveTally& Tally, int32 NaturalRoll);
    /** Damage while at 0 HP: one failure, two on a critical; damage of at least max HP kills outright. */
    static EDeathState ApplyDamageAtZero(FDeathSaveTally& Tally, int32 Damage, bool bCritical, int32 MaxHitPoints);
    /** Massive damage: the damage left after reaching 0 HP is at least the hit point maximum. */
    static bool InstantDeath(int32 Damage, int32 HitPointsBefore, int32 MaxHitPoints);
    /** Temporary hit points soak damage first; returns the damage that reaches real hit points. */
    static int32 AbsorbWithTemporary(int32& TemporaryHitPoints, int32 Damage);
    /** Temporary hit points do not stack: keep the larger. */
    static int32 GainTemporary(int32 Current, int32 Offered);
    /** Medicine DC 10 stabilizes a dying creature. */
    static bool MedicineStabilizes(int32 Total);
};
