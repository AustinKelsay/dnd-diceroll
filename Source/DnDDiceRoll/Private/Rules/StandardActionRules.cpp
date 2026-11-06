#include "Rules/StandardActionRules.h"
#include "Combat/CombatBattle.h"

bool FStandardActionRules::CanTakeAsBonusAction(EStandardAction Action, EAdventurerRole Role, int32 Level)
{
    const bool bCunning = Action == EStandardAction::Dash || Action == EStandardAction::Disengage || Action == EStandardAction::Hide;
    return bCunning && Role == EAdventurerRole::Rogue && Level >= 2;
}

EActionCost FStandardActionRules::Cost(EStandardAction Action, EAdventurerRole Role, int32 Level, bool bActionAlreadyUsed)
{
    return bActionAlreadyUsed && CanTakeAsBonusAction(Action, Role, Level) ? EActionCost::BonusAction : EActionCost::Action;
}

int32 FStandardActionRules::MovementBudget(int32 SpeedCells, int32 Dashes)
{
    return FMath::Max(0, SpeedCells) * (1 + FMath::Max(0, Dashes));
}

ERollMode FStandardActionRules::AttackAgainstDodger(bool bDodging, bool bSeesAttacker)
{
    return bDodging && bSeesAttacker ? ERollMode::Disadvantage : ERollMode::Normal;
}

ERollMode FStandardActionRules::DexSaveWhileDodging(bool bDodging)
{
    return bDodging ? ERollMode::Advantage : ERollMode::Normal;
}

bool FStandardActionRules::LeavingReachProvokes(bool bDisengaged) { return !bDisengaged; }
