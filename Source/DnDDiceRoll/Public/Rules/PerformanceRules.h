#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"

/** SRD 5.1 NPC attitudes. */
enum class ENpcAttitude : uint8 { Hostile, Indifferent, Friendly };

struct DNDDICEROLL_API FPerformanceOutcome
{
    FD20Outcome Check;
    int32 Coins = 0;
    int32 AttitudeShift = 0;
};

/** SRD 5.1 Performance (CHA) for camps, towns and merchants. Coin and attitude amounts are Expedition tuning. */
struct DNDDICEROLL_API FPerformanceRules
{
    /** Fail by 5+: booed (attitude -1, no coin). Fail: nothing. Success: base coin and attitude +1. Beat by 10+: double coin. */
    static int32 CoinsFor(int32 Total, int32 Dc, int32 BaseCoins);
    static int32 AttitudeShiftFor(int32 Total, int32 Dc);
    static FPerformanceOutcome Resolve(int32 Modifier, ERollMode Mode, int32 Dc, int32 BaseCoins, int32& RandomState);
    /** Moves an attitude by steps, clamped to hostile..friendly. */
    static ENpcAttitude ShiftAttitude(ENpcAttitude Attitude, int32 Steps);
    static const TCHAR* AttitudeName(ENpcAttitude Attitude);
};
