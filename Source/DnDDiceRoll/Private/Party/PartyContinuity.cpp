#include "Party/PartyContinuity.h"

bool FPartyContinuity::Advance(FCombatBattle& Battle, const FCombatEncounter& Layout, int32 Seed, int32 RecipientId, FString& Error)
{
    Error.Reset();
    const FCombatantState* Recipient = Battle.Find(RecipientId);
    if (Battle.Snapshot().Phase != EBattlePhase::Victory || !Recipient || Recipient->Side != ECombatSide::Party || Recipient->Life != ECombatLife::Conscious)
    {
        Error = TEXT("Win the battle and select a conscious survivor for the reward."); return false;
    }
    FCombatEncounter Next = Layout;
    TSet<int32> CarriedIds;
    for (FCombatantState& Spawn : Next.Combatants)
    {
        if (Spawn.Side != ECombatSide::Party) { continue; }
        const FCombatantState* Previous = Battle.Find(Spawn.Id);
        if (!Previous || Previous->Side != ECombatSide::Party || CarriedIds.Contains(Spawn.Id))
        {
            Error = TEXT("Layout must retain exactly the existing party identities."); return false;
        }
        CarriedIds.Add(Spawn.Id);
        const FIntPoint Cell = Spawn.Cell;
        Spawn = *Previous;
        Spawn.Cell = Cell;
        if (Spawn.Id == RecipientId)
        {
            Spawn.WeaponRank = FMath::Min(3, Spawn.WeaponRank + 1);
            Spawn.Potions = FMath::Min(9, Spawn.Potions + 1);
        }
    }
    for (const FCombatantState& Previous : Battle.Snapshot().Encounter.Combatants)
    {
        if (Previous.Side == ECombatSide::Party && !CarriedIds.Contains(Previous.Id))
        {
            Error = TEXT("Layout omitted a persistent adventurer, including a dead member."); return false;
        }
    }
    FCombatBattle Candidate;
    if (!Candidate.Start(Next, Seed, Error)) { return false; }
    Battle = MoveTemp(Candidate);
    return true;
}
