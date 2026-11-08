#include "Presentation/ExpeditionCapture.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/ExpeditionPlayerController.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "Presentation/ExpeditionSpike.h"
#include "RHI.h"
#include "RenderTimer.h"
#include "HAL/PlatformTime.h"

namespace
{
constexpr int32 IdleTimeoutFrames = 1800;
constexpr int32 SaveTimeoutFrames = 1800;

bool ValidName(const FString& Name)
{
    if (Name.IsEmpty() || Name.Len() > 64) { return false; }
    for (const TCHAR Character : Name)
    { if (!FChar::IsAlnum(Character) && Character != TEXT('-') && Character != TEXT('_')) { return false; } }
    return true;
}

// TryGetNumberField also accepts numeric strings; poses must hold real numbers.
bool Number(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, double& Value)
{
    const TSharedPtr<FJsonValue> Entry = Object->TryGetField(Field);
    return Entry.IsValid() && Entry->Type == EJson::Number && Entry->TryGetNumber(Value);
}

TSharedRef<FJsonObject> Vector(const FVector& Value, FString A, FString B, FString C)
{
    TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(A, Value.X); Object->SetNumberField(B, Value.Y); Object->SetNumberField(C, Value.Z);
    return Object;
}
}

const TCHAR* ExpeditionCapture::ModeName(ECaptureMode Mode)
{
    switch (Mode)
    {
    case ECaptureMode::Battle: return TEXT("battle");
    case ECaptureMode::Targeting: return TEXT("targeting");
    case ECaptureMode::Overview: return TEXT("overview");
    default: return TEXT("start");
    }
}

bool ExpeditionCapture::ParsePoses(const FString& Json, FCapturePoses& Out, FString& Error)
{
    Out = FCapturePoses();
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()) { Error = TEXT("Poses file is not a JSON object"); return false; }
    double Version = 0.0;
    if (!Number(Root, TEXT("version"), Version) || Version != 1.0) { Error = TEXT("Poses file needs \"version\": 1"); return false; }
    if (Root->HasField(TEXT("settleFrames")))
    {
        double Settle = 0.0;
        if (!Number(Root, TEXT("settleFrames"), Settle) || Settle != FMath::FloorToDouble(Settle) || Settle < 1.0 || Settle > MaxSettleFrames)
        { Error = FString::Printf(TEXT("\"settleFrames\" must be a whole number from 1 to %d"), MaxSettleFrames); return false; }
        Out.SettleFrames = static_cast<int32>(Settle);
    }
    const TArray<TSharedPtr<FJsonValue>>* Shots = nullptr;
    if (!Root->TryGetArrayField(TEXT("shots"), Shots) || Shots->IsEmpty()) { Error = TEXT("Poses file needs a non-empty \"shots\" array"); return false; }
    TSet<FString> Names;
    for (int32 Index = 0; Index < Shots->Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Entry = nullptr;
        const FString Where = FString::Printf(TEXT("Shot %d"), Index);
        if (!(*Shots)[Index].IsValid() || !(*Shots)[Index]->TryGetObject(Entry)) { Error = Where + TEXT(" is not an object"); return false; }
        FCaptureShot Shot;
        if (!(*Entry)->TryGetStringField(TEXT("name"), Shot.Name) || !ValidName(Shot.Name))
        { Error = Where + TEXT(" needs a \"name\" of letters, digits, '-' and '_' (up to 64)"); return false; }
        if (Names.Contains(Shot.Name)) { Error = FString::Printf(TEXT("Shot name \"%s\" is used twice"), *Shot.Name); return false; }
        Names.Add(Shot.Name);
        FString Mode;
        if (!(*Entry)->TryGetStringField(TEXT("mode"), Mode)) { Error = Shot.Name + TEXT(" needs a \"mode\""); return false; }
        if (Mode == TEXT("battle")) { Shot.Mode = ECaptureMode::Battle; }
        else if (Mode == TEXT("targeting")) { Shot.Mode = ECaptureMode::Targeting; }
        else if (Mode == TEXT("overview")) { Shot.Mode = ECaptureMode::Overview; }
        else if (Mode == TEXT("start")) { Shot.Mode = ECaptureMode::Start; }
        else { Error = FString::Printf(TEXT("%s has unknown mode \"%s\" (battle, targeting, overview, start)"), *Shot.Name, *Mode); return false; }
        if (Shot.Mode == ECaptureMode::Battle || Shot.Mode == ECaptureMode::Targeting)
        {
            Shot.bHasCamera = true;
            double Yaw = 0.0;
            if (!Number(*Entry, TEXT("yaw"), Yaw) || Yaw < -360.0 || Yaw > 360.0) { Error = Shot.Name + TEXT(" needs a \"yaw\" from -360 to 360"); return false; }
            Shot.Yaw = static_cast<float>(Yaw);
            const TSharedPtr<FJsonValue> Distance = (*Entry)->TryGetField(TEXT("distance"));
            FString Word;
            double Number = 0.0;
            if (Distance.IsValid() && Distance->TryGetString(Word) && Word == TEXT("default")) { Shot.Distance = 0.0f; }
            else if (Distance.IsValid() && Distance->Type == EJson::Number && Distance->TryGetNumber(Number) && Number >= MinDistance && Number <= MaxDistance) { Shot.Distance = static_cast<float>(Number); }
            else { Error = FString::Printf(TEXT("%s needs a \"distance\" of \"default\" or a number from %.0f to %.0f"), *Shot.Name, MinDistance, MaxDistance); return false; }
        }
        Out.Shots.Add(Shot);
    }
    return true;
}

