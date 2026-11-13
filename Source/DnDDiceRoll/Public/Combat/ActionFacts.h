#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"

/**
 * What a command costs, whether the actor can use it now, and exactly why not (#58, #112 slice 2).
 * Facts only: no display strings and no decisions. Presentation formats them. The rules stay
 * authoritative: bAvailable is defined by FCombatBattle::LegalTargets/LegalMoves, never re-derived.
 *
 * Names avoid the SRD ruleset's EActionCost (PR #95) so both can exist in one build. SRD mode extends
 * these enums additively (a Reaction cost, blockers such as ReactionSpent or Grappled); Compact never emits them.
 */
enum class EActionCostKind : uint8 { Action, BonusAction, Movement, None };
enum class EActionResource : uint8 { None, Kit, Slot, Wind, Maneuver, Rage, Potion };
/** Ally means a party member other than the actor; Friendly includes the actor. Healing also sets bNeedsWounded. */
enum class EActionTargetKind : uint8 { None, Self, Enemy, Ally, Friendly, DownedAlly };
/** Fixed order: this is the order blockers are reported in. The first four stand alone. */
enum class EActionBlocker : uint8
{
    BattleInactive, NotYourTurn, ActorDown, NotInKit,
    ActionSpent, BonusSpent, NoMovementLeft, NoOpenCell,
    ResourceShort, StanceRequired, StanceBlocks,
    NoWoundedTarget, NoDownedTarget, OutOfRange, SightBlocked, NoEligibleTarget,
    Count
};

/** One resource an action spends. Cap is the hero's own (FBuildCatalog::Caps, Reserve included); CampRecovery is the build's. INDEX_NONE when unknown. */
struct DNDDICEROLL_API FActionResourceCost
{
    EActionResource Resource = EActionResource::None;
    int32 Amount = 0;
    int32 Held = 0;
    int32 Cap = INDEX_NONE;
    int32 CampRecovery = INDEX_NONE;
};

/** Why an action is unavailable. Value fields depend on Reason; unused ones stay zero. */
struct DNDDICEROLL_API FActionBlocker
{
    EActionBlocker Reason = EActionBlocker::NoEligibleTarget;
    /** ResourceShort: which resource, how many are held and how many are needed. */
    EActionResource Resource = EActionResource::None;
    int32 Held = 0;
    int32 Needed = 0;
    /** OutOfRange: the action's range in cells. */
    int32 Range = 0;
};

struct DNDDICEROLL_API FActionFacts
{
    ECombatCommandType Command = ECombatCommandType::Attack;
    /** The catalog ability's name; empty for the fixed actions (attack, move, rescue, potion, end turn). */
    FString Name;
    EActionCostKind Cost = EActionCostKind::None;
    /** Resources the action spends when it resolves, in the order they are shown. */
    TArray<FActionResourceCost> Resources;
    /** Rage-stance gating of the Barbarian kits: needs an active Rage, or cannot start one while raging. */
    bool bRequiresRage = false;
    bool bForbidsRage = false;
    EActionTargetKind Target = EActionTargetKind::None;
    /** Healing: only a target below full health is eligible. */
    bool bNeedsWounded = false;
    int32 Range = 0;
    bool bNeedsSight = false;
    bool bAvailable = false;
    /** Empty exactly when bAvailable. */
    TArray<FActionBlocker> Blockers;

    bool Blocks(EActionBlocker Reason) const
    { return Blockers.ContainsByPredicate([Reason](const FActionBlocker& B) { return B.Reason == Reason; }); }
};

/** Const queries over the authoritative battle. They consume no random numbers and change nothing. */
struct DNDDICEROLL_API FActionFactsQuery
{
    static FActionFacts For(const FCombatBattle& Battle, int32 ActorId, ECombatCommandType Action);
    /**
     * What a command costs and needs for this adventurer, with no battle: cost, resources (held, cap, camp recovery),
     * stance and target shape. It runs the same shape code as For, so the two agree on a fresh battle; bAvailable
     * and Blockers stay empty because availability needs a battle. Cost stays None for a command the kit lacks.
     */
    static FActionFacts ForBuild(const FCombatantState& Unit, ECombatCommandType Action);
    /** The commands an adventurer's action bar offers, in bar order: attack, move, catalog abilities, rescue, potion, end turn. */
    static TArray<ECombatCommandType> BarFor(const FCombatBattle& Battle, int32 ActorId);
};
