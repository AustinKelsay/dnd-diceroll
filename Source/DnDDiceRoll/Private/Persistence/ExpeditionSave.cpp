#include "Persistence/ExpeditionSave.h"
#include "Party/BuildCatalog.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
constexpr int32 MaxSaveCharacters = 4 * 1024 * 1024;
constexpr int32 MaxRecords = 10000;

// Deliberately limited to the value types in this save contract. No permissive import,
// missing-field defaults, text-to-number coercion, enum truncation or UObject hydration.
TSharedPtr<FJsonValue> ExportValue(FProperty* Property, const void* Value);
TSharedRef<FJsonObject> ExportStruct(const UStruct* Type, const void* Value)
{
    auto Object = MakeShared<FJsonObject>();
    for (TFieldIterator<FProperty> It(Type); It; ++It)
    { Object->SetField(It->GetName(), ExportValue(*It, It->ContainerPtrToValuePtr<void>(Value))); }
    return Object;
}
TSharedPtr<FJsonValue> ExportValue(FProperty* Property, const void* Value)
{
    if (auto* P = CastField<FStructProperty>(Property)) { return MakeShared<FJsonValueObject>(ExportStruct(P->Struct, Value)); }
    if (auto* P = CastField<FArrayProperty>(Property))
    {
        FScriptArrayHelper Array(P, Value); TArray<TSharedPtr<FJsonValue>> Values;
        for (int32 I = 0; I < Array.Num(); ++I) { Values.Add(ExportValue(P->Inner, Array.GetRawPtr(I))); }
        return MakeShared<FJsonValueArray>(Values);
    }
    if (auto* P = CastField<FEnumProperty>(Property)) { return MakeShared<FJsonValueNumber>(static_cast<double>(P->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value))); }
    if (auto* P = CastField<FIntProperty>(Property)) { return MakeShared<FJsonValueNumber>(P->GetPropertyValue(Value)); }
    if (auto* P = CastField<FBoolProperty>(Property)) { return MakeShared<FJsonValueBoolean>(P->GetPropertyValue(Value)); }
    if (auto* P = CastField<FStrProperty>(Property)) { return MakeShared<FJsonValueString>(P->GetPropertyValue(Value)); }
    checkNoEntry(); return nullptr;
}
bool ImportValue(FProperty* Property, const TSharedPtr<FJsonValue>& Json, void* Value);
bool ImportStruct(const UStruct* Type, const TSharedPtr<FJsonObject>& Json, void* Value)
{
    if (!Json) { return false; }
    int32 Fields = 0;
    for (TFieldIterator<FProperty> It(Type); It; ++It)
    {
        ++Fields;
        const auto* Field = Json->Values.Find(FJsonObject::FStringType(It->GetName()));
        if (!Field || !ImportValue(*It, *Field, It->ContainerPtrToValuePtr<void>(Value))) { return false; }
    }
    return Fields == Json->Values.Num();
}
bool ImportValue(FProperty* Property, const TSharedPtr<FJsonValue>& Json, void* Value)
{
    if (!Json || Json->IsNull()) { return false; }
    if (auto* P = CastField<FStructProperty>(Property))
    { return Json->Type == EJson::Object && ImportStruct(P->Struct, Json->AsObject(), Value); }
    if (auto* P = CastField<FArrayProperty>(Property))
    {
        if (Json->Type != EJson::Array || Json->AsArray().Num() > MaxRecords) { return false; }
        FScriptArrayHelper Array(P, Value); Array.Resize(Json->AsArray().Num());
        for (int32 I = 0; I < Array.Num(); ++I) { if (!ImportValue(P->Inner, Json->AsArray()[I], Array.GetRawPtr(I))) { return false; } }
        return true;
    }
    if (auto* P = CastField<FBoolProperty>(Property))
    { if (Json->Type != EJson::Boolean) { return false; } P->SetPropertyValue(Value, Json->AsBool()); return true; }
    if (auto* P = CastField<FStrProperty>(Property))
    { if (Json->Type != EJson::String || Json->AsString().Len() > 512) { return false; } P->SetPropertyValue(Value, Json->AsString()); return true; }
    if (Json->Type != EJson::Number) { return false; }
    const double Number = Json->AsNumber();
    if (!FMath::IsFinite(Number) || Number < MIN_int32 || Number > MAX_int32 || Number != FMath::FloorToDouble(Number)) { return false; }
    if (auto* P = CastField<FEnumProperty>(Property))
    {
        const int64 Integer = static_cast<int64>(Number);
        if (Integer < 0 || Integer >= P->GetEnum()->GetMaxEnumValue() || !P->GetEnum()->IsValidEnumValue(Integer)) { return false; }
        P->GetUnderlyingProperty()->SetIntPropertyValue(Value, Integer); return true;
    }
    if (auto* P = CastField<FIntProperty>(Property)) { P->SetPropertyValue(Value, static_cast<int32>(Number)); return true; }
    return false;
}
bool SameEnding(const FExpeditionEnding& A, const FExpeditionEnding& B)
{ return FExpeditionEnding::StaticStruct()->CompareScriptStruct(&A, &B, 0); }
FString EndingSummary(EExpeditionPhase Outcome, int32 Victories, const TArray<FString>& SurvivorNames)
{
    FString Names;
    for (const auto& Name : SurvivorNames) { if (!Names.IsEmpty()) { Names += TEXT(", "); } Names += Name; }
    return FString::Printf(TEXT("%s | %d/5 victories | Survivors: %s"), Outcome == EExpeditionPhase::Victory ? TEXT("Waystone recovered") : TEXT("Party lost"), Victories, Names.IsEmpty() ? TEXT("none") : *Names);
}
}

