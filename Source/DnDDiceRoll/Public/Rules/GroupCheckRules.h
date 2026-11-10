#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"

/** One member's part in a group check. */
struct DNDDICEROLL_API FGroupMember
{
    int32 Modifier = 0;
    ERollMode Mode = ERollMode::Normal;
};

struct DNDDICEROLL_API FGroupCheckOutcome
{
    TArray<FD20Outcome> Members;
    int32 Successes = 0;
    bool bSuccess = false;
};

/** SRD 5.2.1 Passive Checks, Working Together (Help) and Group Checks. */
struct DNDDICEROLL_API FGroupCheckRules
{
    /** 10 + modifier; +5 with advantage, -5 with disadvantage. */
    static int32 PassiveScore(int32 Modifier, ERollMode Mode);
    /** Help adds one advantage source; any disadvantage still cancels it. */
    static ERollMode HelpedMode(int32 AdvantageSources, int32 DisadvantageSources, bool bHelped);
    /** A group check succeeds when at least half the group succeeds. An empty group fails. */
    static bool GroupSucceeds(int32 Successes, int32 GroupSize);
    /** Exact group success chance in parts per million (rounded), from each member's 1/400 chance. Spends no RNG. */
    static int32 GroupSuccessPerMillion(const TArray<FGroupMember>& Group, int32 Dc);
    /** Rolls each member in order with FCheckRules::Resolve, advancing RandomState. */
    static FGroupCheckOutcome Resolve(const TArray<FGroupMember>& Group, int32 Dc, int32& RandomState);
};
