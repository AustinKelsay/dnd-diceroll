#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"

/** Optional SRD Inspiration: a creature holds at most one and spends it for advantage. */
struct DNDDICEROLL_API FInspirationRules
{
    static bool EnabledByDefault();
    /** Grants inspiration; returns false (and changes nothing) when already held. */
    static bool Grant(bool& bHeld);
    /** Spending gives advantage on one d20 test; without inspiration it does nothing. */
    static bool Spend(bool& bHeld);
    static ERollMode ModeAfterSpending(ERollMode Mode, bool bSpent);
};