TSharedPtr<FExpeditionCaptureDirector> FExpeditionCaptureDirector::TryCreate(AExpeditionPlayerController* Controller, AExpeditionBattleView* View)
{
    FString PosesPath;
    if (!FParse::Value(FCommandLine::Get(), TEXT("ExpeditionCapture="), PosesPath)) { return nullptr; }
    TSharedPtr<FExpeditionCaptureDirector> Director = MakeShared<FExpeditionCaptureDirector>();
    Director->Controller = Controller;
    Director->View = View;
    Director->PosesPath = PosesPath;
    if (!FParse::Value(FCommandLine::Get(), TEXT("ExpeditionCaptureOut="), Director->OutputDir) || Director->OutputDir.IsEmpty())
    { Director->OutputDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExpeditionCapture")); }
    Director->OutputDir = FPaths::ConvertRelativePathToFull(Director->OutputDir);
    FParse::Value(FCommandLine::Get(), TEXT("ExpeditionCaptureFixture="), Director->Fixture);
    View->HoldEnemyTurns(true);
    FString Text;
    FString Error;
    if (!FPaths::IsRelative(PosesPath) && FFileHelper::LoadFileToString(Text, *PosesPath)) { ExpeditionCapture::ParsePoses(Text, Director->Poses, Error); }
    else { Error = FString::Printf(TEXT("Cannot read the poses file \"%s\" (it must be an absolute path)"), *PosesPath); }
    if (!Error.IsEmpty()) { Director->Fail(Error); }
    else
    {
        // Stable order, but leaving the run for the start screen must come last.
        Director->Poses.Shots.StableSort([](const FCaptureShot& A, const FCaptureShot& B) { return (A.Mode != ECaptureMode::Start) && (B.Mode == ECaptureMode::Start); });
        IFileManager::Get().MakeDirectory(*Director->OutputDir, true);
    }
    return Director;
}

bool FExpeditionCaptureDirector::Idle() const
{
    const AExpeditionBattleView* Board = View.Get();
    if (!Board || Board->IsPresenting()) { return false; }
    const FExpeditionPlayback& Show = Board->Playback();
    const bool bBanner = !Show.Banner().Title.IsEmpty() && Show.Banner().Age <= Show.Banner().Life;
    return !bBanner && Show.Texts().IsEmpty() && Show.Sparkles().IsEmpty();
}

void FExpeditionCaptureDirector::Fail(const FString& Message)
{
    bFailed = true;
    Failure = Message;
    Stage = EStage::Done;
    UE_LOG(LogTemp, Error, TEXT("ExpeditionCapture: %s"), *Message);
    WriteManifest();
    FPlatformMisc::RequestExitWithStatus(false, 1);
}

void FExpeditionCaptureDirector::Finish()
{
    Stage = EStage::Done;
    WriteManifest();
    FPlatformMisc::RequestExit(false);
}

bool FExpeditionCaptureDirector::Apply(const FCaptureShot& Shot, FString& Skip)
{
    AExpeditionBattleView* Board = View.Get();
    AExpeditionPlayerController* Player = Controller.Get();
    if (!Board || !Player) { Skip = TEXT("view or controller is gone"); return false; }
    Player->PreviewAim(false);
    switch (Shot.Mode)
    {
    case ECaptureMode::Battle:
    case ECaptureMode::Targeting:
    {
        if (!Board->InCombat()) { Skip = TEXT("the checkpoint is not in an encounter"); return false; }
        Board->SnapCamera(Shot.Yaw, Shot.Distance > 0.0f ? Shot.Distance * FExpeditionSpike::Get().DistanceScale() : Board->FramedDistance());
        if (Shot.Mode == ECaptureMode::Battle) { return true; }
        const int32 ActorId = Board->Rules().ActiveId();
        const FCombatantState* Active = Board->Rules().Find(ActorId);
        if (!Active || Active->Side != ECombatSide::Party) { Skip = TEXT("an enemy holds the turn"); return false; }
        const TArray<int32> Targets = Board->Rules().LegalTargets(ActorId, ECombatCommandType::Attack);
        auto Steps = [](FIntPoint A, FIntPoint B) { return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y); };
        // Aim at the nearest enemy in reach; when none is, at the reachable step that gets closest to one.
        FIntPoint Aim = Active->Cell;
        int32 Best = MAX_int32;
        for (const int32 Id : Board->Rules().LegalTargets(ActorId, ECombatCommandType::Attack))
        {
            const FCombatantState* Unit = Board->Rules().Find(Id);
            if (Unit && Steps(Unit->Cell, Active->Cell) < Best) { Best = Steps(Unit->Cell, Active->Cell); Aim = Unit->Cell; }
        }
        if (Best == MAX_int32)
        {
            FIntPoint Nearest = Active->Cell;
            for (const FCombatantState& Unit : Board->Rules().Snapshot().Encounter.Combatants)
            {
                if (Unit.Side == ECombatSide::Enemy && Unit.Life != ECombatLife::Dead && Steps(Unit.Cell, Active->Cell) < Best) { Best = Steps(Unit.Cell, Active->Cell); Nearest = Unit.Cell; }
            }
            if (Best == MAX_int32) { Skip = TEXT("no enemy is standing"); return false; }
            int32 Closest = MAX_int32;
            for (const FIntPoint& Cell : Board->Rules().LegalMoves(ActorId))
            { if (Steps(Cell, Nearest) < Closest) { Closest = Steps(Cell, Nearest); Aim = Cell; } }
            if (Closest == MAX_int32) { Skip = TEXT("the active hero has no move or target to show"); return false; }
        }
        Player->PreviewAim(true, Aim);
        return true;
    }
    case ECaptureMode::Overview:
        if (!Board->ShowingOverview()) { Skip = TEXT("the checkpoint is not at the route table"); return false; }
        Board->SnapCameraToTargets();
        return true;
    default:
        if (Board->HasRun())
        {
            if (!Board->CanReturnToMenu()) { Skip = TEXT("the run cannot be put away"); return false; }
            Board->ReturnToMenu();
        }
        Board->SnapCameraToTargets();
        return true;
    }
}

