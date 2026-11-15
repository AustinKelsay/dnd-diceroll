#include "Presentation/ExpeditionMusic.h"
#include "Presentation/ExpeditionAudio.h"
#include "Run/ExpeditionRun.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"

namespace ExpeditionMusic
{
EExpeditionMusic TrackFor(bool bRunInPlay, EExpeditionPhase Phase, int32 Encounter, bool bFinale)
{
    if (!bRunInPlay || Phase != EExpeditionPhase::Combat) { return EExpeditionMusic::Table; }
    if (bFinale) { return EExpeditionMusic::Finale; }
    return Encounter % 2 == 0 ? EExpeditionMusic::CombatB : EExpeditionMusic::CombatA;
}

TOptional<EExpeditionSting> StingFor(bool bRunInPlay, EExpeditionPhase Phase)
{
    if (!bRunInPlay) { return {}; }
    if (Phase == EExpeditionPhase::Victory) { return EExpeditionSting::Victory; }
    if (Phase == EExpeditionPhase::Defeat) { return EExpeditionSting::Defeat; }
    return {};
}

FString Name(EExpeditionMusic Track)
{
    return StaticEnum<EExpeditionMusic>()->GetNameStringByValue(static_cast<int64>(Track));
}

FString StingName(EExpeditionSting Sting)
{
    return StaticEnum<EExpeditionSting>()->GetNameStringByValue(static_cast<int64>(Sting));
}
}

TOptional<EExpeditionSting> FExpeditionMusicDirector::Update(const FExpeditionMusicMoment& Now, float DeltaSeconds)
{
    StingLeft = FMath::Max(StingLeft - FMath::Max(DeltaSeconds, 0.0f), 0.0f);
    TOptional<EExpeditionSting> Started;
    if (!Now.bRunInPlay || !Now.bPresenting)
    {
        const TOptional<EExpeditionSting> End = ExpeditionMusic::StingFor(Now.bRunInPlay, Now.Phase);
        if (End && !bEnded)
        {
            bEnded = true;
            Playing = End;
            StingLeft = StingLengths[*End == EExpeditionSting::Victory ? 0 : 1];
            Started = End;
            if (StingLeft > 0.0f) { Crossfade.Select(0, ExpeditionMusic::StingFadeSeconds); }
        }
        if (!End) { bEnded = false; }
        if (StingLeft <= ExpeditionMusic::StingOverlapSeconds)
        {
            Crossfade.Select(static_cast<uint8>(ExpeditionMusic::TrackFor(Now.bRunInPlay, Now.Phase, Now.Encounter, Now.bFinale)), ExpeditionMusic::CrossfadeSeconds);
        }
    }
    Crossfade.Tick(DeltaSeconds);
    return Started;
}

void FExpeditionMusicDirector::Reset()
{
    Crossfade.Reset();
    Playing.Reset();
    StingLeft = 0.0f;
    bEnded = false;
}

namespace
{
template <typename T>
T* ResolveMusicPiece(const TSoftObjectPtr<T>& Reference)
{
    if (Reference.IsNull()) { return nullptr; }
    if (T* Loaded = Reference.Get()) { return Loaded; }
    return LoadObject<T>(nullptr, *Reference.ToSoftObjectPath().ToString(), nullptr, LOAD_Quiet | LOAD_NoWarn);
}

template <typename K>
void LoadMusicPieces(const TMap<K, FExpeditionMusicPiece>& From, TMap<K, TObjectPtr<USoundWave>>& Waves, TMap<K, float>& Volumes, TFunctionRef<FString(K)> Describe)
{
    for (const TPair<K, FExpeditionMusicPiece>& Entry : From)
    {
        USoundWave* Wave = ResolveMusicPiece(Entry.Value.Wave);
        if (Wave && Wave->GetDuration() > 0.0f)
        {
            Waves.Add(Entry.Key, Wave);
            Volumes.Add(Entry.Key, FMath::Clamp(Entry.Value.Volume, 0.0f, FExpeditionMusicPiece::MaxVolume));
        }
        else if (!Entry.Value.Wave.IsNull())
        {
            UE_LOG(LogExpeditionAudio, Display, TEXT("Music %s: %s did not load; it is silent"), *Describe(Entry.Key), *Entry.Value.Wave.ToSoftObjectPath().ToString());
        }
    }
}
}

void FExpeditionMusicBank::Reset()
{
    Tracks.Reset(); Stings.Reset(); TrackVolumes.Reset(); StingVolumes.Reset();
    MusicClass = nullptr;
}

void FExpeditionMusicBank::Load(const UExpeditionMusicSounds* Sounds)
{
    Reset();
    if (!Sounds) { return; }
    TMap<EExpeditionMusic, FExpeditionMusicPiece> Playable = Sounds->Tracks;
    Playable.Remove(EExpeditionMusic::None);
    LoadMusicPieces<EExpeditionMusic>(Playable, Tracks, TrackVolumes, [](EExpeditionMusic Key) { return ExpeditionMusic::Name(Key); });
    LoadMusicPieces<EExpeditionSting>(Sounds->Stings, Stings, StingVolumes, [](EExpeditionSting Key) { return ExpeditionMusic::StingName(Key) + TEXT(" sting"); });
    MusicClass = ResolveMusicPiece(Sounds->MusicClass);
}

USoundWave* FExpeditionMusicBank::Track(EExpeditionMusic Key) const
{
    const TObjectPtr<USoundWave>* Found = Tracks.Find(Key);
    return Found && IsValid(*Found) ? Found->Get() : nullptr;
}

float FExpeditionMusicBank::TrackVolume(EExpeditionMusic Key) const
{
    const float* Found = TrackVolumes.Find(Key);
    return Found ? *Found : 0.0f;
}

USoundWave* FExpeditionMusicBank::Sting(EExpeditionSting Key) const
{
    const TObjectPtr<USoundWave>* Found = Stings.Find(Key);
    return Found && IsValid(*Found) ? Found->Get() : nullptr;
}

float FExpeditionMusicBank::StingVolume(EExpeditionSting Key) const
{
    const float* Found = StingVolumes.Find(Key);
    return Found ? *Found : 0.0f;
}

TArray<FString> FExpeditionMusicBank::RecordedPieces() const
{
    TArray<FString> Result;
    for (int32 Index = 1; Index < ExpeditionMusicCount; ++Index)
    {
        if (Track(static_cast<EExpeditionMusic>(Index))) { Result.Add(ExpeditionMusic::Name(static_cast<EExpeditionMusic>(Index))); }
    }
    for (EExpeditionSting Key : {EExpeditionSting::Victory, EExpeditionSting::Defeat})
    {
        if (Sting(Key)) { Result.Add(ExpeditionMusic::StingName(Key) + TEXT(" sting")); }
    }
    return Result;
}
