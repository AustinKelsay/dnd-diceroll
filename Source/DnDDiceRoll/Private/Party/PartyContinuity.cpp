#include "Party/PartyContinuity.h"
#include "Party/FeatureKit.h"
#include "Rules/RestRules.h"
#include "Party/BuildCatalog.h"
#include "Rules/SpellRules.h"

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
    Error.Reset();
    const FCombatantState* Recipient = Battle.Find(RecipientId);
    if (Battle.Snapshot().Phase != EBattlePhase::Victory || !Recipient || Recipient->Side != ECombatSide::Party ||
        Recipient->Life != ECombatLife::Conscious || Potions < 1 || Potions > 2)
    {
        Error = TEXT("Select a conscious survivor for the victory reward."); return false;
    }
    FCombatantState* Survivor = Battle.MutableFind(RecipientId);
    Survivor->WeaponRank = FMath::Min(3, Survivor->WeaponRank + 1);
    Survivor->Potions = FMath::Min(9, Survivor->Potions + Potions);
    return true;
}

namespace
{
constexpr int32 CampHealing = 10;
constexpr int32 CampResource = 1;
constexpr int32 ResourceCap = 2;
}

FString FPartyContinuity::RecoveryDescription()
{
    return FString::Printf(TEXT("Survivors: +%d HP, +%d Wind/slot/Rage/Kit (caps: max HP/%d). No revival or potions."), CampHealing, CampResource, ResourceCap);
}

bool FPartyContinuity::Recover(FCombatBattle& Battle, FString& Error)
{
    Error.Reset();
    if (Battle.Snapshot().Phase != EBattlePhase::Victory) { Error = TEXT("Recovery requires a completed victory."); return false; }
    for (FCombatantState& Survivor : Battle.State.Encounter.Combatants)
    {
        if (Survivor.Side != ECombatSide::Party || Survivor.Life != ECombatLife::Conscious) { continue; }
        Survivor.HP = FMath::Min(Survivor.MaxHP, Survivor.HP + CampHealing);
        // Camp is a long rest: regain half the hit dice (minimum one).
        Survivor.HitDiceSpent = Survivor.Level - FRestRules::HitDiceAfterLongRest(Survivor.Level - Survivor.HitDiceSpent, Survivor.Level);
        if (const auto* B = FBuildCatalog::Find(Survivor.BuildId))
        {
            Survivor.KitUses = FMath::Min(B->KitCap, Survivor.KitUses + B->KitRecovery);
            Survivor.RageUses = FMath::Min(B->RageCap, Survivor.RageUses + B->RageRecovery);
            Survivor.SecondWindUses = FMath::Min(FBuildCatalog::WindCap(Survivor), Survivor.SecondWindUses + FMath::Max(B->WindRecovery, B->WindCap ? 0 : 1));
            Survivor.SpellSlots = FMath::Min(FBuildCatalog::SlotCap(Survivor), Survivor.SpellSlots + FMath::Max(B->SlotRecovery, B->SlotCap ? 0 : 1));
            Survivor.ManeuverUses = FMath::Min(FBuildCatalog::ManeuverCap(Survivor), Survivor.ManeuverUses + B->ManeuverRecovery + (FBuildCatalog::ManeuverCap(Survivor) > B->ManeuverCap ? 2 : 0));
        }
        else
        {
            if (Survivor.AdventurerRole == EAdventurerRole::Fighter) { Survivor.SecondWindUses = FMath::Min(ResourceCap, Survivor.SecondWindUses + CampResource); }
            if (Survivor.AdventurerRole == EAdventurerRole::Cleric || Survivor.AdventurerRole == EAdventurerRole::Wizard)
            { Survivor.SpellSlots = FMath::Min(ResourceCap, Survivor.SpellSlots + CampResource); }
        }
        if (!Survivor.SlotsByLevel.IsEmpty()) { Survivor.SlotsByLevel[0] = FMath::Min(Survivor.SpellSlots, FBuildCatalog::SlotMax(Survivor, 1)); Survivor.SpellSlots = Survivor.SlotsByLevel[0]; }
        if (FBuildCatalog::Find(Survivor.BuildId)) { FFeatureKit::Refill(Survivor); }
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
