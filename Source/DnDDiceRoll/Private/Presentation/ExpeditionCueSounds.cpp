#include "Presentation/ExpeditionCueSounds.h"
#include "Presentation/ExpeditionAudio.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"

namespace
{
/** The loaded object behind a soft reference: the one already in memory, else a quiet load (a missing asset is not an error here). */
template <typename T>
T* Resolve(const TSoftObjectPtr<T>& Reference)
{
    if (Reference.IsNull()) { return nullptr; }
    if (T* Loaded = Reference.Get()) { return Loaded; }
    return LoadObject<T>(nullptr, *Reference.ToSoftObjectPath().ToString(), nullptr, LOAD_Quiet | LOAD_NoWarn);
}
}

void FExpeditionCueBank::Reset()
{
    Cues.Reset();
    EffectsClass = nullptr;
}

void FExpeditionCueBank::Load(const UExpeditionCueSounds* Sounds)
{
    Reset();
    if (!Sounds) { return; }
    for (const TPair<EExpeditionCue, FExpeditionCueSound>& Entry : Sounds->Cues)
    {
        FExpeditionLoadedCue Loaded;
        Loaded.Volume = FMath::Clamp(Entry.Value.Volume, 0.0f, FExpeditionCueSound::MaxVolume);
        for (const TSoftObjectPtr<USoundWave>& Variation : Entry.Value.Variations)
        {
            USoundWave* Wave = Resolve(Variation);
            if (Wave && Wave->GetDuration() > 0.0f) { Loaded.Waves.Add(Wave); }
            else if (!Variation.IsNull())
            {
                UE_LOG(LogExpeditionAudio, Display, TEXT("Cue %s: %s did not load; skipping that variation"),
                    *ExpeditionAudio::CueName(Entry.Key), *Variation.ToSoftObjectPath().ToString());
            }
        }
        if (!Loaded.Waves.IsEmpty()) { Cues.Add(Entry.Key, MoveTemp(Loaded)); }
    }
    EffectsClass = Resolve(Sounds->EffectsClass);
}

int32 FExpeditionCueBank::Count(EExpeditionCue Cue) const
{
    const FExpeditionLoadedCue* Loaded = Cues.Find(Cue);
    return Loaded ? Loaded->Waves.Num() : 0;
}

FExpeditionCuePlay FExpeditionCueBank::Choose(EExpeditionCue Cue, uint32 Serial) const
{
    FExpeditionCuePlay Play;
    if (const FExpeditionLoadedCue* Loaded = Cues.Find(Cue))
    {
        Play.Variation = ExpeditionAudio::PickVariation(Cue, Serial, Loaded->Waves.Num());
        Play.Wave = Loaded->Waves[Play.Variation];
        Play.Volume = Loaded->Volume;
        if (IsValid(Play.Wave)) { return Play; }
    }
    Play.Wave = nullptr;
    Play.Variation = ExpeditionAudio::PickVariation(Cue, Serial, ExpeditionAudio::SynthVariations);
    Play.Volume = ExpeditionAudio::Volume(Cue);
    return Play;
}

TArray<EExpeditionCue> FExpeditionCueBank::SynthesizedCues() const
{
    TArray<EExpeditionCue> Result;
    for (int32 Index = 0; Index < ExpeditionCueCount; ++Index)
    {
        const EExpeditionCue Cue = static_cast<EExpeditionCue>(Index);
        if (Count(Cue) == 0) { Result.Add(Cue); }
    }
    return Result;
}
