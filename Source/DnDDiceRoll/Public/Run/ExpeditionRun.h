#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"
#include "ExpeditionRun.generated.h"

UENUM()
enum class EExpeditionPhase : uint8 { Route, Combat, Reward, Recovery, Victory, Defeat };

/** Rest-point activities after a victory: one try each per rest point, keyed rolls (never shift the run stream). */
enum class ECampActivity : uint8 { PickLock, Parley, BuyPotion, Forage, Pickpocket, Perform, CalmBeast, TreatWounds, Count };

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
    /** Camp activities: extra potions added to this victory's loot, and a bit per activity already tried. */
    UPROPERTY() int32 BonusPotions = 0;
    UPROPERTY() int32 ActivitiesUsed = 0;
    /** Route hazard met on the way (EHazard index, -1 none) and whether the party passed its check. */
    UPROPERTY() int32 Hazard = -1;
    UPROPERTY() bool bHazardPassed = false;
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
    UPROPERTY() int32 CatalogRevision = 7;
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
    /** Party purse in copper: a purse per victory, spent at the rest point. */
    UPROPERTY() int32 Coins = 0;
    /** Off by default: dangerous routes roll an exploration hazard (keyed) before the fight. */
    UPROPERTY() bool bRouteHazards = false;
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
    bool NewRun(int32 Seed, FString& Error, const FString& Identity = FString(), EFighterSubclass Subclass = EFighterSubclass::Champion);
    bool NewRun(int32 Seed, const TArray<FPartyBuildSelection>& Party, FString& Error, const FString& Identity = FString());
    /** Validates a detached candidate in full, then installs without rolling. */
    bool Restore(const FExpeditionSnapshot& Snapshot, FString& Error);
    bool ChooseRoute(int32 RouteId, FString& Error);
    bool ClaimReward(int32 RecipientId, FString& Error);
    bool Recover(bool bTakeRecovery, FString& Error);
    /** #76: at the rest point after a victory, a survivor spends one hit die (roll + CON) to heal. */
    bool ShortRest(int32 UnitId, int32& Healed, FString& Error);
    /** Rest-point activity by a living survivor (lock picking, ...). Outcome describes the roll. */
    bool CampActivity(int32 UnitId, ECampActivity Activity, FString& Outcome, FString& Error);
    static FString CampActivityName(ECampActivity Activity);
    static constexpr int32 VictoryPurseCopper = 2500;
    static constexpr int32 PickpocketCopper = 1000;
    static constexpr int32 PerformanceCopper = 500;
    /** Toggle route hazards while choosing a route. */
    bool SetRouteHazards(bool bEnabled, FString& Error);
    static FString HazardDescription(const FExpeditionEncounterRecord& Record);
    FCombatResult Submit(const FCombatCommand& Command);
    TArray<FCombatResult> PlayEnemyTurn();
    const FCombatBattle& Battle() const { return Combat; }
    const FExpeditionProgress& Progress() const { return State; }
    FExpeditionSnapshot Snapshot() const { return {State, Combat.Snapshot()}; }
    static FString RecoveryDescription();
private:
    FCombatBattle Combat;
    FExpeditionProgress State;
    bool bStarted = false;
    void OfferRoutes();
    void ResolveCombat();
    FCombatEncounter Generate(const FExpeditionRoute& Route) const;
    int32 CampD20(int32 UnitId, ECampActivity Activity) const;
};
