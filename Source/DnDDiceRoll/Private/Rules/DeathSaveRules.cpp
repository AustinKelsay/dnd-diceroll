#include "Rules/DeathSaveRules.h"

namespace DeathSaveRulesPrivate
{
EDeathState StateOf(const FDeathSaveTally& Tally)
{
    if (Tally.Failures >= 3) { return EDeathState::Dead; }
    if (Tally.Successes >= 3) { return EDeathState::Stable; }
    return EDeathState::Dying;
}
}

EDeathState FDeathSaveRules::ApplySave(FDeathSaveTally& Tally, int32 NaturalRoll)
{
    if (NaturalRoll >= 20)
    {
        Tally = FDeathSaveTally();
        return EDeathState::Revived;
    }
    if (NaturalRoll <= 1) { Tally.Failures += 2; }
    else if (NaturalRoll >= 10) { ++Tally.Successes; }
    else { ++Tally.Failures; }
    return DeathSaveRulesPrivate::StateOf(Tally);
}

EDeathState FDeathSaveRules::ApplyDamageAtZero(FDeathSaveTally& Tally, int32 Damage, bool bCritical, int32 MaxHitPoints)
{
    if (Damage >= MaxHitPoints) { return EDeathState::Dead; }
    Tally.Failures += bCritical ? 2 : 1;
    return DeathSaveRulesPrivate::StateOf(Tally);
}

bool FDeathSaveRules::InstantDeath(int32 Damage, int32 HitPointsBefore, int32 MaxHitPoints)
{
    return Damage - FMath::Max(0, HitPointsBefore) >= MaxHitPoints;
}

int32 FDeathSaveRules::AbsorbWithTemporary(int32& TemporaryHitPoints, int32 Damage)
{
    const int32 Soaked = FMath::Min(FMath::Max(0, TemporaryHitPoints), FMath::Max(0, Damage));
    TemporaryHitPoints -= Soaked;
    return FMath::Max(0, Damage) - Soaked;
}

int32 FDeathSaveRules::GainTemporary(int32 Current, int32 Offered) { return FMath::Max(Current, Offered); }
bool FDeathSaveRules::MedicineStabilizes(int32 Total) { return Total >= 10; }
