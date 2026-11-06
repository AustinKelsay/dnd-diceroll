#include "Rules/StealthRules.h"

int32 FStealthRules::PassiveScore(int32 Modifier, ERollMode Mode)
{
    return 10 + Modifier + (Mode == ERollMode::Advantage ? 5 : Mode == ERollMode::Disadvantage ? -5 : 0);
}

bool FStealthRules::HiddenFrom(int32 StealthTotal, int32 ObserverPassivePerception)
{
    return StealthTotal >= ObserverPassivePerception;
}

ERollMode FStealthRules::AttackModeWhileHidden(bool bAttackerHidden, bool bTargetHidden)
{
    return FCheckRules::CombineMode(bAttackerHidden ? 1 : 0, bTargetHidden ? 1 : 0);
}

bool FStealthRules::StaysHiddenAfterAttack() { return false; }

bool FStealthRules::IsSurprised(const TArray<int32>& AmbusherStealthTotals, int32 PassivePerception)
{
    if (AmbusherStealthTotals.Num() == 0) { return false; }
    for (const int32 Total : AmbusherStealthTotals)
    {
        if (!HiddenFrom(Total, PassivePerception)) { return false; }
    }
    return true;
}

bool FStealthRules::CanActOnTurn(bool bSurprised, int32 RoundNumber)
{
    return !(bSurprised && RoundNumber <= 1);
}
