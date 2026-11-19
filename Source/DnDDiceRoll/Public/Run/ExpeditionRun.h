#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"
#include "Party/BuildCatalog.h"
#include "ExpeditionRun.generated.h"

UENUM()
enum class EExpeditionPhase : uint8 { Route, Combat, Reward, Recovery, Victory, Defeat };
/** ERuleset lives in Combat/CombatBattle.h (also on FCombatEncounter). */

/** An opportunity is rolled once, disclosed truthfully, and retained when selected. */
USTRUCT()
struct DNDDICEROLL_API FExpeditionRoute
{
    GENERATED_BODY()
    UPROPERTY() int32 Id = 0;
    UPROPERTY() FString Name;
    UPROPERTY() FString Preview;
    UPROPERTY() int32 EncounterSeed = 0;
    UPROPERTY() int32 Recipe = 0;
    UPROPERTY() int32 Width = 8;
    UPROPERTY() int32 Height = 8;
    UPROPERTY() int32 EnemyCount = 2;
    UPROPERTY() bool bDangerous = false;
    UPROPERTY() int32 RewardPotions = 1;
};

USTRUCT()
struct DNDDICEROLL_API FExpeditionEncounterRecord
{
    GENERATED_BODY()
    UPROPERTY() FExpeditionRoute Route;
    UPROPERTY() EBattlePhase Outcome = EBattlePhase::Fighting;
    UPROPERTY() int32 RewardRecipient = INDEX_NONE;
    UPROPERTY() int32 Survivors = 4;
    /** Schema 6 (#218): the card the recipient took, the authoritative record of the claim. Gear on every record without a reward, and on
     * every record of an SRD run or a run created before catalog revision 10. */
    UPROPERTY() ERewardCard RewardCard = ERewardCard::Gear;
};

/** Selection is fixed for a run; instance identity does not imply a class. */
USTRUCT()
struct DNDDICEROLL_API FPartyBuildSelection
{
    GENERATED_BODY()
    UPROPERTY() int32 Id = INDEX_NONE;
    UPROPERTY() FString Name;
    UPROPERTY() FString BuildId;
};

/** All run metadata is reflected, bounded, and independent of presentation lifetime. */
USTRUCT()
struct DNDDICEROLL_API FExpeditionProgress
{
    GENERATED_BODY()
    UPROPERTY() TArray<FPartyBuildSelection> Party;
    UPROPERTY() int32 CatalogRevision = FBuildCatalog::Revision;
    /** Compact (default) or SRD 5.2.1. Schema 4 loads as Compact. */
    UPROPERTY() ERuleset Ruleset = ERuleset::Compact;
    UPROPERTY() int32 Seed = 0;
    UPROPERTY() FString RunId;
    UPROPERTY() int32 GenerationRandomState = 0;
    UPROPERTY() EExpeditionPhase Phase = EExpeditionPhase::Route;
    UPROPERTY() int32 EncountersCompleted = 0;
    UPROPERTY() TArray<FExpeditionRoute> Offers;
    UPROPERTY() TArray<FExpeditionEncounterRecord> History;
    UPROPERTY() bool bCampResolved = false;
    UPROPERTY() bool bCampUsed = false;
    UPROPERTY() bool bArtifactRecovered = false;
};

/** The three stages of an expedition (#188): fights 1-2, fights 3-4 and the fifth, the artifact vault. */
enum class EEncounterStage : uint8 { Approach, Pressure, Finale };

/**
 * The little world a route is fought in, named for the kit that dresses it (UDiceRollTheme::KitFor): Root is the forest of the guarded roads,
 * Vigil the drowned cathedral of the dangerous roads, Vault the finale. Unknown is a world no route has revealed yet (a fogged stage).
 * Derived from the stage and the road alone (FExpeditionRun::WorldOf), so presentation and the journey query agree by construction.
 */
enum class EExpeditionWorld : uint8 { Unknown, Root, Vigil, Vault };

/** The route card's risk word (contract 11). None where the run's rules never disclosed one: SRD runs and catalog revisions before 9. */
enum class ERouteRisk : uint8 { None, Low, Moderate, High, Severe };

/**
 * What presentation may know about one route's encounter (#188), derived only from saved fields: the route's recipe and id,
 * and the run's revision and ruleset. Rules hold no presentation types (ADR-0001); this names the layout, nothing about how it looks.
 */
struct DNDDICEROLL_API FEncounterIdentity
{
    EEncounterStage Stage = EEncounterStage::Approach;
    /** The obstacle family the layout is built from: 0 stones, 1 pillars, 2 walls. */
    int32 ObstacleKind = 0;
    /** The layout's name, as the route preview shows it ("split stones" ... "vault sanctum"); empty for an unknown recipe. */
    FString Layout;
    /** The party enters along the front row (the camera's home side) and the enemies wait at the back; otherwise left to right. */
    bool bFrontToBack = false;
    /** The world the route is fought in: the Vault for the finale, otherwise the Vigil for a dangerous road and the Root for a guarded one. */
    EExpeditionWorld World = EExpeditionWorld::Root;
    /** The risk word the route card shows (None before catalog revision 9 and in SRD runs). */
    ERouteRisk Risk = ERouteRisk::None;
};

/** Detached serialization contract: export both values together. Battle is a
 * query copy, never a second authority; its RNG, turn and party values are exact. */
