#include "Combat/ActionFacts.h"
#include "Party/BuildCatalog.h"

namespace
{
using namespace BuildRules;

int32 HeldOf(const FCombatantState& Unit, EActionResource Resource)
{
    switch (Resource)
    {
    case EActionResource::Kit: return Unit.KitUses;
    case EActionResource::Slot: return Unit.SpellSlots;
    case EActionResource::Wind: return Unit.SecondWindUses;
    case EActionResource::Maneuver: return Unit.ManeuverUses;
    case EActionResource::Rage: return Unit.RageUses;
    case EActionResource::Potion: return Unit.Potions;
    default: return 0;
    }
}

FActionResourceCost ResourceCost(const FCombatantState& Unit, const FBuildDefinition* Build, EActionResource Resource, int32 Amount)
{
    FActionResourceCost Cost;
    Cost.Resource = Resource;
    Cost.Amount = Amount;
    Cost.Held = HeldOf(Unit, Resource);
    if (Resource == EActionResource::Potion) { Cost.CampRecovery = 0; } // Camp never refills potions.
    else if (Build)
    {
        switch (Resource)
        {
        case EActionResource::Kit: Cost.Cap = Build->KitCap; Cost.CampRecovery = Build->KitRecovery; break;
        case EActionResource::Slot: Cost.Cap = Build->SlotCap; Cost.CampRecovery = Build->SlotRecovery; break;
        case EActionResource::Wind: Cost.Cap = Build->WindCap; Cost.CampRecovery = Build->WindRecovery; break;
        case EActionResource::Maneuver: Cost.Cap = Build->ManeuverCap; Cost.CampRecovery = Build->ManeuverRecovery; break;
        case EActionResource::Rage: Cost.Cap = Build->RageCap; Cost.CampRecovery = Build->RageRecovery; break;
        default: break;
        }
    }
    return Cost;
}

/** The Barbarian kits gate their abilities on the Rage stance rather than on a Kit pool. */
bool IsRageStyle(const FBuildDefinition* Build) { return Build && !Build->Abilities.IsEmpty() && Build->KitCap == 0; }

EActionTargetKind KindOf(EAbilityTarget Target)
{
    switch (Target)
    {
    case EAbilityTarget::Self: return EActionTargetKind::Self;
    case EAbilityTarget::Enemy: return EActionTargetKind::Enemy;
    case EAbilityTarget::Ally: return EActionTargetKind::Ally;
    default: return EActionTargetKind::Friendly;
    }
}

/**
 * What the command is: its cost, resources, stance and target shape. Mirrors how FCombatBattle::Resolve
 * dispatches and what it spends, not how descriptions word it. Returns false when the actor's kit lacks it.
 */
bool Shape(const FCombatantState& Unit, ECombatCommandType Action, FActionFacts& Facts)
{
    const FBuildDefinition* Build = FBuildCatalog::Find(Unit.BuildId);
    auto Spend = [&](EActionResource Resource, int32 Amount)
    { if (Amount > 0) { Facts.Resources.Add(ResourceCost(Unit, Build, Resource, Amount)); } };
    auto Aim = [&Facts](EActionTargetKind Kind, int32 Range, bool bSight, bool bWounded)
    { Facts.Target = Kind; Facts.Range = Range; Facts.bNeedsSight = bSight; Facts.bNeedsWounded = bWounded; };

    // Catalog abilities: the two special slots, and Cure Wounds for catalog builds.
    if (Action == ECombatCommandType::Special1 || Action == ECombatCommandType::Special2 ||
        (Action == ECombatCommandType::CureWounds && !Unit.BuildId.IsEmpty()))
    {
        const TArray<FAbilityFacts> Abilities = FBuildCatalog::Abilities(Unit);
        const FAbilityFacts* Ability = Abilities.FindByPredicate([Action](const FAbilityFacts& A) { return A.Command == Action; });
        if (!Ability) { return false; }
        Facts.Name = Ability->Name;
        Facts.Cost = Ability->bBonus ? EActionCostKind::BonusAction : EActionCostKind::Action;
        if (Ability->bKitResource) { Spend(EActionResource::Kit, Ability->ResourceCost); }
        if (Ability->bCureDice) { Spend(EActionResource::Slot, 1); }
        if (Ability->Effect == EAbilityEffect::Rage) { Spend(EActionResource::Rage, 1); }
        if (IsRageStyle(Build))
        {
            Facts.bForbidsRage = Ability->Effect == EAbilityEffect::Rage;
            Facts.bRequiresRage = !Facts.bForbidsRage;
        }
        Aim(KindOf(Ability->Target), Ability->Range, Ability->Target != EAbilityTarget::Self, Ability->Effect == EAbilityEffect::Healing);
        return true;
    }
    switch (Action)
    {
    case ECombatCommandType::Move:
        Facts.Cost = EActionCostKind::Movement;
        return true;
    case ECombatCommandType::EndTurn:
        Facts.Cost = EActionCostKind::None;
        return true;
    case ECombatCommandType::Attack:
        Facts.Cost = EActionCostKind::Action;
        Aim(EActionTargetKind::Enemy, Unit.Range, true, false);
        return true;
    case ECombatCommandType::PrecisionStrike:
        if (!FBuildCatalog::HasSpecial(Unit, Action)) { return false; }
        Facts.Cost = EActionCostKind::Action;
        Spend(EActionResource::Maneuver, 1);
        Aim(EActionTargetKind::Enemy, Unit.Range, true, false);
        return true;
    case ECombatCommandType::MagicMissile:
        if (!FBuildCatalog::HasSpecial(Unit, Action)) { return false; }
        Facts.Cost = EActionCostKind::Action;
        if (Build) { Spend(EActionResource::Kit, Build->MissileKitCost); }
        Spend(EActionResource::Slot, 1);
        Aim(EActionTargetKind::Enemy, MissileRange, true, false);
        return true;
    case ECombatCommandType::CureWounds: // Fixtures without a catalog build.
        if (!FBuildCatalog::HasSpecial(Unit, Action)) { return false; }
        Facts.Cost = EActionCostKind::Action;
        Spend(EActionResource::Slot, 1);
        Aim(EActionTargetKind::Friendly, TouchRange, true, true);
        return true;
    case ECombatCommandType::SecondWind:
        if (!FBuildCatalog::HasSpecial(Unit, Action)) { return false; }
        Facts.Cost = EActionCostKind::BonusAction;
        Spend(EActionResource::Wind, 1);
        Aim(EActionTargetKind::Self, 0, false, true);
        return true;
    case ECombatCommandType::Rescue:
        Facts.Cost = EActionCostKind::Action;
        Aim(EActionTargetKind::DownedAlly, TouchRange, true, false);
        return true;
    case ECombatCommandType::Potion:
        Facts.Cost = EActionCostKind::Action;
        Spend(EActionResource::Potion, 1);
        Aim(EActionTargetKind::Friendly, TouchRange, true, true);
        return true;
    default:
        return false;
    }
}

/**
 * Why no target is usable, from geometry that ignores budgets: nobody of the right kind, nobody hurt,
 * nobody downed, everyone too far, or everyone behind blocked sight. Adds nothing when a target is usable.
 */
void ClassifyTargets(const FCombatBattle& Battle, const FCombatantState& Unit, ECombatCommandType Action, FActionFacts& Facts)
{
    const FCombatSnapshot& Snapshot = Battle.Snapshot();
    const bool bPartyOnly = Action == ECombatCommandType::Potion || Action == ECombatCommandType::Rescue ||
        (Action == ECombatCommandType::CureWounds && Unit.BuildId.IsEmpty());
    auto OfKind = [&](const FCombatantState& T)
    {
        if (bPartyOnly && Unit.Side != ECombatSide::Party) { return false; }
        const bool bSame = T.Side == Unit.Side;
        switch (Facts.Target)
        {
        case EActionTargetKind::Self: return T.Id == Unit.Id;
        case EActionTargetKind::Enemy: return !bSame && T.HP > 0 && T.Life == ECombatLife::Conscious;
        case EActionTargetKind::Ally: return bSame && T.Id != Unit.Id && T.Life == ECombatLife::Conscious;
        case EActionTargetKind::DownedAlly: return bSame && T.Life == ECombatLife::Downed;
        default: // Friendly: a potion also reaches the downed, everything else needs a conscious friend.
            return bSame && (Action == ECombatCommandType::Potion ? T.Life != ECombatLife::Dead : T.Life == ECombatLife::Conscious);
        }
    };
    TArray<const FCombatantState*> Candidates;
    for (const FCombatantState& T : Snapshot.Encounter.Combatants) { if (OfKind(T)) { Candidates.Add(&T); } }
    if (Candidates.IsEmpty())
    {
        Facts.Blockers.Add({Facts.Target == EActionTargetKind::DownedAlly ? EActionBlocker::NoDownedTarget : EActionBlocker::NoEligibleTarget});
        return;
    }
    if (Facts.bNeedsWounded)
    {
        Candidates = Candidates.FilterByPredicate([](const FCombatantState* T) { return T->HP < T->MaxHP; });
        if (Candidates.IsEmpty()) { Facts.Blockers.Add({EActionBlocker::NoWoundedTarget}); return; }
    }
    const FActionReach Reach = Battle.ActionReach(Unit.Id, Action);
    const TSet<FIntPoint> Visible(Reach.Visible);
    const TSet<FIntPoint> Obstructed(Reach.Obstructed);
    bool bBlockedSight = false;
    for (const FCombatantState* T : Candidates)
    {
        if (Visible.Contains(T->Cell)) { return; }
        bBlockedSight |= Obstructed.Contains(T->Cell);
    }
    FActionBlocker Blocker;
    Blocker.Reason = bBlockedSight ? EActionBlocker::SightBlocked : EActionBlocker::OutOfRange;
    Blocker.Range = Facts.Range;
    Facts.Blockers.Add(Blocker);
}
}