void FExpeditionCaptureDirector::Tick()
{
    AExpeditionBattleView* Board = View.Get();
    if (Stage == EStage::Done) { return; }
    if (!Board || !Controller.IsValid()) { Fail(TEXT("The battle view disappeared")); return; }
    ++Frames; ++StageFrames;
    const double Now = FPlatformTime::Seconds();
    const double FrameMs = LastTime > 0.0 ? (Now - LastTime) * 1000.0 : 0.0;
    LastTime = Now;
    switch (Stage)
    {
    case EStage::Loading:
        if (!bResumed)
        {
            // A few frames first so the world, HUD and viewport are fully up.
            if (Frames < 10) { return; }
            Board->Resume();
            if (!Board->HasRun()) { Fail(FString::Printf(TEXT("Resume found no checkpoint: %s"), Board->Messages().IsEmpty() ? TEXT("no message") : *Board->Messages().Last())); return; }
            Board->PrepareExposureForCapture();
            Seed = Board->Progress().Seed;
            // Route checkpoints have no encounter board to measure.
            if (Board->InCombat()) { Layout = FIntPoint(Board->Rules().Snapshot().Encounter.Width, Board->Rules().Snapshot().Encounter.Height); }
            bResumed = true;
            StageFrames = 0;
            return;
        }
        if (Idle() && StageFrames >= 5) { Stage = EStage::Next; StageFrames = 0; }
        else if (StageFrames > IdleTimeoutFrames) { Fail(TEXT("Playback never went idle after the resume")); }
        return;
    case EStage::Next:
    {
        if (!Idle()) { if (StageFrames > IdleTimeoutFrames) { Fail(TEXT("Playback never went idle between shots")); } return; }
        if (!Poses.Shots.IsValidIndex(NextShot)) { Finish(); return; }
        Pending = FResult();
        Pending.Shot = Poses.Shots[NextShot];
        SumFrame = SumGpu = SumGame = 0.0; Sampled = 0;
        FString Skip;
        if (Apply(Pending.Shot, Skip)) { Pending.Status = TEXT("captured"); Stage = EStage::Settling; StageFrames = 0; }
        else { Pending.Status = TEXT("skipped"); Pending.Reason = Skip; Results.Add(Pending); ++NextShot; StageFrames = 0; }
        return;
    }
    case EStage::Settling:
        // Spike timing: skip the first 20 frames after the pose (shader/streaming warm-up), average the rest.
        if (StageFrames > 20 && FrameMs > 0.0)
        {
            SumFrame += FrameMs;
            SumGpu += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
            SumGame += FPlatformTime::ToMilliseconds(GGameThreadTime);
            ++Sampled;
        }
        if (StageFrames < Poses.SettleFrames || !Idle()) { if (StageFrames > Poses.SettleFrames + IdleTimeoutFrames) { Fail(TEXT("Playback never went idle while settling")); } return; }
        {
            Pending.File = Pending.Shot.Name + TEXT(".png");
            const FString Path = FPaths::Combine(OutputDir, Pending.File);
            IFileManager::Get().Delete(*Path, false, true, true);
            if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport) { Resolution = GEngine->GameViewport->Viewport->GetSizeXY(); }
            // Record the pose actually shown, not the requested one.
            Pending.Yaw = Board->CameraYaw();
            Pending.Distance = Board->CameraDistance();
            const FTransform Camera = Board->CameraTransform();
            Pending.Location = Camera.GetLocation();
            Pending.Rotation = Camera.Rotator();
            Pending.Fov = Board->CameraFieldOfView();
            Pending.Samples = Sampled;
            if (Sampled > 0) { Pending.FrameMs = SumFrame / Sampled; Pending.GpuMs = SumGpu / Sampled; Pending.GameMs = SumGame / Sampled; }
            FScreenshotRequest::RequestScreenshot(Path, true, false);
            Stage = EStage::Saving;
            StageFrames = 0;
        }
        return;
    case EStage::Saving:
    {
        const FString Path = FPaths::Combine(OutputDir, Pending.File);
        // The PNG is written by a worker after the request is consumed; wait for the file itself.
        if (!FScreenshotRequest::IsScreenshotRequested() && StageFrames >= 3 && IFileManager::Get().FileSize(*Path) > 0)
        {
            Results.Add(Pending); ++NextShot; Stage = EStage::Next; StageFrames = 0;
        }
        else if (StageFrames > SaveTimeoutFrames) { Fail(FString::Printf(TEXT("Screenshot %s was not written"), *Pending.File)); }
        return;
    }
    default: return;
    }
}