FString FExpeditionSave::DefaultPath() { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames/Expedition/current-v1.json")); }

FExpeditionEnding FExpeditionSave::Ending(const FExpeditionRun& Run)
{
    FExpeditionEnding Record;
    Record.RunId = Run.Progress().RunId; Record.Seed = Run.Progress().Seed;
    Record.Outcome = Run.Progress().Phase; Record.Victories = Run.Progress().EncountersCompleted;
    for (const auto& Unit : Run.Battle().Snapshot().Encounter.Combatants)
    { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { Record.Survivors.Add(Unit.Id); Record.SurvivorNames.Add(Unit.Name); Record.SurvivorBuilds.Add(Unit.BuildId); } }
    Record.Summary = EndingSummary(Record.Outcome, Record.Victories, Record.SurvivorNames);
    return Record;
}

bool FExpeditionSave::Validate(const FExpeditionSaveData& Data, FString& Error)
{
    Error.Reset();
    if (Data.Version != Version) { Error = TEXT("This expedition save uses an unsupported version."); return false; }
    FExpeditionRun Candidate;
    if (!Candidate.Restore(Data.Current, Error)) { return false; }
    auto Reject = [&Error]() { Error = TEXT("Expedition ending records are invalid or incomplete."); return false; };
    if (Data.Records.Num() > MaxRecords) { return Reject(); }
    TSet<FString> Ids;
    for (const auto& Record : Data.Records)
    {
        if (Record.RunId.IsEmpty() || Record.RunId.Len() > 80 || Ids.Contains(Record.RunId) ||
            (Record.Outcome != EExpeditionPhase::Victory && Record.Outcome != EExpeditionPhase::Defeat) ||
            Record.Victories < 0 || Record.Victories > 5 ||
            (Record.Outcome == EExpeditionPhase::Victory ? Record.Victories != 5 || Record.Survivors.IsEmpty() : Record.Victories > 4 || !Record.Survivors.IsEmpty())) { return Reject(); }
        if (Record.SurvivorNames.Num() != Record.Survivors.Num() || Record.SurvivorBuilds.Num() != Record.Survivors.Num()) { return Reject(); }
        for (int32 I = 0; I < Record.Survivors.Num(); ++I)
        { if (Record.SurvivorNames[I].IsEmpty() || Record.SurvivorNames[I].Len() > 80 || !FBuildCatalog::Find(Record.SurvivorBuilds[I])) { return Reject(); } }
        TSet<int32> Survivors;
        for (int32 Id : Record.Survivors) { if (Id < 1 || Id > 4 || Survivors.Contains(Id)) { return Reject(); } Survivors.Add(Id); }
        if (Record.Summary != EndingSummary(Record.Outcome, Record.Victories, Record.SurvivorNames)) { return Reject(); }
        Ids.Add(Record.RunId);
    }
    const auto* Existing = Data.Records.FindByPredicate([&](const auto& Record) { return Record.RunId == Data.Current.Progress.RunId; });
    const bool bEnded = Data.Current.Progress.Phase == EExpeditionPhase::Victory || Data.Current.Progress.Phase == EExpeditionPhase::Defeat;
    if (bEnded ? !Existing || !SameEnding(*Existing, Ending(Candidate)) : Existing != nullptr) { return Reject(); }
    return true;
}

bool FExpeditionSave::Encode(const FExpeditionSaveData& Data, FString& Json, FString& Error)
{
    if (!Validate(Data, Error)) { return false; }
    FString Candidate;
    auto Writer = TJsonWriterFactory<>::Create(&Candidate);
    if (!FJsonSerializer::Serialize(ExportStruct(FExpeditionSaveData::StaticStruct(), &Data), Writer) || Candidate.Len() > MaxSaveCharacters)
    { Error = TEXT("Expedition save is too large or could not be encoded."); return false; }
    Json = MoveTemp(Candidate); return true;
}

bool FExpeditionSave::Decode(const FString& Json, FExpeditionSaveData& Data, FString& Error)
{
    TSharedPtr<FJsonObject> Object;
    FExpeditionSaveData Candidate;
    if (Json.IsEmpty() || Json.Len() > MaxSaveCharacters || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) ||
        !ImportStruct(FExpeditionSaveData::StaticStruct(), Object, &Candidate))
    { Error = TEXT("The expedition save is incomplete or unreadable."); return false; }
    if (!Validate(Candidate, Error)) { return false; }
    Data = MoveTemp(Candidate); Error.Reset(); return true;
}

