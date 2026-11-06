#pragma once

#include "CoreMinimal.h"

enum class ECreatureSize : uint8 { Tiny, Small, Medium, Large, Huge, Gargantuan };
enum class EShoveChoice : uint8 { KnockProne, PushAway };

/** SRD grapple and shove as Athletics contests. */
struct DNDDICEROLL_API FGrappleRules
{
    /** A creature can grapple or shove a target no more than one size larger. */
    static bool SizeAllows(ECreatureSize Attacker, ECreatureSize Target);
    /** Exact chance in 1/400ths that d20 + ActorModifier beats d20 + DefenderModifier (ties hold). */
    static int32 ContestOutOf400(int32 ActorModifier, int32 DefenderModifier);
    /** A grappled creature's speed is 0. */
    static int32 GrappledSpeed(int32 SpeedCells);
    /** Dragging a grappled creature costs double movement. */
    static int32 DragCost(int32 Cells);
    /** Escaping is the grappled creature's contest against the grappler. */
    static int32 EscapeOutOf400(int32 EscaperModifier, int32 GrapplerModifier);
    static int32 ShovePushCells(bool bWon, EShoveChoice Choice);
    static bool ShoveKnocksProne(bool bWon, EShoveChoice Choice);
};
