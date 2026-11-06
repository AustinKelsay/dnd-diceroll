#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"

/** Stealth, hiding and surprise as pure queries (SRD 5.2.1). */
struct DNDDICEROLL_API FStealthRules
{
    /** 10 + modifier; +5 with advantage, -5 with disadvantage. */
    static int32 PassiveScore(int32 Modifier, ERollMode Mode);
    /** A Stealth total hides from an observer when it meets or beats their passive Perception. */
    static bool HiddenFrom(int32 StealthTotal, int32 ObserverPassivePerception);
    /** Hidden attacker: advantage. Unseen target: disadvantage. Both cancel. */
    static ERollMode AttackModeWhileHidden(bool bAttackerHidden, bool bTargetHidden);
    /** Attacking reveals the attacker (ends the Hidden condition for that creature). */
    static bool StaysHiddenAfterAttack();
    /**
     * SRD 5.2.1 Surprise (revised vs 5.1): if every ambusher is hidden from the creature,
     * that creature is surprised and rolls Initiative with Disadvantage. It still takes its
     * turn normally; there is no skipped first turn.
     */
    static bool IsSurprised(const TArray<int32>& AmbusherStealthTotals, int32 PassivePerception);
    /** Surprised combatants have Disadvantage on Initiative (SRD 5.2.1 Combat). */
    static ERollMode InitiativeMode(bool bSurprised);
};
