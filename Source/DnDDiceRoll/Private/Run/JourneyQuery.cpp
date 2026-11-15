#include "Run/JourneyQuery.h"

namespace
{
/** The camp opens after the first victory (FExpeditionRun::ClaimReward moves to Recovery when one fight is won). */
constexpr int32 CampAfter = 0;

FJourneyRoute DescribeRoute(const FExpeditionProgress& Progress, const FExpeditionRoute& Route)
{
    const FEncounterIdentity Identity = FExpeditionRun::Encounter(Progress, Route);
    FJourneyRoute Out;
    Out.Id = Route.Id;
    Out.Name = Route.Name;
    Out.World = Identity.World;
    Out.bDangerous = Route.bDangerous;
    Out.Risk = Identity.Risk;
    return Out;
}
}

FJourney FJourneyQuery::For(const FExpeditionProgress& Progress)
{
    FJourney Journey;
    Journey.Phase = Progress.Phase;
    const bool bEnded = Progress.Phase == EExpeditionPhase::Victory || Progress.Phase == EExpeditionPhase::Defeat;
    bool bCurrentFound = false;
    for (int32 Index = 0; Index < FExpeditionRun::EncounterCount; ++Index)
    {
        FJourneyStage Stage;
        Stage.Index = Index;
        Stage.Kind = FExpeditionRun::StageKind(Index);
        if (Progress.History.IsValidIndex(Index))
        {
            const FExpeditionEncounterRecord& Record = Progress.History[Index];
            Stage.bTaken = true;
            Stage.Taken = DescribeRoute(Progress, Record.Route);
            Stage.World = Stage.Taken.World;
            Stage.Outcome = Record.Outcome;
            Stage.Survivors = Record.Survivors;
            switch (Record.Outcome)
            {
            case EBattlePhase::Victory: Stage.State = EJourneyState::Cleared; break;
            case EBattlePhase::Defeat: Stage.State = EJourneyState::Lost; break;
            default: Stage.State = EJourneyState::Current; bCurrentFound = true; Journey.CurrentIndex = Index; break;
            }
        }
        else if (!bEnded && !bCurrentFound && Index == Progress.History.Num())
        {
            // The next stage: its offers exist only in the Route phase (they are rolled on arrival, after the reward and the camp).
            Stage.State = EJourneyState::Current;
            bCurrentFound = true;
            Journey.CurrentIndex = Index;
            for (const FExpeditionRoute& Offer : Progress.Offers) { Stage.Offers.Add(DescribeRoute(Progress, Offer)); }
        }
        Journey.Stages.Add(Stage);
    }
    Journey.bCampUsed = Progress.bCampUsed;
    Journey.CampAfterStage = Progress.bCampUsed ? CampAfter : INDEX_NONE;
    return Journey;
}
