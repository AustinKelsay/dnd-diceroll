#pragma once

#include "CoreMinimal.h"

/** SRD advantage state. Any advantage plus any disadvantage cancels to Normal, however many sources. */
enum class ERollMode : uint8 { Normal, Advantage, Disadvantage };

/** One resolved d20 test. Rolls[1] is used only when Mode is not Normal. */
struct DNDDICEROLL_API FD20Outcome
{
    ERollMode Mode = ERollMode::Normal;
    int32 Rolls[2] = {0, 0};
    int32 RollCount = 0;
    int32 Kept = 0;
    int32 Modifier = 0;
    int32 Total = 0;
    int32 Dc = 0;
    bool bSuccess = false;
};

struct DNDDICEROLL_API FCheckRules
{
    static constexpr int32 MinDc = 1;
    static constexpr int32 MaxDc = 30;
    static ERollMode CombineMode(int32 AdvantageSources, int32 DisadvantageSources);
    static const TCHAR* ModeName(ERollMode Mode);
    /** Exact success chance in 1/400ths (every pair of d20 faces). Ability checks and saves have no natural 1/20 rule. */
    static int32 SuccessOutOf400(int32 Modifier, int32 Dc, ERollMode Mode);
    /** Rounded whole percent for display. */
    static int32 SuccessPercent(int32 Modifier, int32 Dc, ERollMode Mode);
    /** Rolls with the same FRandomStream stepping as combat dice and advances RandomState. */
    static FD20Outcome Resolve(int32 Modifier, int32 Dc, ERollMode Mode, int32& RandomState);
    /** Contested checks: the actor must beat the defender; ties keep the status quo. */
    static bool ActorWinsContest(int32 ActorTotal, int32 DefenderTotal);
};
