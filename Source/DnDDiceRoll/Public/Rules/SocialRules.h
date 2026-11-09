#pragma once

#include "CoreMinimal.h"
#include "Rules/PerformanceRules.h"

enum class ESocialOutcome : uint8 { None, AvoidFight, Information, Reward };

/** NPC influence: Persuasion and Intimidation against attitude DCs, Deception against Insight. */
struct DNDDICEROLL_API FSocialRules
{
    /** Friendly 10, Indifferent 15, Hostile 20. */
    static int32 InfluenceDc(ENpcAttitude Attitude);
    /** Success moves the attitude one step toward Friendly; failure leaves it. */
    static ENpcAttitude AfterInfluence(ENpcAttitude Attitude, int32 Total);
    /** A lie works only if the Deception total beats the target's Insight total; ties go to the target. */
    static bool LieSucceeds(int32 DeceptionTotal, int32 InsightTotal);
    /** What a convinced NPC offers: hostile foes stand down, indifferent ones talk, friendly ones reward. */
    static ESocialOutcome OutcomeFor(ENpcAttitude Before, ENpcAttitude After);
};
