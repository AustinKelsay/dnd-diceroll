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
    UPROPERTY() int32 Version = 5;
    UPROPERTY() FExpeditionSnapshot Current;
    UPROPERTY() TArray<FExpeditionEnding> Records;
};

/** Local file adapter. Strict decoding + domain validation; failed reads/writes never
 * change output/live state or replace the prior file. Single desktop player only. */
class DNDDICEROLL_API FExpeditionSave
{
public:
    // Version 5 adds persisted rules state (conditions, reactions, attacks, terrain, slots, equipment); older saves are unsupported.
    static constexpr int32 Version = 5;
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
    bool Recover(bool bTake, FString& Error);
    bool ShortRest(int32 Id, int32& Healed, FString& Error);
    bool CampActivity(int32 Id, ECampActivity Activity, FString& Outcome, FString& Error);
    FCombatResult Submit(const FCombatCommand& Command);
    TArray<FCombatResult> PlayEnemyTurn();
    bool Save(FString& Error);
    bool Resume(FString& Error);
    bool ReadRecords(TArray<FExpeditionEnding>& Records, FString& Error) const;
private:
    FString SavePath;
    FExpeditionRun Current;
    bool bHasRun = false;
    bool Commit(FExpeditionRun&& Candidate, FString& Error);
};