FActionFacts FActionFactsQuery::For(const FCombatBattle& Battle, int32 ActorId, ECombatCommandType Action)
{
    FActionFacts Facts;
    Facts.Command = Action;
    auto Block = [&Facts](EActionBlocker Reason) { Facts.Blockers.Add({Reason}); };
    const FCombatantState* Unit = Battle.Find(ActorId);
    if (!Unit) { Block(EActionBlocker::NotYourTurn); return Facts; }
    const bool bKnown = Shape(*Unit, Action, Facts);
    const FCombatSnapshot& Snapshot = Battle.Snapshot();
    // Turn-level blockers stand alone: budgets and resources mean nothing for someone who cannot act.
    if (Snapshot.Phase != EBattlePhase::Fighting) { Block(EActionBlocker::BattleInactive); return Facts; }
    if (Battle.ActiveId() != ActorId) { Block(EActionBlocker::NotYourTurn); return Facts; }
    if (Unit->Life != ECombatLife::Conscious) { Block(EActionBlocker::ActorDown); return Facts; }
    if (!bKnown) { Block(EActionBlocker::NotInKit); return Facts; }

    if (Facts.Cost == EActionCostKind::Action && !Snapshot.bActionAvailable) { Block(EActionBlocker::ActionSpent); }
    if (Facts.Cost == EActionCostKind::BonusAction && !Snapshot.bBonusActionAvailable) { Block(EActionBlocker::BonusSpent); }
    if (Facts.Cost == EActionCostKind::Movement)
    {
        if (Snapshot.MovementLeft <= 0) { Block(EActionBlocker::NoMovementLeft); }
        else if (Battle.LegalMoves(ActorId).IsEmpty()) { Block(EActionBlocker::NoOpenCell); }
    }
    for (const FActionResourceCost& Cost : Facts.Resources)
    {
        if (Cost.Held >= Cost.Amount) { continue; }
        FActionBlocker Short;
        Short.Reason = EActionBlocker::ResourceShort;
        Short.Resource = Cost.Resource;
        Short.Held = Cost.Held;
        Short.Needed = Cost.Amount;
        Facts.Blockers.Add(Short);
    }
    if (Facts.bRequiresRage && Unit->RageTurns <= 0) { Block(EActionBlocker::StanceRequired); }
    if (Facts.bForbidsRage && Unit->RageTurns > 0) { Block(EActionBlocker::StanceBlocks); }
    if (Facts.Target != EActionTargetKind::None) { ClassifyTargets(Battle, *Unit, Action, Facts); }

    // Availability is the rules' own answer. A mismatch with the blockers is a defect the tests catch.
    if (Action == ECombatCommandType::Move) { Facts.bAvailable = !Battle.LegalMoves(ActorId).IsEmpty(); }
    else if (Action == ECombatCommandType::EndTurn) { Facts.bAvailable = true; }
    else { Facts.bAvailable = !Battle.LegalTargets(ActorId, Action).IsEmpty(); }
    if (!Facts.bAvailable && Facts.Blockers.IsEmpty()) { Block(Action == ECombatCommandType::Move ? EActionBlocker::NoOpenCell : EActionBlocker::NoEligibleTarget); }
    return Facts;
}

TArray<ECombatCommandType> FActionFactsQuery::BarFor(const FCombatBattle& Battle, int32 ActorId)
{
    TArray<ECombatCommandType> Bar = {ECombatCommandType::Attack, ECombatCommandType::Move};
    if (const FCombatantState* Unit = Battle.Find(ActorId))
    { for (const FAbilityFacts& A : FBuildCatalog::Abilities(*Unit)) { Bar.AddUnique(A.Command); } }
    Bar.Add(ECombatCommandType::Rescue);
    Bar.Add(ECombatCommandType::Potion);
    Bar.Add(ECombatCommandType::EndTurn);
    return Bar;
}
