#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Presentation/AudioCrossfade.h"
#include "ExpeditionMusic.generated.h"

class USoundClass;
class USoundWave;
enum class EExpeditionPhase : uint8;

/** The looping music tracks (#245). None is silence (a track with no recording). */
UENUM(BlueprintType)
enum class EExpeditionMusic : uint8
{
    None,
    /** The war table and the start screen: between fights, the reward and the camp, and after a run ends. */
    Table,
    /** The fights, alternating by stage: odd encounters play A, even ones B. */
    CombatA,
    CombatB,
    /** The Vault finale. */
    Finale
};

constexpr int32 ExpeditionMusicCount = static_cast<int32>(EExpeditionMusic::Finale) + 1;

/** The one-shot stings at the end of a run (#245). */
UENUM(BlueprintType)
enum class EExpeditionSting : uint8
{
    Victory,
    Defeat
};

namespace ExpeditionMusic
{
/** The music data asset the battle view loads. */
constexpr const TCHAR* SoundsPath = TEXT("/Game/DnDDiceRoll/Audio/DA_ExpeditionMusic.DA_ExpeditionMusic");
/** How long a track takes to fade in while the previous one fades out. */
constexpr float CrossfadeSeconds = 2.0f;
/** At a run's end the playing track fades out this fast under the sting. */
constexpr float StingFadeSeconds = 1.0f;
/** The table theme starts fading in this long before the sting ends, so the music never stops. */
constexpr float StingOverlapSeconds = 1.0f;

/**
 * What the music plays for a phase the board shows (pure). No run in play, the route, the reward, the camp and a finished run play the
 * table theme; a fight plays CombatA on odd encounters and CombatB on even ones, and the Vault finale plays Finale. Encounter is the
 * 1-based number of the fight (History.Num()).
 */
DNDDICEROLL_API EExpeditionMusic TrackFor(bool bRunInPlay, EExpeditionPhase Phase, int32 Encounter, bool bFinale);
/** The sting a run's end plays: Victory or Defeat when the phase is one of them, else none. */
DNDDICEROLL_API TOptional<EExpeditionSting> StingFor(bool bRunInPlay, EExpeditionPhase Phase);
DNDDICEROLL_API FString Name(EExpeditionMusic Track);
DNDDICEROLL_API FString StingName(EExpeditionSting Sting);
}

/** What the board shows this frame, as far as the music cares. */
struct FExpeditionMusicMoment
{
    bool bRunInPlay = false;
    EExpeditionPhase Phase{};
    /** The 1-based number of the current or last fight (History.Num()). */
    int32 Encounter = 0;
    /** The current or last fight is the Vault finale. */
    bool bFinale = false;
    /** Playback is still showing committed results (a fight's last blows, a banner): the music waits for it. */
    bool bPresenting = false;
};

/**
 * The music director (#245): which track plays and when a run-end sting starts, as plain state with no audio objects (tests drive it
 * through any phase sequence). Each Update, unless playback is still showing the last committed results, it selects TrackFor the moment
 * through an FAudioCrossfade (2 s, equal power, at most two layers). When the run ends it starts the sting once: the track fades out under
 * it over 1 s and the table theme fades in for the sting's last second, so the music never stops for more than a fade. A track with no
 * recording is silence; a missing sting has no length, so the table theme follows at once. Leaving the ending for a new run cancels a
 * sting that is still sounding (the view stops its voice) and the new phase's track crossfades in at once. Selecting the current track changes nothing,
 * so a Resume into the same track keeps playing it. Presentation only (ADR-0001).
 */
class DNDDICEROLL_API FExpeditionMusicDirector
{
public:
    /** The stings' lengths in seconds (0 when a sting has no recording). */
    void SetStingLengths(float Victory, float Defeat) { StingLengths[0] = FMath::Max(Victory, 0.0f); StingLengths[1] = FMath::Max(Defeat, 0.0f); }
    /** Follows the moment and advances the fades; returns the sting to start this frame, if one starts. */
    TOptional<EExpeditionSting> Update(const FExpeditionMusicMoment& Now, float DeltaSeconds);
    void Reset();
    const FAudioCrossfade& Fade() const { return Crossfade; }
    EExpeditionMusic Current() const { return static_cast<EExpeditionMusic>(Crossfade.Current()); }
    /** The sting still sounding, and for how long. */
    TOptional<EExpeditionSting> Sting() const { return StingLeft > 0.0f ? Playing : TOptional<EExpeditionSting>(); }
    float StingRemaining() const { return StingLeft; }

private:
    FAudioCrossfade Crossfade;
    TOptional<EExpeditionSting> Playing;
    float StingLeft = 0.0f;
    float StingLengths[2] = {0.0f, 0.0f};
    /** The run's end has had its sting; cleared when a run is in play again. */
    bool bEnded = false;
};

/** One recording and its level (#245). */
USTRUCT(BlueprintType)
struct DNDDICEROLL_API FExpeditionMusicPiece
{
    GENERATED_BODY()
    /** A seamless loop for a track (the importer sets it looping); a one-shot for a sting. */
    UPROPERTY(EditAnywhere, Category = "Audio") TSoftObjectPtr<USoundWave> Wave;
    /** Multiplies the imported loudness (the pieces are already normalised; 1 plays them as measured). */
    UPROPERTY(EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.0")) float Volume = 1.0f;
    static constexpr float MaxVolume = 2.0f;
};

/**
 * The score (#245), created by Content/Python/import_expedition_music.py at /Game/DnDDiceRoll/Audio/DA_ExpeditionMusic. Optional as a whole
 * and per piece: a track or sting with no loadable recording is silence. Music is never synthesized.
 */
UCLASS(BlueprintType)
class DNDDICEROLL_API UExpeditionMusicSounds : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Audio") TMap<EExpeditionMusic, FExpeditionMusicPiece> Tracks;
    UPROPERTY(EditAnywhere, Category = "Audio") TMap<EExpeditionSting, FExpeditionMusicPiece> Stings;
    /** The Music class every track and sting plays through (the mix of #246 sets its volume). Optional. */
    UPROPERTY(EditAnywhere, Category = "Audio") TSoftObjectPtr<USoundClass> MusicClass;
};

/** The loaded score. A piece that is unset or fails to load is absent, and plays as silence. */
USTRUCT()
struct DNDDICEROLL_API FExpeditionMusicBank
{
    GENERATED_BODY()

    void Load(const UExpeditionMusicSounds* Sounds);
    void Reset();
    USoundWave* Track(EExpeditionMusic Key) const;
    float TrackVolume(EExpeditionMusic Key) const;
    USoundWave* Sting(EExpeditionSting Key) const;
    float StingVolume(EExpeditionSting Key) const;
    USoundClass* Class() const { return MusicClass; }
    /** The tracks and stings that have a recording, by name, for the load summary and tests. */
    TArray<FString> RecordedPieces() const;

private:
    UPROPERTY() TMap<EExpeditionMusic, TObjectPtr<USoundWave>> Tracks;
    UPROPERTY() TMap<EExpeditionSting, TObjectPtr<USoundWave>> Stings;
    TMap<EExpeditionMusic, float> TrackVolumes;
    TMap<EExpeditionSting, float> StingVolumes;
    UPROPERTY() TObjectPtr<USoundClass> MusicClass;
};
