#include "Rules/PerformanceRules.h"

int32 FPerformanceRules::CoinsFor(int32 Total, int32 Dc, int32 BaseCoins)
{
    if (Total < Dc) { return 0; }
    return Total - Dc >= 10 ? BaseCoins * 2 : BaseCoins;
}

int32 FPerformanceRules::AttitudeShiftFor(int32 Total, int32 Dc)
{
    if (Total >= Dc) { return 1; }
    return Dc - Total >= 5 ? -1 : 0;
}

FPerformanceOutcome FPerformanceRules::Resolve(int32 Modifier, ERollMode Mode, int32 Dc, int32 BaseCoins, int32& RandomState)
{
    FPerformanceOutcome Outcome;
    Outcome.Check = FCheckRules::Resolve(Modifier, Dc, Mode, RandomState);
    Outcome.Coins = CoinsFor(Outcome.Check.Total, Dc, BaseCoins);
    Outcome.AttitudeShift = AttitudeShiftFor(Outcome.Check.Total, Dc);
    return Outcome;
}

ENpcAttitude FPerformanceRules::ShiftAttitude(ENpcAttitude Attitude, int32 Steps)
{
    return static_cast<ENpcAttitude>(FMath::Clamp(static_cast<int32>(Attitude) + Steps, 0, 2));
}

const TCHAR* FPerformanceRules::AttitudeName(ENpcAttitude Attitude)
{
    return Attitude == ENpcAttitude::Friendly ? TEXT("friendly") : Attitude == ENpcAttitude::Hostile ? TEXT("hostile") : TEXT("indifferent");
}
