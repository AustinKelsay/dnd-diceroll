#include "Rules/InspirationRules.h"

bool FInspirationRules::EnabledByDefault() { return false; }

bool FInspirationRules::Grant(bool& bHeld)
{
    if (bHeld) { return false; }
    bHeld = true;
    return true;
}

bool FInspirationRules::Spend(bool& bHeld)
{
    if (!bHeld) { return false; }
    bHeld = false;
    return true;
}

ERollMode FInspirationRules::ModeAfterSpending(ERollMode Mode, bool bSpent)
{
    if (!bSpent) { return Mode; }
    return FCheckRules::CombineMode(1, Mode == ERollMode::Disadvantage ? 1 : 0);
}
