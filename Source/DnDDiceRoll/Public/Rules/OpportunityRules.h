#pragma once

#include "CoreMinimal.h"

/** SRD opportunity attack triggers on the 5-foot grid (diagonals count as 1 cell). No reaction state is stored here. */
struct DNDDICEROLL_API FOpportunityRules
{
    static int32 GridDistance(const FIntPoint& A, const FIntPoint& B);
    static bool InReach(const FIntPoint& Reactor, int32 ReachCells, const FIntPoint& Cell);
    /** One step provokes when it leaves the reactor's reach voluntarily, without Disengage, while the reactor has its reaction. */
    static bool StepProvokes(const FIntPoint& Reactor, int32 ReachCells, const FIntPoint& From, const FIntPoint& To,
        bool bDisengaged, bool bForcedMovement, bool bReactionAvailable);
    /** Index of the path cell being left at the first provoking step, or INDEX_NONE. A reactor gets at most one per turn. */
    static int32 FirstProvokingCell(const TArray<FIntPoint>& Path, const FIntPoint& Reactor, int32 ReachCells, bool bDisengaged);
};
