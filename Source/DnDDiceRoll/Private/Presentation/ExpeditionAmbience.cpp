#include "Presentation/ExpeditionAmbience.h"
#include "Presentation/DiceRollTheme.h"
#include "Presentation/ExpeditionAudio.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"

namespace ExpeditionAmbience
{
EExpeditionAmbience BedFor(bool bRunInPlay, bool bOverview, EDiceRollKit Kit)
{
    if (!bRunInPlay) { return EExpeditionAmbience::None; }
    if (bOverview) { return EExpeditionAmbience::WarTable; }
    switch (Kit)
    {
    case EDiceRollKit::Root: return EExpeditionAmbience::Root;
    case EDiceRollKit::Vault: return EExpeditionAmbience::Vault;
    default: return EExpeditionAmbience::Vigil;
    }
}

FString Name(EExpeditionAmbience Bed)
{
    return StaticEnum<EExpeditionAmbience>()->GetNameStringByValue(static_cast<int64>(Bed));
}
}

namespace
{
template <typename T>
T* Resolve(const TSoftObjectPtr<T>& Reference)
{
    if (Reference.IsNull()) { return nullptr; }
    if (T* Loaded = Reference.Get()) { return Loaded; }
    return LoadObject<T>(nullptr, *Reference.ToSoftObjectPath().ToString(), nullptr, LOAD_Quiet | LOAD_NoWarn);
}
}

void FExpeditionAmbienceBank::Reset()
{
    Waves.Reset();
    Volumes.Reset();
    AmbienceClass = nullptr;
}

void FExpeditionAmbienceBank::Load(const UExpeditionAmbienceSounds* Sounds)
{
    Reset();
    if (!Sounds) { return; }
    for (const TPair<EExpeditionAmbience, FExpeditionAmbienceBed>& Entry : Sounds->Beds)
    {
        if (Entry.Key == EExpeditionAmbience::None) { continue; }
        USoundWave* Wave = Resolve(Entry.Value.Loop);
        if (Wave && Wave->GetDuration() > 0.0f)
        {
            Waves.Add(Entry.Key, Wave);
            Volumes.Add(Entry.Key, FMath::Clamp(Entry.Value.Volume, 0.0f, FExpeditionAmbienceBed::MaxVolume));
        }
        else if (!Entry.Value.Loop.IsNull())
        {
            UE_LOG(LogExpeditionAudio, Display, TEXT("Ambience %s: %s did not load; that bed is silent"),
                *ExpeditionAmbience::Name(Entry.Key), *Entry.Value.Loop.ToSoftObjectPath().ToString());
        }
    }
    AmbienceClass = Resolve(Sounds->AmbienceClass);
}

USoundWave* FExpeditionAmbienceBank::Wave(EExpeditionAmbience Bed) const
{
    const TObjectPtr<USoundWave>* Found = Waves.Find(Bed);
    return Found && IsValid(*Found) ? Found->Get() : nullptr;
}

float FExpeditionAmbienceBank::Volume(EExpeditionAmbience Bed) const
{
    const float* Found = Volumes.Find(Bed);
    return Found ? *Found : 0.0f;
}

TArray<EExpeditionAmbience> FExpeditionAmbienceBank::RecordedBeds() const
{
    TArray<EExpeditionAmbience> Result;
    for (int32 Index = 1; Index < ExpeditionAmbienceCount; ++Index)
    {
        const EExpeditionAmbience Bed = static_cast<EExpeditionAmbience>(Index);
        if (Wave(Bed)) { Result.Add(Bed); }
    }
    return Result;
}
