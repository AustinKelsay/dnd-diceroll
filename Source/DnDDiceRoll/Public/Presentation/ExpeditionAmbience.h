#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ExpeditionAmbience.generated.h"

class USoundClass;
class USoundWave;
enum class EDiceRollKit : uint8;

/** The looping ambience beds (#244). None is silence: the start screen, and anything with no recording. */
UENUM(BlueprintType)
enum class EExpeditionAmbience : uint8
{
    None,
    /** The candlelit war table between fights: candle crackle and room tone. */
    WarTable,
    /** The Drowned Vigil: dripping stone nave and distant wind. */
    Vigil,
    /** The Root of the World: leaves, wind, insects and birds. */
    Root,
    /** The Vault: a low hum and the Waystone's resonance. */
    Vault
};

constexpr int32 ExpeditionAmbienceCount = static_cast<int32>(EExpeditionAmbience::Vault) + 1;

namespace ExpeditionAmbience
{
/** The bed data asset the battle view loads. */
constexpr const TCHAR* SoundsPath = TEXT("/Game/DnDDiceRoll/Audio/DA_ExpeditionAmbience.DA_ExpeditionAmbience");
/** How long a bed takes to fade in while the previous one fades out. */
constexpr float CrossfadeSeconds = 2.5f;
/**
 * The bed for what the board shows. No run in play (the start screen, including after Return to menu) is silence; the war-table
 * overview plays the war table; a world board plays the bed of the kit it was drawn with (the Vault finale drawn with the Vigil
 * kit when the Vault kit is incomplete plays the Vigil, so the bed always matches the world on screen).
 */
DNDDICEROLL_API EExpeditionAmbience BedFor(bool bRunInPlay, bool bOverview, EDiceRollKit Kit);
/** The bed's enum name ("WarTable"), for logs and asset names. */
DNDDICEROLL_API FString Name(EExpeditionAmbience Bed);
}

/** One bed's recording and level (#244). */
USTRUCT(BlueprintType)
struct DNDDICEROLL_API FExpeditionAmbienceBed
{
    GENERATED_BODY()
    /** A seamless loop (the importer sets it looping). */
    UPROPERTY(EditAnywhere, Category = "Audio") TSoftObjectPtr<USoundWave> Loop;
    /** Multiplies the imported loudness (the loops are already normalised; 1 plays them as measured). */
    UPROPERTY(EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.0")) float Volume = 1.0f;
    static constexpr float MaxVolume = 2.0f;
};

/**
 * The ambience beds (#244), created by Content/Python/import_expedition_ambience.py at /Game/DnDDiceRoll/Audio/DA_ExpeditionAmbience.
 * Optional as a whole and per bed: a bed with no loadable recording is silence. Ambience is never synthesized.
 */
UCLASS(BlueprintType)
class DNDDICEROLL_API UExpeditionAmbienceSounds : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Audio") TMap<EExpeditionAmbience, FExpeditionAmbienceBed> Beds;
    /** The Ambience class every bed plays through (the mix of #246 sets its volume). Optional. */
    UPROPERTY(EditAnywhere, Category = "Audio") TSoftObjectPtr<USoundClass> AmbienceClass;
};

/** The loaded beds. A bed that is unset or fails to load is absent, and plays as silence. */
USTRUCT()
struct DNDDICEROLL_API FExpeditionAmbienceBank
{
    GENERATED_BODY()

    /** Replaces the bank with what loads from Sounds (null empties it: every bed is silence). */
    void Load(const UExpeditionAmbienceSounds* Sounds);
    void Reset();
    /** The bed's loop, or null for silence. */
    USoundWave* Wave(EExpeditionAmbience Bed) const;
    float Volume(EExpeditionAmbience Bed) const;
    USoundClass* Class() const { return AmbienceClass; }
    /** The beds that have a recording, for the load summary and tests. */
    TArray<EExpeditionAmbience> RecordedBeds() const;

private:
    UPROPERTY() TMap<EExpeditionAmbience, TObjectPtr<USoundWave>> Waves;
    TMap<EExpeditionAmbience, float> Volumes;
    UPROPERTY() TObjectPtr<USoundClass> AmbienceClass;
};