void FExpeditionCaptureDirector::WriteManifest() const
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("version"), 1);
    Root->SetStringField(TEXT("status"), bFailed ? TEXT("failed") : TEXT("ok"));
    if (bFailed) { Root->SetStringField(TEXT("error"), Failure); }
    Root->SetStringField(TEXT("fixture"), Fixture);
    Root->SetStringField(TEXT("poses"), PosesPath);
    Root->SetNumberField(TEXT("settleFrames"), Poses.SettleFrames);
    Root->SetBoolField(TEXT("primitives"), FParse::Param(FCommandLine::Get(), TEXT("ExpeditionPrimitives")));
    Root->SetBoolField(TEXT("themed"), View.IsValid() && View->UsesTheme());
    Root->SetStringField(TEXT("spike"), FExpeditionSpike::Get().Describe());
    Root->SetStringField(TEXT("commandLine"), FCommandLine::Get());
    TSharedRef<FJsonObject> Size = MakeShared<FJsonObject>();
    Size->SetNumberField(TEXT("width"), Resolution.X);
    Size->SetNumberField(TEXT("height"), Resolution.Y);
    Root->SetObjectField(TEXT("resolution"), Size);
    Root->SetNumberField(TEXT("seed"), Seed);
    Root->SetNumberField(TEXT("boardWidth"), Layout.X);
    Root->SetNumberField(TEXT("boardHeight"), Layout.Y);
    TArray<TSharedPtr<FJsonValue>> Shots;
    for (const FResult& Result : Results)
    {
        TSharedRef<FJsonObject> Shot = MakeShared<FJsonObject>();
        Shot->SetStringField(TEXT("name"), Result.Shot.Name);
        Shot->SetStringField(TEXT("mode"), ExpeditionCapture::ModeName(Result.Shot.Mode));
        Shot->SetStringField(TEXT("status"), Result.Status);
        if (Result.Status == TEXT("skipped")) { Shot->SetStringField(TEXT("reason"), Result.Reason); }
        else
        {
            Shot->SetStringField(TEXT("file"), Result.File);
            Shot->SetNumberField(TEXT("yaw"), Result.Yaw);
            Shot->SetNumberField(TEXT("distance"), Result.Distance);
            TSharedRef<FJsonObject> Camera = MakeShared<FJsonObject>();
            Camera->SetObjectField(TEXT("location"), Vector(Result.Location, TEXT("x"), TEXT("y"), TEXT("z")));
            Camera->SetObjectField(TEXT("rotation"), Vector(FVector(Result.Rotation.Pitch, Result.Rotation.Yaw, Result.Rotation.Roll), TEXT("pitch"), TEXT("yaw"), TEXT("roll")));
            Camera->SetNumberField(TEXT("fov"), Result.Fov);
            Shot->SetObjectField(TEXT("camera"), Camera);
            Shot->SetNumberField(TEXT("avgFrameMs"), Result.FrameMs);
            Shot->SetNumberField(TEXT("avgGpuMs"), Result.GpuMs);
            Shot->SetNumberField(TEXT("avgGameThreadMs"), Result.GameMs);
            Shot->SetNumberField(TEXT("timingSamples"), Result.Samples);
        }
        Shots.Add(MakeShared<FJsonValueObject>(Shot));
    }
    Root->SetArrayField(TEXT("shots"), Shots);
    FString Text;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
    FJsonSerializer::Serialize(Root, Writer);
    FFileHelper::SaveStringToFile(Text, *FPaths::Combine(OutputDir, TEXT("manifest.json")));
}