bool FExpeditionSave::Write(const FString& Path, const FExpeditionSaveData& Data, FString& Error)
{
    FString Json;
    if (!Encode(Data, Json, Error)) { return false; }
    auto& Files = FPlatformFileManager::Get().GetPlatformFile();
    const FString Temporary = Path + TEXT(".pending");
    if (!Files.CreateDirectoryTree(*FPaths::GetPath(Path)) ||
        !FFileHelper::SaveStringToFile(Json, *Temporary, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Silent))
    { Error = TEXT("Could not save expedition. Your last checkpoint is retained; this action was not applied."); return false; }
    bool bCommitted = false;
    bool bRestoreFailed = false;
#if PLATFORM_WINDOWS
    // One atomic same-volume replace: the old checkpoint is never missing (#83).
    bCommitted = ::MoveFileExW(*FPaths::ConvertRelativePathToFull(Temporary), *FPaths::ConvertRelativePathToFull(Path),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    // Apple/POSIX MoveFile is a same-directory rename, replacing atomically.
    // Do not use IFileManager::Move, whose replacement can delete the destination first.
    bCommitted = Files.MoveFile(*Path, *Temporary);
#if !PLATFORM_APPLE
    // Platforms whose MoveFile does not replace: keep the old checkpoint aside, install the new one, restore on failure.
    if (!bCommitted && Files.FileExists(*Path))
    {
        const FString Previous = Path + TEXT(".previous");
        Files.DeleteFile(*Previous);
        if (Files.MoveFile(*Previous, *Path))
        {
            bCommitted = Files.MoveFile(*Path, *Temporary);
            if (bCommitted) { Files.DeleteFile(*Previous); }
            else { bRestoreFailed = !Files.MoveFile(*Path, *Previous); }
        }
    }
#endif
#endif
    if (!bCommitted)
    {
        Files.DeleteFile(*Temporary);
        if (bRestoreFailed)
        { Error = FString::Printf(TEXT("Could not commit expedition, and the previous checkpoint could not be restored. It remains at %s.previous."), *Path); return false; }
        Error = TEXT("Could not commit expedition. Your last checkpoint is retained; this action was not applied."); return false;
    }
    Error.Reset(); return true;
}

bool FExpeditionSave::Read(const FString& Path, FExpeditionSaveData& Data, FString& Error)
{
    FString Json;
    const int64 Size = FPlatformFileManager::Get().GetPlatformFile().FileSize(*Path);
    if (Size < 1 || Size > MaxSaveCharacters * 4 || !FFileHelper::LoadFileToString(Json, *Path))
    { Error = TEXT("No readable expedition checkpoint was found."); return false; }
    return Decode(Json, Data, Error);
}

bool FExpeditionSession::Commit(FExpeditionRun&& Candidate, FString& Error)
{
    FExpeditionSaveData Data;
    if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*SavePath))
    {
        if (!FExpeditionSave::Read(SavePath, Data, Error)) { return false; }
        // Detect a different/stale live session instead of overwriting its progress.
        if (bHasRun)
        {
            const auto Live = Current.Snapshot();
            if (!FExpeditionSnapshot::StaticStruct()->CompareScriptStruct(&Live, &Data.Current, 0))
            { Error = TEXT("The checkpoint has changed. Resume it before continuing."); return false; }
        }
    }
    else if (bHasRun) { Error = TEXT("The checkpoint is missing. This action was not applied."); return false; }
    Data.Current = Candidate.Snapshot();
    if (Candidate.Progress().Phase == EExpeditionPhase::Victory || Candidate.Progress().Phase == EExpeditionPhase::Defeat)
    {
        const auto Record = FExpeditionSave::Ending(Candidate);
        const auto* Existing = Data.Records.FindByPredicate([&](const auto& R) { return R.RunId == Record.RunId; });
        if (!Existing) { Data.Records.Add(Record); }
        else if (!SameEnding(*Existing, Record)) { Error = TEXT("This ending conflicts with its recorded expedition."); return false; }
    }
    if (!FExpeditionSave::Write(SavePath, Data, Error)) { return false; }
    Current = MoveTemp(Candidate); bHasRun = true; return true;
}

