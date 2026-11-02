#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"

/** SRD hiding, hidden attackers and surprise. */
struct DNDDICEROLL_API FStealthRules
{
    /** Passive score: 10 + modifier, +5 with advantage, -5 with disadvantage. */
    static int32 PassiveScore(int32 Modifier, ERollMode Mode);
    /** Hidden from an observer when the Stealth total meets or beats its passive Perception (house reading of the tie). */
    static bool HiddenFrom(int32 StealthTotal, int32 ObserverPassivePerception);
    /** Attacking a target that cannot see you grants advantage; being unseen by the attacker imposes disadvantage. */
    static ERollMode AttackModeWhileHidden(bool bAttackerHidden, bool bTargetHidden);
    /** Attacking reveals the attacker, hit or miss. */
    static bool StaysHiddenAfterAttack();
    /** A creature is surprised only if it notices no ambusher. */
    static bool IsSurprised(const TArray<int32>& AmbusherStealthTotals, int32 PassivePerception);
    /** Surprised creatures cannot move or act on the first turn of combat. */
    static bool CanActOnTurn(bool bSurprised, int32 RoundNumber);
};
