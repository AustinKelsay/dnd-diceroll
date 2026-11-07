#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"

/** A reported reason for attack advantage/disadvantage; callers supply condition-specific facts. */
struct DNDDICEROLL_API FAttackRollSource
{
    FString Name;
    ERollMode Mode = ERollMode::Normal;
};

/** Pure attack result, including the reasons used to select the roll mode. */
struct DNDDICEROLL_API FAttackOutcome
{
    FD20Outcome Roll;
    TArray<FAttackRollSource> Sources;
    bool bCritical = false;
};

/** SRD 5.2.1 attack rolls only; battle/HUD integration is deferred. */
struct DNDDICEROLL_API FAttackRules
{
    /** Adjacent threat means a hostile within 5 feet that sees the attacker and is not incapacitated. */
    static TArray<FAttackRollSource> WithRangedAdjacency(const TArray<FAttackRollSource>& Sources, bool bRangedAttack, bool bAdjacentThreat);
    static ERollMode CombineMode(const TArray<FAttackRollSource>& Sources);
    /** Exact probability, accounting for natural 1 always missing and natural 20 always hitting. No RNG. */
    static int32 SuccessOutOf400(int32 Modifier, int32 ArmorClass, const TArray<FAttackRollSource>& Sources);
    static int32 SuccessPercent(int32 Modifier, int32 ArmorClass, const TArray<FAttackRollSource>& Sources);
    static FAttackOutcome Resolve(int32 Modifier, int32 ArmorClass, const TArray<FAttackRollSource>& Sources, int32& RandomState);
};
