#include "Rules/SleightRules.h"
#include "Combat/CombatBattle.h"

int32 FSleightRules::SuccessOutOf400(int32 SleightModifier, ERollMode Mode, int32 TargetPassivePerception)
{
    return FCheckRules::SuccessOutOf400(SleightModifier, TargetPassivePerception, Mode);
}

ESleightResult FSleightRules::ResultFor(int32 Total, int32 TargetPassivePerception)
{
    if (Total >= TargetPassivePerception) { return ESleightResult::Unnoticed; }
    return TargetPassivePerception - Total >= 5 ? ESleightResult::Caught : ESleightResult::Fumbled;
}

FSleightOutcome FSleightRules::Resolve(int32 SleightModifier, ERollMode Mode, int32 TargetPassivePerception, int32& RandomState)
{
    FSleightOutcome Outcome;
    Outcome.Check = FCheckRules::Resolve(SleightModifier, TargetPassivePerception, Mode, RandomState);
    Outcome.Result = ResultFor(Outcome.Check.Total, TargetPassivePerception);
    return Outcome;
}

bool FSleightRules::UseObjectAsBonusAction(EAdventurerRole Role, int32 Level)
{
    return Role == EAdventurerRole::Rogue && Level >= 2;
}