USTRUCT()
struct DNDDICEROLL_API FExpeditionSnapshot
{
    GENERATED_BODY()
    UPROPERTY() FExpeditionProgress Progress;
    UPROPERTY() FCombatSnapshot Battle;
};

/** Authoritative five-combat expedition. Queries never roll; rejected commands preserve
 * the complete snapshot. There is no mutable battle access or caller-selected layout. */
class DNDDICEROLL_API FExpeditionRun
{
public:
    /** Combats in one expedition; winning the last recovers the artifact. */
    static constexpr int32 EncounterCount = 5;
    /** Revision is the catalog revision the run is created under and keeps for life: the current one, or an older supported one
     * (tests and the balance harness's before-run). Anything outside MinimumRevision..Revision is refused. */
    bool NewRun(int32 Seed, FString& Error, const FString& Identity = FString(), EFighterSubclass Subclass = EFighterSubclass::Champion, ERuleset Ruleset = ERuleset::Compact,
        int32 Revision = FBuildCatalog::Revision);
    bool NewRun(int32 Seed, const TArray<FPartyBuildSelection>& Party, FString& Error, const FString& Identity = FString(), ERuleset Ruleset = ERuleset::Compact,
        int32 Revision = FBuildCatalog::Revision);
    /** Validates a detached candidate in full, then installs without rolling. */
    bool Restore(const FExpeditionSnapshot& Snapshot, FString& Error);
    bool ChooseRoute(int32 RouteId, FString& Error);
    /** The Gear card (every reward before #218, and the only card of SRD and older-revision runs). */
    bool ClaimReward(int32 RecipientId, FString& Error);
    /** Gives the reward to a conscious survivor with one card (#218). Named rejections (wrong phase, a card out of range, a recipient that is
     * not a conscious survivor, "Gear only in this run", "Reserve taken", "Already honed") leave the full snapshot unchanged. */
    bool ClaimReward(int32 RecipientId, ERewardCard Card, FString& Error);
    /** True when runs of Ruleset created under Revision offer Reserve and Honed: Compact runs from catalog revision 10. */
    static bool OffersUpgradeCards(ERuleset Ruleset, int32 Revision);
    /** True when runs of Ruleset created under Revision move through allies, Downed and dead creatures (FCombatEncounter::bMoveThrough, #240):
     * Compact runs from catalog revision 11. Revisions 7-10 and SRD keep the older rule, under which every unit blocks its cell. */
    static bool MovesThroughUnits(ERuleset Ruleset, int32 Revision);
    /** True when runs of Ruleset created under Revision play catalog revision 11's balance (#241, M2 contract): Compact runs from revision 11.
     * It selects the revision 11 enemy numbers, the Monk and Paladin profiles, enemy target choice (FCombatEncounter::bNearestTarget), Rescue
     * (bSturdyRescue) and the cards' refill (CardsRefill). */
    static bool PlaysRevision11Balance(ERuleset Ruleset, int32 Revision);
    /** The hero NewRun creates from BuildId for Ruleset and Revision: the catalog profile (FBuildCatalog::Create), with revision 11's Monk and
     * Paladin adjustments in Compact (#241). The party builder previews with it, so its HP and AC are the ones the run starts with. */
    static bool CreateHero(const FString& BuildId, int32 Id, const FString& Name, ERuleset Ruleset, int32 Revision, FCombatantState& Out);
    /** True when this run's Reserve and Honed cards refill the recipient's special pools to its caps (Revision11Rules::CardsRefill): Compact
     * from revision 11. Reserve then refills to its new cap instead of adding one use. */
    static bool CardsRefill(ERuleset Ruleset, int32 Revision);
    bool Recover(bool bTakeRecovery, FString& Error);
    FCombatResult Submit(const FCombatCommand& Command);
    TArray<FCombatResult> PlayEnemyTurn();
    const FCombatBattle& Battle() const { return Combat; }
    const FExpeditionProgress& Progress() const { return State; }
    FExpeditionSnapshot Snapshot() const { return {State, Combat.Snapshot()}; }
    static FString RecoveryDescription();
    /** The identity of Route's encounter in a run with Progress's revision and ruleset. A const query: it never rolls or validates. */
    static FEncounterIdentity Encounter(const FExpeditionProgress& Progress, const FExpeditionRoute& Route);
    /** The stage kind of a 0-based fight index: fights 0-1 approach, 2-3 pressure, 4 the finale. */
    static EEncounterStage StageKind(int32 Stage);
    /** The world a route at Stage is fought in: the Vault for the finale, otherwise the Vigil if bDangerous, else the Root. */
    static EExpeditionWorld WorldOf(EEncounterStage Stage, bool bDangerous);
    /** The route card's word for a risk ("low", "moderate", "high", "severe"); empty for None. */
    static const TCHAR* RiskWord(ERouteRisk Risk);
    /** The encounter Route generates at Stage (0-based) under Ruleset and Revision: board, obstacles, spawns and enemies, with
     * the tutorial party standing on the party spawns. False for a recipe the stage or revision does not use, or an unsupported
     * ruleset or revision. A pure query: Route.EnemyCount is not read (generation and restore check it against the result). */
    static bool EncounterLayout(const FExpeditionRoute& Route, int32 Stage, ERuleset Ruleset, int32 Revision, FCombatEncounter& Out);
private:
    FCombatBattle Combat;
    FExpeditionProgress State;
    bool bStarted = false;
    void OfferRoutes();
    void ResolveCombat();
    FCombatEncounter Generate(const FExpeditionRoute& Route) const;
};