bool FExpeditionSession::NewRun(int32 Seed, FString& Error, const FString& Identity, FString* PreservedCheckpoint, EFighterSubclass Subclass)
{
    FExpeditionRun Selected;
    if (!Selected.NewRun(Seed, Error, Identity, Subclass)) { return false; }
    return NewRun(Seed, Selected.Progress().Party, Error, Selected.Progress().RunId, PreservedCheckpoint);
}

bool FExpeditionSession::NewRun(int32 Seed, const TArray<FPartyBuildSelection>& Party, FString& Error, const FString& Identity, FString* PreservedCheckpoint)
{
    if (PreservedCheckpoint) { PreservedCheckpoint->Reset(); }
    auto IsEnding = [](EExpeditionPhase Phase)
    { return Phase == EExpeditionPhase::Victory || Phase == EExpeditionPhase::Defeat; };
    if (bHasRun && !IsEnding(Current.Progress().Phase))
    { Error = TEXT("Finish this expedition before starting a new run."); return false; }
    FExpeditionRun Candidate;
    if (!Candidate.NewRun(Seed, Party, Error, Identity)) { return false; }
    FString Preserved;
    auto& Files = FPlatformFileManager::Get().GetPlatformFile();
    if (!bHasRun && Files.FileExists(*SavePath))
    {
        FExpeditionSaveData Existing;
        if (FExpeditionSave::Read(SavePath, Existing, Error))
        {
            if (!IsEnding(Existing.Current.Progress.Phase))
            { Error = TEXT("A saved expedition is unfinished. Press O to resume it."); return false; }
        }
        else
        {
            Preserved = SavePath + TEXT(".unreadable-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
            if (Files.FileExists(*Preserved) || !Files.MoveFile(*Preserved, *SavePath))
            { Error = TEXT("The saved expedition is unreadable and could not be preserved. No new run was started."); return false; }
            if (PreservedCheckpoint) { *PreservedCheckpoint = Preserved; }
        }
    }
    if (Commit(MoveTemp(Candidate), Error)) { return true; }
    if (!Preserved.IsEmpty()) { Error += TEXT(" The unreadable original remains preserved beside the checkpoint."); }
    return false;
}
bool FExpeditionSession::ChooseRoute(int32 Id, FString& Error)
{
    if (!bHasRun) { Error = TEXT("Start or resume an expedition first."); return false; }
    auto Candidate = Current; return Candidate.ChooseRoute(Id, Error) && Commit(MoveTemp(Candidate), Error);
}
bool FExpeditionSession::ClaimReward(int32 Id, FString& Error)
{
    if (!bHasRun) { Error = TEXT("Start or resume an expedition first."); return false; }
    auto Candidate = Current; return Candidate.ClaimReward(Id, Error) && Commit(MoveTemp(Candidate), Error);
}
bool FExpeditionSession::Recover(bool bTake, FString& Error)
{
    if (!bHasRun) { Error = TEXT("Start or resume an expedition first."); return false; }
    auto Candidate = Current; return Candidate.Recover(bTake, Error) && Commit(MoveTemp(Candidate), Error);
}
FCombatResult FExpeditionSession::Submit(const FCombatCommand& Command)
{
    if (!bHasRun) { return {false, TEXT("Start or resume an expedition first.")}; }
    auto Candidate = Current; auto Result = Candidate.Submit(Command);
    FString Error;
    if (Result.bAccepted && !Commit(MoveTemp(Candidate), Error)) { return {false, Error}; }
    return Result;
}
TArray<FCombatResult> FExpeditionSession::PlayEnemyTurn()
{
    if (!bHasRun) { return {}; }
    auto Candidate = Current; auto Results = Candidate.PlayEnemyTurn();
    FString Error;
    if (!Results.IsEmpty() && !Commit(MoveTemp(Candidate), Error)) { return {{false, Error}}; }
    return Results;
}
bool FExpeditionSession::Save(FString& Error)
{
    if (!bHasRun) { Error = TEXT("Start or resume an expedition first."); return false; }
    auto Candidate = Current; return Commit(MoveTemp(Candidate), Error);
}
bool FExpeditionSession::Resume(FString& Error)
{
    FExpeditionSaveData Data; FExpeditionRun Candidate;
    if (!FExpeditionSave::Read(SavePath, Data, Error) || !Candidate.Restore(Data.Current, Error)) { return false; }
    Current = MoveTemp(Candidate); bHasRun = true; return true;
}
bool FExpeditionSession::ReadRecords(TArray<FExpeditionEnding>& Records, FString& Error) const
{
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*SavePath)) { Records.Reset(); Error.Reset(); return true; }
    FExpeditionSaveData Data;
    if (!FExpeditionSave::Read(SavePath, Data, Error)) { return false; }
    Records = MoveTemp(Data.Records); return true;
}
