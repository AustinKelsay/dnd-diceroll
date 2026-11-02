#pragma once

#include "CoreMinimal.h"

/** Which painted spell effect a playback event carries. Display only; the rules never read it. */
enum class EExpeditionSpell : uint8
{
    None, FireBolt, MagicMissile, CureWounds
};

/** The body motions playback can perform on a figure. */
enum class EExpeditionMotion : uint8
{
    Melee, Shoot, Cast, Heal, Step, Recoil, Dodge, Fall, Rise, Count
};

/**
 * Timing and distances for one motion. Seconds are at Speed 1; distances are in cells
 * along the facing (Reach, Pullback) or centimetres (Hop). Display only.
 */
struct DNDDICEROLL_API FMotionProfile
{
    float WindUp = 0.0f;   // Anticipation, overlapping the dice tumble for rolled actions.
    float Strike = 0.0f;   // The swing, release, cast, step, recoil, dodge, fall or rise itself.
    float Recover = 0.0f;  // Settle back to the idle pose.
    float Reach = 0.0f;    // Peak forward travel: lunge, knockback or sidestep.
    float Pullback = 0.0f; // Backward lean at the top of the wind-up.
    float Hop = 0.0f;      // Peak lift.
};

namespace ExpeditionMotion
{
/** The data table every playback step reads its timing from. */
DNDDICEROLL_API const FMotionProfile& Profile(EExpeditionMotion Motion);
DNDDICEROLL_API float TotalSeconds(const FMotionProfile& Profile);
/** Lean along the facing while winding up: 0 at the start, -Pullback at the top. */
DNDDICEROLL_API float WindUpLean(const FMotionProfile& Profile, float Alpha);
/** Lean along the facing during the strike: -Pullback at the start, Reach at the end. */
DNDDICEROLL_API float StrikeLean(const FMotionProfile& Profile, float Alpha);
/** Out-and-back curve for recoil, dodge and step hops: 0 at both ends, Peak at the middle. */
DNDDICEROLL_API float Arc(float Peak, float Alpha);
/** Which painted effect a committed action shows. Fire Bolt is the Wizard's ranged attack. */
DNDDICEROLL_API EExpeditionSpell SpellFor(uint8 CommandType, bool bWizard, bool bRanged);
DNDDICEROLL_API FString SpellName(EExpeditionSpell Spell);
}