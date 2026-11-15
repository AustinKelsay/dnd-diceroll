#include "Presentation/ExpeditionMix.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
const TCHAR* const BusKeys[ExpeditionBusCount] = {TEXT("master"), TEXT("effects"), TEXT("ambience"), TEXT("music")};
}

bool FExpeditionAudioSettings::Set(EExpeditionBus Bus, float Value)
{
    const float Snapped = FMath::Clamp(FMath::RoundToFloat(FMath::Clamp(Value, 0.0f, 1.0f) / Step) * Step, 0.0f, 1.0f);
    float& Slot = Volumes[static_cast<int32>(Bus)];
    if (FMath::IsNearlyEqual(Slot, Snapped, 1e-4f)) { return false; }
    Slot = Snapped;
    return true;
}

bool FExpeditionAudioSettings::operator==(const FExpeditionAudioSettings& Other) const
{
    for (int32 Index = 0; Index < ExpeditionBusCount; ++Index) { if (!FMath::IsNearlyEqual(Volumes[Index], Other.Volumes[Index], 1e-4f)) { return false; } }
    return true;
}

FString FExpeditionAudioSettings::Percent(float Value)
{
    return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(FMath::Clamp(Value, 0.0f, 1.0f) * 100.0f));
}

FString FExpeditionAudioSettings::BusName(EExpeditionBus Bus)
{
    switch (Bus)
    {
    case EExpeditionBus::Master: return TEXT("Master");
    case EExpeditionBus::Effects: return TEXT("Effects");
    case EExpeditionBus::Ambience: return TEXT("Ambience");
    default: return TEXT("Music");
    }
}

FString FExpeditionAudioSettings::ToJson() const
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("version"), 1);
    for (int32 Index = 0; Index < ExpeditionBusCount; ++Index) { Root->SetNumberField(BusKeys[Index], Volumes[Index]); }
    FString Text;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
    FJsonSerializer::Serialize(Root, Writer);
    return Text;
}

bool FExpeditionAudioSettings::FromJson(const FString& Text, FExpeditionAudioSettings& Out)
{
    Out = FExpeditionAudioSettings();
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) { return false; }
    int32 Version = 0;
    if (!Root->TryGetNumberField(TEXT("version"), Version) || Version != 1) { return false; }
    for (int32 Index = 0; Index < ExpeditionBusCount; ++Index)
    {
        double Value = 0.0;
        if (Root->TryGetNumberField(BusKeys[Index], Value) && FMath::IsFinite(Value)) { Out.Set(static_cast<EExpeditionBus>(Index), static_cast<float>(Value)); }
    }
    return true;
}

FString FExpeditionAudioSettings::DefaultPath(const TCHAR* CommandLine)
{
    FString Root;
    if (!(CommandLine && FParse::Value(CommandLine, TEXT("ExpeditionSettingsRoot="), Root) && !Root.IsEmpty()))
    {
        Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Settings"));
    }
    return FPaths::Combine(Root, TEXT("expedition-audio.json"));
}

bool FExpeditionAudioSettings::Load(const FString& Path, FExpeditionAudioSettings& Out)
{
    Out = FExpeditionAudioSettings();
    if (!IFileManager::Get().FileExists(*Path)) { return true; }
    FString Text;
    return FFileHelper::LoadFileToString(Text, *Path) && FromJson(Text, Out);
}

bool FExpeditionAudioSettings::Save(const FString& Path, const FExpeditionAudioSettings& Settings)
{
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    return FFileHelper::SaveStringToFile(Settings.ToJson(), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

namespace ExpeditionMix
{
float Amplitude(float Slider)
{
    const float Value = FMath::Clamp(Slider, 0.0f, 1.0f);
    return Value * Value;
}

float BusGain(const FExpeditionAudioSettings& Settings, EExpeditionBus Bus)
{
    const float Master = Amplitude(Settings.Get(EExpeditionBus::Master));
    return Bus == EExpeditionBus::Master ? Master : Master * Amplitude(Settings.Get(Bus));
}

bool Ducks(EExpeditionCue Cue)
{
    switch (Cue)
    {
    case EExpeditionCue::Hit: case EExpeditionCue::Critical: case EExpeditionCue::Miss: case EExpeditionCue::Swing: case EExpeditionCue::Bow:
    case EExpeditionCue::Spell: case EExpeditionCue::Heal: case EExpeditionCue::Downed: case EExpeditionCue::Death:
        return true;
    default:
        return false;
    }
}
}

void FAudioDucker::Tick(float DeltaSeconds)
{
    const float Delta = FMath::Max(DeltaSeconds, 0.0f);
    if (bAttacking)
    {
        Depth = FMath::Min(1.0f, Depth + Delta / AttackSeconds);
        if (Depth >= 1.0f) { bAttacking = false; }
        return;
    }
    if (Held > 0.0f) { Held = FMath::Max(0.0f, Held - Delta); return; }
    Depth = FMath::Max(0.0f, Depth - Delta / ReleaseSeconds);
}
