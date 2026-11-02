#include "Rules/OpportunityRules.h"

int32 FOpportunityRules::GridDistance(const FIntPoint& A, const FIntPoint& B)
{
    return FMath::Max(FMath::Abs(A.X - B.X), FMath::Abs(A.Y - B.Y));
}

bool FOpportunityRules::InReach(const FIntPoint& Reactor, int32 ReachCells, const FIntPoint& Cell)
{
    return GridDistance(Reactor, Cell) <= ReachCells;
}

bool FOpportunityRules::StepProvokes(const FIntPoint& Reactor, int32 ReachCells, const FIntPoint& From, const FIntPoint& To,
    bool bDisengaged, bool bForcedMovement, bool bReactionAvailable)
{
    return bReactionAvailable && !bDisengaged && !bForcedMovement && InReach(Reactor, ReachCells, From) && !InReach(Reactor, ReachCells, To);
}

int32 FOpportunityRules::FirstProvokingCell(const TArray<FIntPoint>& Path, const FIntPoint& Reactor, int32 ReachCells, bool bDisengaged)
{
    for (int32 Step = 1; Step < Path.Num(); ++Step)
    {
        if (StepProvokes(Reactor, ReachCells, Path[Step - 1], Path[Step], bDisengaged, false, true)) { return Step - 1; }
    }
    return INDEX_NONE;
}
