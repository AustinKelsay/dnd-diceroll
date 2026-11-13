#include "Party/PartyContinuity.h"
#include "Party/BuildCatalog.h"

bool FPartyContinuity::Carry(FCombatBattle& Battle, const FCombatEncounter& Layout, int32 Seed, FString& Error)
{
    Error.Reset();
    if (Battle.Snapshot().Phase != EBattlePhase::Victory)
    {
        Error = TEXT("Win the encounter before continuing."); return false;
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

bool FPartyContinuity::Reward(FCombatBattle& Battle, int32 RecipientId, int32 Potions, FString& Error)
{
    return Reward(Battle, RecipientId, Potions, ERewardCard::Gear, Error);
}

bool FPartyContinuity::Reward(FCombatBattle& Battle, int32 RecipientId, int32 Potions, ERewardCard Card, FString& Error)
{
    Error.Reset();
    const FCombatantState* Recipient = Battle.Find(RecipientId);
    if (Battle.Snapshot().Phase != EBattlePhase::Victory || !Recipient || Recipient->Side != ECombatSide::Party ||
        Recipient->Life != ECombatLife::Conscious || Potions < 1 || Potions > 2)
    {
        Error = TEXT("Select a conscious survivor for the victory reward."); return false;
    }
    if (Card != ERewardCard::Gear && Card != ERewardCard::Reserve && Card != ERewardCard::Honed) { Error = TEXT("Choose Gear, Reserve or Honed."); return false; }
    if ((Card != ERewardCard::Gear) && !FBuildCatalog::Find(Recipient->BuildId)) { Error = TEXT("Gear only in this run."); return false; }
    if (Card == ERewardCard::Reserve && Recipient->bReserve) { Error = TEXT("Reserve taken."); return false; }
    if (Card == ERewardCard::Honed && Recipient->bHoned) { Error = TEXT("Already honed."); return false; }
    FCombatantState* Survivor = Battle.MutableFind(RecipientId);
    switch (Card)
    {
    case ERewardCard::Gear: Survivor->WeaponRank = FMath::Min(3, Survivor->WeaponRank + 1); break;
    case ERewardCard::Reserve:
    {
        // +1 cap and +1 use on each pool: a hero at Kit 0/2 goes to 1/3. Never a refill.
        const FResourceCaps Before = FBuildCatalog::Caps(*Survivor);
        Survivor->bReserve = true;
        const int32 Extra = UpgradeRules::ReserveUses;
        if (Before.Kit > 0) { Survivor->KitUses += Extra; }
        if (Before.Slot > 0) { Survivor->SpellSlots += Extra; }
        if (Before.Wind > 0) { Survivor->SecondWindUses += Extra; }
        if (Before.Maneuver > 0) { Survivor->ManeuverUses += Extra; }
        if (Before.Rage > 0) { Survivor->RageUses += Extra; }
        break;
    }
    case ERewardCard::Honed: Survivor->bHoned = true; break;
    }
    Survivor->Potions = FMath::Min(9, Survivor->Potions + Potions);
    return true;
}

namespace
{
constexpr int32 CampHealing = 10;
constexpr int32 CampResource = 1;
/** Heroes without a catalog build (combat fixtures) keep the original two-use pools. */
constexpr int32 ResourceCap = 2;
}

FString FPartyContinuity::RecoveryDescription()
{
    // Every hero's own cap (#218): true for every ruleset and revision, and after Reserve at reward 1.
    return FString::Printf(TEXT("Survivors: +%d HP, +%d Wind/slot/Rage/Kit up to each hero's own cap. No revival or potions."), CampHealing, CampResource);
}

bool FPartyContinuity::Recover(FCombatBattle& Battle, FString& Error)
{
    Error.Reset();
    if (Battle.Snapshot().Phase != EBattlePhase::Victory) { Error = TEXT("Recovery requires a completed victory."); return false; }
    for (FCombatantState& Survivor : Battle.State.Encounter.Combatants)
    {
        if (Survivor.Side != ECombatSide::Party || Survivor.Life != ECombatLife::Conscious) { continue; }
        Survivor.HP = FMath::Min(Survivor.MaxHP, Survivor.HP + CampHealing);
        if (const auto* B = FBuildCatalog::Find(Survivor.BuildId))
        {
            const FResourceCaps Cap = FBuildCatalog::Caps(Survivor);
            Survivor.KitUses = FMath::Min(Cap.Kit, Survivor.KitUses + B->KitRecovery);
            Survivor.RageUses = FMath::Min(Cap.Rage, Survivor.RageUses + B->RageRecovery);
            Survivor.SecondWindUses = FMath::Min(Cap.Wind, Survivor.SecondWindUses + B->WindRecovery);
            Survivor.SpellSlots = FMath::Min(Cap.Slot, Survivor.SpellSlots + B->SlotRecovery);
            Survivor.ManeuverUses = FMath::Min(Cap.Maneuver, Survivor.ManeuverUses + B->ManeuverRecovery);
        }
        else
        {
            if (Survivor.AdventurerRole == EAdventurerRole::Fighter) { Survivor.SecondWindUses = FMath::Min(ResourceCap, Survivor.SecondWindUses + CampResource); }
            if (Survivor.AdventurerRole == EAdventurerRole::Cleric || Survivor.AdventurerRole == EAdventurerRole::Wizard)
            { Survivor.SpellSlots = FMath::Min(ResourceCap, Survivor.SpellSlots + CampResource); }
        }
    }
    return true;
}

bool FPartyContinuity::Advance(FCombatBattle& Battle, const FCombatEncounter& Layout, int32 Seed, int32 RecipientId, FString& Error)
{
    FCombatBattle Candidate = Battle;
    if (!Reward(Candidate, RecipientId, 1, Error) || !Carry(Candidate, Layout, Seed, Error)) { return false; }
    Battle = MoveTemp(Candidate);
    return true;
}
