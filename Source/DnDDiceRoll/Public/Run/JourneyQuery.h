#pragma once

#include "CoreMinimal.h"
#include "Run/ExpeditionRun.h"

/**
 * Where the party is in the whole five-stage expedition (#247, spec #239), for the war-table journey map. Facts only (ADR-0001): no
 * display strings beyond the route's own name, no decisions. Derived from the saved FExpeditionProgress alone (History, Offers,
 * EncountersCompleted, Phase, camp flags, revision and ruleset). The query is const: it never rolls, never generates an offer (offers
 * are rolled on arrival at a stage, so a stage the party has not reached is genuinely unknown) and changes no state. No save schema.
 *
 * Every stage is exactly one of:
 *   Cleared  the route taken, its world, its outcome (Victory) and the survivors after the fight;
 *   Lost     the fight the run was defeated in (a defeated run);
 *   Current  the stage the party is at: the two offers (Route phase), the route being fought (Combat), or the next stage while the
 *            reward or the camp is pending (no offers yet);
 *   Fogged   only the kind (approach, pressure, finale) is known; the world is Unknown.
 */

enum class EJourneyState : uint8 { Cleared, Lost, Current, Fogged };

/** One route as the map shows it: what it is called, which world it leads to and how risky the route card calls it. */
struct DNDDICEROLL_API FJourneyRoute
{
    int32 Id = 0;
    FString Name;
    EExpeditionWorld World = EExpeditionWorld::Unknown;
    bool bDangerous = false;
    /** None for SRD runs and catalog revisions before 9, whose route cards never showed a risk. */
    ERouteRisk Risk = ERouteRisk::None;
};

struct DNDDICEROLL_API FJourneyStage
{
    /** 0-based fight index, 0 to FExpeditionRun::EncounterCount - 1. */
    int32 Index = 0;
    EEncounterStage Kind = EEncounterStage::Approach;
    EJourneyState State = EJourneyState::Fogged;
    /** The world this stage is fought in: Unknown while Fogged, and while Current with the offers still open. */
    EExpeditionWorld World = EExpeditionWorld::Unknown;
    /** Cleared, Lost, or Current while the fight is under way: the route taken. Otherwise bTaken is false and Taken is empty. */
    bool bTaken = false;
    FJourneyRoute Taken;
    /** Victory for Cleared, Defeat for Lost, Fighting for the fight under way, and Fighting for a stage with no fight yet. */
    EBattlePhase Outcome = EBattlePhase::Fighting;
    /** Party members alive after the fight (Cleared, Lost); at the fight's start while it is under way; 0 where there is no fight yet. */
    int32 Survivors = 0;
    /** Current in the Route phase: the two routes on offer, in offer order. Empty otherwise. */
    TArray<FJourneyRoute> Offers;
};

struct DNDDICEROLL_API FJourney
{
    EExpeditionPhase Phase = EExpeditionPhase::Route;
    /** Exactly FExpeditionRun::EncounterCount stages, in order. */
    TArray<FJourneyStage> Stages;
    /** The Current stage's index, or INDEX_NONE once the run has ended (Victory or Defeat). */
    int32 CurrentIndex = INDEX_NONE;
    /** The camp is used (Recover(true)): it sat after the fight CampAfterStage. INDEX_NONE if the party skipped it or has not reached it. */
    bool bCampUsed = false;
    int32 CampAfterStage = INDEX_NONE;
    /** The goal the map shows from the first stage on, whatever the fog: the artifact Vault, the world of the finale. */
    EExpeditionWorld Goal = EExpeditionWorld::Vault;
};

struct DNDDICEROLL_API FJourneyQuery
{
    /** The whole journey from saved fields. Safe on any progress: a malformed or partial one yields fogged stages rather than a crash. */
    static FJourney For(const FExpeditionProgress& Progress);
    static FJourney For(const FExpeditionRun& Run) { return For(Run.Progress()); }
};
