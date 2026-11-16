#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Presentation/ExpeditionCue.h"
#include "ExpeditionCueSounds.generated.h"

class USoundClass;
class USoundWave;

/** One cue's recorded variations and its volume (#242). */
USTRUCT(BlueprintType)
struct DNDDICEROLL_API FExpeditionCueSound
{
    GENERATED_BODY()
    /** Takes of the same moment; a presentation-only hash picks one per play. One to four. */
    UPROPERTY(EditAnywhere, Category = "Audio") TArray<TSoftObjectPtr<USoundWave>> Variations;
    /** Multiplies the imported loudness (the sources are already normalised per category; 1 plays them as measured). */
    UPROPERTY(EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.0")) float Volume = 1.0f;
    /** The range the bank applies at runtime; the metadata clamp above only guides the editor. */
    static constexpr float MaxVolume = 2.0f;
};

/**
 * The recorded expedition cues (#242): each cue's variations and volume, plus the sound class they play through. Created by
 * Content/Python/import_expedition_cues.py at /Game/DnDDiceRoll/Audio/DA_ExpeditionCueSounds. Optional as a whole and per cue:
 * a cue with no loadable variation plays the old synthesis (ExpeditionAudio::Synthesize).
 */
UCLASS(BlueprintType)
class DNDDICEROLL_API UExpeditionCueSounds : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Audio") TMap<EExpeditionCue, FExpeditionCueSound> Cues;
    /** The Effects class every cue plays through (a stub that #246 builds the mix on). Optional. */
    UPROPERTY(EditAnywhere, Category = "Audio") TSoftObjectPtr<USoundClass> EffectsClass;
};

/** A loaded cue: the variations that resolved, in their data-asset order. */
USTRUCT()
struct DNDDICEROLL_API FExpeditionLoadedCue
{
    GENERATED_BODY()
    UPROPERTY() TArray<TObjectPtr<USoundWave>> Waves;
    float Volume = 1.0f;
};

/** What one play of a cue resolved to: a recorded wave, or none (play synthesis variation SynthVariation). */
struct FExpeditionCuePlay
{
    USoundWave* Wave = nullptr;
    float Volume = 1.0f;
    /** The variation index the hash chose among the loaded waves (or among the four synthesis variations). */
    int32 Variation = 0;
};

/**
 * The loaded recorded cues. Load() resolves every variation once; an unset variation, or one that fails to load, is dropped, and a cue left
 * with none falls back to synthesis on its own, so one cue's failure never silences another. Choose() is pure presentation: the variation
 * comes from ExpeditionAudio::PickVariation, never from run or combat RNG (ADR-0001).
 */
USTRUCT()
struct DNDDICEROLL_API FExpeditionCueBank
{
    GENERATED_BODY()

    /** Replaces the bank with what loads from Sounds (null empties it: every cue uses synthesis). */
    void Load(const UExpeditionCueSounds* Sounds);
    void Reset();
    /** Loaded variations of a cue (0: synthesis). */
    int32 Count(EExpeditionCue Cue) const;
    /** The play for the Serial-th time this cue sounds. */
    FExpeditionCuePlay Choose(EExpeditionCue Cue, uint32 Serial) const;
    USoundClass* Class() const { return EffectsClass; }
    /** The cues that play synthesis (no loadable variation), for the load summary and tests. */
    TArray<EExpeditionCue> SynthesizedCues() const;

private:
    UPROPERTY() TMap<EExpeditionCue, FExpeditionLoadedCue> Cues;
    UPROPERTY() TObjectPtr<USoundClass> EffectsClass;
};
