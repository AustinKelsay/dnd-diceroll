#pragma once

#include "CoreMinimal.h"
#include "Run/ExpeditionRun.h"
#include "ExpeditionSave.generated.h"

USTRUCT()
struct DNDDICEROLL_API FExpeditionEnding
{
    GENERATED_BODY()
    UPROPERTY() FString RunId;
    UPROPERTY() int32 Seed = 0;
    UPROPERTY() EExpeditionPhase Outcome = EExpeditionPhase::Defeat;
    UPROPERTY() int32 Victories = 0;
    UPROPERTY() TArray<int32> Survivors;
    UPROPERTY() TArray<FString> SurvivorNames;
    UPROPERTY() TArray<FString> SurvivorBuilds;
    UPROPERTY() FString Summary;
};

/** One replacement commits the current checkpoint and its deduplicated ending together. */
USTRUCT()
struct DNDDICEROLL_API FExpeditionSaveData
{
    GENERATED_BODY()
    UPROPERTY() int32 Version = 6;
    UPROPERTY() FExpeditionSnapshot Current;
    UPROPERTY() TArray<FExpeditionEnding> Records;
};

/** Local file adapter. Strict decoding + domain validation; failed reads/writes never
 * change output/live state or replace the prior file. Single desktop player only. */
class DNDDICEROLL_API FExpeditionSave
{
public:
    // Version 5 persists ERuleset; version 6 (#218) the reward card of each claim and each hero's Reserve and Honed. Older documents
    // decode through explicit step migrations, 4 -> 5 (Compact) and 5 -> 6 (gear only); older than 4, and any unknown version, is refused.
    static constexpr int32 Version = 6;
    static bool Encode(const FExpeditionSaveData& Data, FString& Json, FString& Error);
    static bool Decode(const FString& Json, FExpeditionSaveData& Data, FString& Error);
    static bool Write(const FString& Path, const FExpeditionSaveData& Data, FString& Error);
    static bool Read(const FString& Path, FExpeditionSaveData& Data, FString& Error);
    static FExpeditionEnding Ending(const FExpeditionRun& Run);
    static FString DefaultPath();
private:
    static bool Validate(const FExpeditionSaveData& Data, FString& Error);
};

/** All accepted gameplay is checkpointed before installation. Failed commit rejects the
 * candidate command (including RNG/deaths); Resume always reads the latest file.
 * Startup is read-only. History never enters run construction or combat calculations. */
class DNDDICEROLL_API FExpeditionSession
{
public:
    explicit FExpeditionSession(FString Path = FExpeditionSave::DefaultPath()) : SavePath(MoveTemp(Path)) {}
    bool HasRun() const { return bHasRun; }
    const FExpeditionRun& Run() const { return Current; }
    /** Never replaces an unfinished expedition. At startup only, an unreadable checkpoint
     * is preserved beside the save before an explicit new run. Optional output identifies it. */
    bool NewRun(int32 Seed, const TArray<FPartyBuildSelection>& Party, FString& Error, const FString& Identity = FString(), FString* PreservedCheckpoint = nullptr);
    bool NewRun(int32 Seed, FString& Error, const FString& Identity = FString(), FString* PreservedCheckpoint = nullptr, EFighterSubclass Subclass = EFighterSubclass::Champion);
    bool ChooseRoute(int32 Id, FString& Error);
    bool ClaimReward(int32 Id, FString& Error);
    /** The run's card claim (#218), committed before it is installed: a failed write rejects it and leaves state, RNG and file unchanged. */
    bool ClaimReward(int32 Id, ERewardCard Card, FString& Error);
    bool Recover(bool bTake, FString& Error);
    FCombatResult Submit(const FCombatCommand& Command);
    TArray<FCombatResult> PlayEnemyTurn();
    bool Save(FString& Error);
    bool Resume(FString& Error);
    bool ReadRecords(TArray<FExpeditionEnding>& Records, FString& Error) const;
    /** Puts the live run away without writing anything. The checkpoint stays exactly as
     * last committed, so Resume continues the run and New Run still refuses to replace it. */
    void Close();
    /** True when the checkpoint holds a readable, unfinished run that has not been abandoned, so New Run would refuse. Read-only. */
    bool HasUnfinishedCheckpoint() const;
    /**
     * Abandon the unfinished checkpoint from the start screen (#224): only with no live run and a readable, unfinished checkpoint. It writes one
     * small marker beside the checkpoint naming that run, atomically; the checkpoint itself is never written, so on failure (or cancel) its bytes
     * are unchanged. While the marker names the checkpoint's run, Resume refuses it, HasUnfinishedCheckpoint is false and New Run replaces it
     * exactly as after an ending, keeping the records of finished runs. The abandoned run is not recorded: the records format can only hold
     * victories and defeats, and the save schema does not change for this.
     */
    bool Abandon(FString& Error);
    /** The marker's path: the checkpoint path plus ".abandoned". */
    FString AbandonMarkerPath() const { return SavePath + TEXT(".abandoned"); }
private:
    FString SavePath;
    FExpeditionRun Current;
    bool bHasRun = false;
    bool Commit(FExpeditionRun&& Candidate, FString& Error);
    /** True when the abandon marker names RunId. */
    bool IsAbandoned(const FString& RunId) const;
};
