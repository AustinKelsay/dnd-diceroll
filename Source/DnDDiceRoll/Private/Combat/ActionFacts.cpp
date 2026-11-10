#include "Combat/ActionFacts.h"
#include "Party/BuildCatalog.h"
#include "Rules/StandardActionRules.h"
#include "Rules/ClassFeatureRules.h"
#include "Rules/GrappleRules.h"

namespace
{
using namespace BuildRules;

/** The most bottles a combatant can hold: the limit FCombatBattle validates and FPartyContinuity::Reward clamps to. */
constexpr int32 PotionBottleCap = 9;

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
    case EActionResource::None: return 0;
    }
    return 0;
}

FActionResourceCost ResourceCost(const FCombatantState& Unit, const FBuildDefinition* Build, EActionResource Resource, int32 Amount)
{
    FActionResourceCost Cost;
    Cost.Resource = Resource;
    Cost.Amount = Amount;
    Cost.Held = HeldOf(Unit, Resource);
    if (Resource == EActionResource::Potion) { Cost.Cap = PotionBottleCap; Cost.CampRecovery = 0; } // Camp never refills potions.
    else if (Build)
    {
        switch (Resource)
        {
        case EActionResource::Kit: Cost.Cap = Build->KitCap; Cost.CampRecovery = Build->KitRecovery; break;
        case EActionResource::Slot: Cost.Cap = Build->SlotCap; Cost.CampRecovery = Build->SlotRecovery; break;
        case EActionResource::Wind: Cost.Cap = Build->WindCap; Cost.CampRecovery = Build->WindRecovery; break;
        case EActionResource::Maneuver: Cost.Cap = Build->ManeuverCap; Cost.CampRecovery = Build->ManeuverRecovery; break;
        case EActionResource::Rage: Cost.Cap = Build->RageCap; Cost.CampRecovery = Build->RageRecovery; break;
        case EActionResource::None:
        case EActionResource::Potion: break;
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
    case EAbilityTarget::Friendly: return EActionTargetKind::Friendly;
    }
    return EActionTargetKind::Friendly;
}

/**
 * What the command is: its cost, resources, stance and target shape. Mirrors how FCombatBattle::Resolve
 * dispatches and what it spends, not how descriptions word it. Returns false when the actor's kit lacks it.
 */
bool Shape(const FCombatantState& Unit, ECombatCommandType Action, bool bSrd, bool bActionAlreadyUsed, FActionFacts& Facts)
{
    const FBuildDefinition* Build = FBuildCatalog::Find(Unit.BuildId);
    auto Spend = [&](EActionResource Resource, int32 Amount)
    { if (Amount > 0) { Facts.Resources.Add(ResourceCost(Unit, Build, Resource, Amount)); } };
    auto Aim = [&Facts](EActionTargetKind Kind, int32 Range, bool bSight, bool bWounded)
    { Facts.Target = Kind; Facts.Range = Range; Facts.bNeedsSight = bSight; Facts.bNeedsWounded = bWounded; };


    if (bSrd)
    {
        if (Action == ECombatCommandType::Dash || Action == ECombatCommandType::Disengage)
        {
            const EStandardAction Standard = Action == ECombatCommandType::Dash ? EStandardAction::Dash :
                EStandardAction::Disengage;
            Facts.Cost = FStandardActionRules::Cost(Standard, Unit.AdventurerRole, Unit.Level, bActionAlreadyUsed) == EActionCost::BonusAction ?
                EActionCostKind::BonusAction : EActionCostKind::Action;
            Facts.Name = Action == ECombatCommandType::Dash ? TEXT("Dash") : TEXT("Disengage");
            Aim(EActionTargetKind::Self, 0, false, false); return true;
        }
        if (Action == ECombatCommandType::Grapple || Action == ECombatCommandType::Shove || Action == ECombatCommandType::Escape)
        {
            Facts.Name = Action == ECombatCommandType::Grapple ? TEXT("Grapple") : Action == ECombatCommandType::Shove ? TEXT("Shove") : TEXT("Escape");
            Facts.Cost = EActionCostKind::Action;
            Aim(EActionTargetKind::Enemy, TouchRange, false, false); return true;
        }
        if (Action == ECombatCommandType::ActionSurge)
        {
            Facts.Name = TEXT("Action Surge"); Facts.Cost = EActionCostKind::None;
            Aim(EActionTargetKind::Self, 0, false, false);
            return Unit.AdventurerRole == EAdventurerRole::Fighter && FClassFeatureRules::ActionSurgeUses(Unit.Level)>0;
        }
    }

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
        const bool bSame = (T.Side == ECombatSide::Enemy) == (Unit.Side == ECombatSide::Enemy);
        if (Action == ECombatCommandType::Escape) { return T.Id == Unit.GrapplerId && T.Life == ECombatLife::Conscious; }
        if ((Action == ECombatCommandType::Grapple || Action == ECombatCommandType::Shove) && !FGrappleRules::SizeAllows(static_cast<ECreatureSize>(Unit.Size), static_cast<ECreatureSize>(T.Size))) { return false; }
        switch (Facts.Target)
        {
        case EActionTargetKind::Self: return T.Id == Unit.Id;
        case EActionTargetKind::Enemy: return !bSame && T.HP > 0 && T.Life == ECombatLife::Conscious;
        case EActionTargetKind::Ally: return bSame && T.Id != Unit.Id && T.Life == ECombatLife::Conscious;
        case EActionTargetKind::DownedAlly: return bSame && T.Life == ECombatLife::Downed;
        case EActionTargetKind::Friendly: // A potion also reaches the downed, everything else needs a conscious friend.
            return bSame && (Action == ECombatCommandType::Potion ? T.Life != ECombatLife::Dead : T.Life == ECombatLife::Conscious);
        case EActionTargetKind::None: return false;
        }
        return false;
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
    if (Action == ECombatCommandType::Escape ||
        (Facts.Target == EActionTargetKind::Self && !Facts.bNeedsSight)) { return; }
    if (!Facts.bNeedsSight)
    {
        for (const FCombatantState* T : Candidates)
        { if (FMath::Abs(Unit.Cell.X-T->Cell.X) + FMath::Abs(Unit.Cell.Y-T->Cell.Y) <= Facts.Range) { return; } }
        FActionBlocker B; B.Reason=EActionBlocker::OutOfRange; B.Range=Facts.Range; Facts.Blockers.Add(B); return;
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
    const bool bSrd = (Battle.Snapshot().Encounter.Ruleset == ERuleset::SRD);
    const bool bKnown = Shape(*Unit, Action, bSrd, !Battle.Snapshot().bActionAvailable, Facts);
    const FCombatSnapshot& Snapshot = Battle.Snapshot();
    // Turn-level blockers stand alone: budgets and resources mean nothing for someone who cannot act.
    if (Snapshot.Phase != EBattlePhase::Fighting) { Block(EActionBlocker::BattleInactive); return Facts; }
    if (Battle.ActiveId() != ActorId) { Block(EActionBlocker::NotYourTurn); return Facts; }
    if (Unit->Life != ECombatLife::Conscious) { Block(EActionBlocker::ActorDown); return Facts; }
    if (!bKnown) { Block(EActionBlocker::NotInKit); return Facts; }

    if (Facts.Cost == EActionCostKind::Action && !Snapshot.bActionAvailable &&
        !(bSrd && (Action == ECombatCommandType::Attack || Action == ECombatCommandType::PrecisionStrike || Action == ECombatCommandType::Grapple || Action == ECombatCommandType::Shove) && Snapshot.AttacksLeft > 0)) { Block(EActionBlocker::ActionSpent); }
    if (Facts.Cost == EActionCostKind::BonusAction && !Snapshot.bBonusActionAvailable) { Block(EActionBlocker::BonusSpent); }
    if (Facts.Cost == EActionCostKind::Movement)
    {
        if (Snapshot.MovementLeft <= 0) { Block(EActionBlocker::NoMovementLeft); }
        else if (Battle.LegalMoves(ActorId).IsEmpty()) { Block(EActionBlocker::NoOpenCell); }
    }
    if (bSrd)
    {
        if (Action == ECombatCommandType::Move && Unit->GrapplerId != INDEX_NONE) { Block(EActionBlocker::Grappled); }
        if (Action == ECombatCommandType::Escape && Unit->GrapplerId == INDEX_NONE) { Block(EActionBlocker::NotGrappled); }
        if (Action == ECombatCommandType::ActionSurge && Unit->ActionSurgeUses <= 0) { Block(EActionBlocker::NoClassCharge); }
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
    Facts.Blockers.StableSort([](const FActionBlocker& A,const FActionBlocker& B) { return static_cast<uint8>(A.Reason)<static_cast<uint8>(B.Reason); });
    return Facts;
}

FActionFacts FActionFactsQuery::ForBuild(const FCombatantState& Unit, ECombatCommandType Action)
{
    FActionFacts Facts;
    Facts.Command = Action;
    if (!Shape(Unit, Action, Facts)) { return FActionFacts(); }
    return Facts;
}

TArray<ECombatCommandType> FActionFactsQuery::BarFor(const FCombatBattle& Battle, int32 ActorId)
{
    TArray<ECombatCommandType> Bar = {ECombatCommandType::Attack, ECombatCommandType::Move};
    if ((Battle.Snapshot().Encounter.Ruleset == ERuleset::SRD))
    {
        for (auto A : {ECombatCommandType::Dash,ECombatCommandType::Disengage,
            ECombatCommandType::Grapple,ECombatCommandType::Shove,ECombatCommandType::Escape}) { Bar.AddUnique(A); }
    }
    if (const FCombatantState* Unit = Battle.Find(ActorId))
    {
        if ((Battle.Snapshot().Encounter.Ruleset == ERuleset::SRD) && Unit->AdventurerRole == EAdventurerRole::Fighter && FClassFeatureRules::ActionSurgeUses(Unit->Level)>0) { Bar.AddUnique(ECombatCommandType::ActionSurge); }
        for (const FAbilityFacts& A : FBuildCatalog::Abilities(*Unit)) { Bar.AddUnique(A.Command); }
    }
    Bar.Add(ECombatCommandType::Rescue);
    Bar.Add(ECombatCommandType::Potion);
    Bar.Add(ECombatCommandType::EndTurn);
    return Bar;
}
