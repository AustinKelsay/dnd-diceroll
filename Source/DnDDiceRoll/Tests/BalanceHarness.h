#pragma once

#if WITH_DEV_AUTOMATION_TESTS
#include "CoreMinimal.h"
#include "Combat/ActionFacts.h"
#include "Run/ExpeditionRun.h"

/**
 * Seeded complete-run balance harness (#179, #112 slice 3). Plays Compact expeditions through
 * public FExpeditionRun commands only, with the deterministic party policy documented in
 * docs/runs/gameplay-depth/07-balance-baseline-contract.md, and records what happened.
 * It never builds an FExpeditionSession, so it cannot write a checkpoint.
 */
namespace BalanceHarness
{
inline constexpr int32 BaseSeed = 20261109;
inline constexpr int32 SeedCount = 32;
inline constexpr int32 RotationPasses = 5;
/** A fight that reaches this round without an outcome is a stall. */
inline constexpr int32 MaxRounds = 60;
/** A party turn that takes this many decision steps is a stall. */
inline constexpr int32 MaxSteps = 16;
/** At most this many look-ahead moves per decision (rules 3 and 9). */
inline constexpr int32 MaxLookAheads = 16;
/** Guard on harness loop iterations per run; only a harness bug could reach it. */
inline constexpr int32 RunStepGuard = 100000;
/** Bump when a policy rule changes, so result folders say which policy produced them. */
inline constexpr int32 PolicyVersion = 1;

struct FPartySpec
{
    FString Id;
    FString Archetype;
    TArray<FString> Builds;
};

struct FSpecialStats
{
    ECombatCommandType Command = ECombatCommandType::Special1;
    int32 AvailableTurns = 0;
    int32 Uses = 0;
    /** First blocker at the turn's first step, on turns the special was never available. */
    int32 Blockers[static_cast<int32>(EActionBlocker::Count)] = {};
};

struct FHeroStats
{
    int32 Slot = 0;
    FString Build;
    FString Class;
    bool bSurvived = true;
    int32 DeathStage = 0;
    /** deadline (rescue timer ran out), victory (still Downed when the fight was won) or wipe. */
    FString DeathCause;
    int32 Level = 1;
    int32 Turns = 0, IdleTurns = 0;
    int32 Damage = 0, Kills = 0, Healing = 0, Protection = 0, DamageTaken = 0, Downs = 0, Revives = 0, PotionsUsed = 0;
    TArray<FSpecialStats> Specials;
    int32 KitLeft = 0, KitCap = 0, SlotLeft = 0, SlotCap = 0, WindLeft = 0, WindCap = 0;
    int32 ManeuverLeft = 0, ManeuverCap = 0, RageLeft = 0, RageCap = 0, PotionsLeft = 0;
};

struct FEncounterStats
{
    int32 Stage = 0;
    bool bDangerous = false;
    int32 Recipe = 0, Width = 0, Height = 0, Enemies = 0;
    FString Outcome;
    int32 Rounds = 0, ConsciousStart = 0, HpStart = 0, HpEnd = 0, HpMax = 0;
    /** Lowest party HP (living heroes) after any enemy turn of the fight: the closest call. */
    int32 HpLow = 0;
    int32 Damage = 0, DamageTaken = 0, Healing = 0, Protection = 0, Downs = 0, Deaths = 0, Revives = 0, SpecialsUsed = 0;
    /** Enemy HP removed by party commands whose actor stood next to (Manhattan 1) the enemy that lost it (#188, layout identity). */
    int32 MeleeDamage = 0;
};

struct FRunStats
{
    FString PartyId;
    FString Archetype;
    TArray<FString> Builds;
    int32 Seed = 0;
    int32 RoutePattern = 0;
    /** The catalog revision the run was created under (#188: the before-run plays revision 8). */
    int32 Revision = 0;
    /** Victory, Defeat or Stall. */
    FString Outcome;
    FString StallReason;
    int32 EncountersWon = 0, Rounds = 0, Deaths = 0, Survivors = 0;
    int32 Damage = 0, DamageTaken = 0, Healing = 0, Protection = 0, SpecialsUsed = 0, PotionsUsed = 0, Commands = 0;
    /** Commands the run rejected (gate zero). */
    int32 PolicyErrors = 0;
    /** Proposals outside LegalTargets/LegalMoves, never submitted (gate zero). */
    int32 IllegalProposals = 0;
    /** End Turn with the Action unspent while an Attack, or a move closer to an enemy, was legal (gate zero). */
    int32 PassiveEnds = 0;
    /** Enemy hits whose committed prevention differs from FCombatBattle::Protection before the turn (gate zero). */
    int32 ProtectionMismatches = 0;
    TArray<FHeroStats> Heroes;
    TArray<FEncounterStats> Encounters;
};

/** Plays one complete Compact expedition. RoutePattern bit s picks the Dangerous road at stage s. Revision is the catalog revision the run
 * is created under (FExpeditionRun::NewRun); an unsupported one makes the run a Stall. */
FRunStats PlayRun(const FPartySpec& Party, int32 Seed, int32 RoutePattern, FExpeditionSnapshot* OutFinal = nullptr, FString* OutTrace = nullptr,
    int32 Revision = FBuildCatalog::Revision);
/** Text board and command log for one run, for reproducing a stall or outlier by seed. */
FString TraceRun(const FPartySpec& Party, int32 Seed, int32 RoutePattern, int32 Revision = FBuildCatalog::Revision);
/** DND_BALANCE_REVISION when it names a supported revision (the before-run of a revision change), otherwise the current one;
 * false with Error when it is set to anything else. */
bool RevisionFromEnvironment(int32& OutRevision, FString& Error);
/** A representative or rotation party by id, or nullptr. */
const FPartySpec* FindParty(const FString& Id);
/** Plays every party on seeds BaseSeed + i, i < Seeds, with route pattern i, under Revision. */
TArray<FRunStats> PlayGroup(const TArray<FPartySpec>& Parties, int32 Seeds = SeedCount, int32 Revision = FBuildCatalog::Revision);

TArray<FPartySpec> RepresentativeParties();
/** Pass 0 .. RotationPasses - 1 of the kit rotation (18 parties each). */
TArray<FPartySpec> RotationParties(int32 Pass);

const TCHAR* CommandName(ECombatCommandType Command);
const TCHAR* BlockerName(EActionBlocker Blocker);

FString RunsCsv(const TArray<FRunStats>& Runs);
FString EncountersCsv(const TArray<FRunStats>& Runs);
FString HeroesCsv(const TArray<FRunStats>& Runs);

/**
 * True when Path lies inside the checkout's .local/: lexically after collapsing "..", and on Unix also physically, with
 * every symlink on the path (dangling ones refused) resolved against the real .local/. Otherwise false with an Error.
 */
bool InsideLocal(const FString& Path, FString& Error);
/** The results root: Requested (relative to the project) or .local/balance/<UTC stamp>; refused (empty) unless InsideLocal. */
bool ResolveOutputRoot(const FString& Requested, FString& OutRoot, FString& Error);
/** ResolveOutputRoot of Requested, then the group name, itself checked; empty (with Error) when refused. */
FString GroupFolderFor(const FString& Requested, const FString& Group, FString& Error);
/** GroupFolderFor of DND_BALANCE_OUT. */
FString GroupFolder(const FString& Group, FString& Error);
/** Creates Folder after checking it is InsideLocal, and checks again once it exists. */
bool EnsureFolder(const FString& Folder, FString& Error);
bool WriteGroup(const FString& Folder, const FString& Group, const TArray<FPartySpec>& Parties, const TArray<FRunStats>& Runs, double Seconds, FString& Error);
}
#endif
