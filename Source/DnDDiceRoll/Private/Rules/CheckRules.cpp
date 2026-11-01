#include "Rules/CheckRules.h"

namespace
{
int32 FacesSucceeding(int32 Modifier, int32 Dc) { return FMath::Clamp(21 - (Dc - Modifier), 0, 20); }

int32 RollD20(int32& RandomState)
{
    FRandomStream Random(RandomState);
    const int32 Value = Random.RandRange(1, 20);
    RandomState = Random.GetCurrentSeed();
    return Value;
}
}

ERollMode FCheckRules::CombineMode(int32 AdvantageSources, int32 DisadvantageSources)
{
    const bool bAdvantage = AdvantageSources > 0;
    const bool bDisadvantage = DisadvantageSources > 0;
    return bAdvantage == bDisadvantage ? ERollMode::Normal : bAdvantage ? ERollMode::Advantage : ERollMode::Disadvantage;
}

const TCHAR* FCheckRules::ModeName(ERollMode Mode)
{
    return Mode == ERollMode::Advantage ? TEXT("advantage") : Mode == ERollMode::Disadvantage ? TEXT("disadvantage") : TEXT("normal");
}

int32 FCheckRules::SuccessOutOf400(int32 Modifier, int32 Dc, ERollMode Mode)
{
    const int32 Faces = FacesSucceeding(Modifier, Dc);
    if (Mode == ERollMode::Advantage) { return 400 - (20 - Faces) * (20 - Faces); }
    if (Mode == ERollMode::Disadvantage) { return Faces * Faces; }
    return Faces * 20;
}

int32 FCheckRules::SuccessPercent(int32 Modifier, int32 Dc, ERollMode Mode)
{
    return (SuccessOutOf400(Modifier, Dc, Mode) + 2) / 4;
}

FD20Outcome FCheckRules::Resolve(int32 Modifier, int32 Dc, ERollMode Mode, int32& RandomState)
{
    FD20Outcome Outcome;
    Outcome.Mode = Mode;
    Outcome.Modifier = Modifier;
    Outcome.Dc = Dc;
    Outcome.Rolls[0] = RollD20(RandomState);
    Outcome.RollCount = 1;
    Outcome.Kept = Outcome.Rolls[0];
    if (Mode != ERollMode::Normal)
    {
        Outcome.Rolls[1] = RollD20(RandomState);
        Outcome.RollCount = 2;
        Outcome.Kept = Mode == ERollMode::Advantage ? FMath::Max(Outcome.Rolls[0], Outcome.Rolls[1]) : FMath::Min(Outcome.Rolls[0], Outcome.Rolls[1]);
    }
    Outcome.Total = Outcome.Kept + Modifier;
    Outcome.bSuccess = Outcome.Total >= Dc;
    return Outcome;
}

bool FCheckRules::ActorWinsContest(int32 ActorTotal, int32 DefenderTotal)
{
    return ActorTotal > DefenderTotal;
}
