#include "Presentation/ExpeditionCapture.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/ExpeditionPlayerController.h"
#include "Presentation/CombatHints.h"
#include "Presentation/DiceRollTheme.h"
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
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#endif

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

bool ValidBuild(const FString& Id)
{
    if (Id.IsEmpty() || Id.Len() > 64) { return false; }
    for (const TCHAR Character : Id)
    { if (!(Character >= TEXT('a') && Character <= TEXT('z')) && !FChar::IsDigit(Character) && Character != TEXT('-') && Character != TEXT('.')) { return false; } }
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
    case ECaptureMode::Tooltip: return TEXT("tooltip");
    case ECaptureMode::Hint: return TEXT("hint");
    case ECaptureMode::Impact: return TEXT("impact");
    case ECaptureMode::Roll: return TEXT("roll");
    case ECaptureMode::Pose: return TEXT("pose");
    case ECaptureMode::Start: return TEXT("start");
    }
    return TEXT("start");
}

const TCHAR* ExpeditionCapture::ActionName(ECaptureAction Action)
{
    switch (Action)
    {
    case ECaptureAction::Special1: return TEXT("special1");
    case ECaptureAction::Special2: return TEXT("special2");
    case ECaptureAction::Rescue: return TEXT("rescue");
    case ECaptureAction::Potion: return TEXT("potion");
    case ECaptureAction::Attack: return TEXT("attack");
    }
    return TEXT("attack");
}

const TCHAR* ExpeditionCapture::EffectName(EImpactPreview Effect)
{
    switch (Effect)
    {
    case EImpactPreview::Spell: return TEXT("spell");
    case EImpactPreview::Fire: return TEXT("fire");
    case EImpactPreview::Missile: return TEXT("missile");
    case EImpactPreview::Arrow: return TEXT("arrow");
    case EImpactPreview::Bolt: return TEXT("bolt");
    case EImpactPreview::Hit: return TEXT("hit");
    }
    return TEXT("hit");
}

const TCHAR* ExpeditionCapture::PhaseName(EImpactMoment Moment)
{
    switch (Moment)
    {
    case EImpactMoment::Flight: return TEXT("flight");
    case EImpactMoment::Release: return TEXT("release");
    case EImpactMoment::FirstLanding: return TEXT("landing-first");
    case EImpactMoment::SecondLanding: return TEXT("landing-second");
    case EImpactMoment::Landing: return TEXT("landing");
    }
    return TEXT("landing");
}

const TCHAR* ExpeditionCapture::HintName(ECaptureHint Hint)
{
    switch (Hint)
    {
    case ECaptureHint::Resources: return TEXT("resources");
    case ECaptureHint::Rescue: return TEXT("rescue");
    case ECaptureHint::EndTurn: return TEXT("endturn");
    case ECaptureHint::Move: return TEXT("move");
    }
    return TEXT("move");
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
        else if (Mode == TEXT("start"))
        {
            Shot.Mode = ECaptureMode::Start;
            if ((*Entry)->HasField(TEXT("build")) && ((*Entry)->TryGetField(TEXT("build"))->Type != EJson::String || !(*Entry)->TryGetStringField(TEXT("build"), Shot.Build) || !ValidBuild(Shot.Build)))
            { Error = Shot.Name + TEXT(" needs a \"build\" of lowercase letters, digits, '.' and '-' (up to 64)"); return false; }
        }
        else if (Mode == TEXT("tooltip")) { Shot.Mode = ECaptureMode::Tooltip; }
        else if (Mode == TEXT("hint")) { Shot.Mode = ECaptureMode::Hint; }
        else if (Mode == TEXT("impact")) { Shot.Mode = ECaptureMode::Impact; }
        else if (Mode == TEXT("roll")) { Shot.Mode = ECaptureMode::Roll; }
        else if (Mode == TEXT("pose")) { Shot.Mode = ECaptureMode::Pose; }
        else { Error = FString::Printf(TEXT("%s has unknown mode \"%s\" (battle, targeting, overview, start, tooltip, hint, impact, roll, pose)"), *Shot.Name, *Mode); return false; }
        if (Shot.Mode == ECaptureMode::Pose)
        {
            // A hero of a class held in one clip at one time (#150): the clip name as in EUnitPose ("Melee"), any case.
            FString Pose;
            if (!(*Entry)->TryGetStringField(TEXT("role"), Shot.Role) || !ValidName(Shot.Role)) { Error = Shot.Name + TEXT(" needs a \"role\" (a class name such as Rogue)"); return false; }
            bool bFound = false;
            if ((*Entry)->TryGetStringField(TEXT("pose"), Pose))
            {
                for (int32 Clip = 0; Clip < ExpeditionPose::Count && !bFound; ++Clip)
                {
                    if (Pose.Equals(ExpeditionPose::ClipName(static_cast<EUnitPose>(Clip)), ESearchCase::IgnoreCase)) { Shot.Pose = static_cast<EUnitPose>(Clip); bFound = true; }
                }
            }
            if (!bFound) { Error = Shot.Name + TEXT(" needs a \"pose\" of Idle, Walk, Melee, Ranged, Cast, Hit, Downed, Death or Recover"); return false; }
            if ((*Entry)->HasField(TEXT("time")))
            {
                double Seconds = 0.0;
                if (!Number(*Entry, TEXT("time"), Seconds) || Seconds < 0.0 || Seconds > 10.0) { Error = Shot.Name + TEXT(" needs \"time\" in seconds from 0 to 10"); return false; }
                Shot.Time = static_cast<float>(Seconds);
            }
        }
        if (Shot.Mode == ECaptureMode::Impact)
        {
            if ((*Entry)->HasField(TEXT("effect")))
            {
                FString Effect;
                if (!(*Entry)->TryGetStringField(TEXT("effect"), Effect)) { Effect = TEXT("?"); }
                if (Effect == TEXT("hit")) { Shot.Effect = EImpactPreview::Hit; }
                else if (Effect == TEXT("spell")) { Shot.Effect = EImpactPreview::Spell; }
                else if (Effect == TEXT("fire")) { Shot.Effect = EImpactPreview::Fire; }
                else if (Effect == TEXT("missile")) { Shot.Effect = EImpactPreview::Missile; }
                else if (Effect == TEXT("arrow")) { Shot.Effect = EImpactPreview::Arrow; }
                else if (Effect == TEXT("bolt")) { Shot.Effect = EImpactPreview::Bolt; }
                else { Error = Shot.Name + TEXT(" needs an \"effect\" of \"hit\", \"spell\", \"fire\", \"missile\", \"arrow\" or \"bolt\""); return false; }
            }
            if ((*Entry)->HasField(TEXT("phase")))
            {
                FString Phase;
                if (!(*Entry)->TryGetStringField(TEXT("phase"), Phase)) { Phase = TEXT("?"); }
                if (Phase == TEXT("landing")) { Shot.Moment = EImpactMoment::Landing; }
                else if (Phase == TEXT("flight")) { Shot.Moment = EImpactMoment::Flight; }
                else if (Phase == TEXT("landing-first")) { Shot.Moment = EImpactMoment::FirstLanding; }
                else if (Phase == TEXT("landing-second")) { Shot.Moment = EImpactMoment::SecondLanding; }
                else if (Phase == TEXT("release")) { Shot.Moment = EImpactMoment::Release; }
                else { Error = Shot.Name + TEXT(" needs a \"phase\" of \"landing\", \"flight\", \"release\", \"landing-first\" or \"landing-second\""); return false; }
                if (Shot.Moment != EImpactMoment::Landing && Shot.Effect == EImpactPreview::Hit) { Error = Shot.Name + TEXT(" has a bolt phase but no bolt (\"effect\" spell, fire, missile, arrow or bolt)"); return false; }
                if (Shot.Moment == EImpactMoment::Release && Shot.Effect != EImpactPreview::Arrow && Shot.Effect != EImpactPreview::Bolt)
                { Error = Shot.Name + TEXT(" shows a release, which only \"effect\" arrow and bolt have"); return false; }
                if ((Shot.Moment == EImpactMoment::FirstLanding || Shot.Moment == EImpactMoment::SecondLanding) && Shot.Effect != EImpactPreview::Missile)
                { Error = Shot.Name + TEXT(" lands a first or second bolt, which only \"effect\" missile has"); return false; }
            }
            if ((*Entry)->HasField(TEXT("role")))
            {
                // The shooter (#198): a party hero of this class, held in its Ranged clip at the shot's moment; the shot's subject is that hero.
                if (!(*Entry)->TryGetStringField(TEXT("role"), Shot.Role) || !ValidName(Shot.Role)) { Error = Shot.Name + TEXT(" needs a \"role\" (a class name such as Ranger)"); return false; }
                if (Shot.Effect != EImpactPreview::Arrow && Shot.Effect != EImpactPreview::Bolt) { Error = Shot.Name + TEXT(" names a shooter, which only \"effect\" arrow and bolt have"); return false; }
            }
            if ((*Entry)->HasField(TEXT("frames")))
            {
                double Frames = 0.0;
                if (!Number(*Entry, TEXT("frames"), Frames) || Frames != FMath::FloorToDouble(Frames) || Frames < 1.0 || Frames > MaxImpactFrames)
                { Error = FString::Printf(TEXT("%s needs \"frames\" as a whole number from 1 to %d"), *Shot.Name, MaxImpactFrames); return false; }
                Shot.Frames = static_cast<int32>(Frames);
            }
        }
        if (Shot.Mode == ECaptureMode::Hint)
        {
            FString Hint;
            if (!(*Entry)->TryGetStringField(TEXT("hint"), Hint)) { Error = Shot.Name + TEXT(" needs a \"hint\" (move, resources, rescue, endturn)"); return false; }
            if (Hint == TEXT("move")) { Shot.Hint = ECaptureHint::Move; }
            else if (Hint == TEXT("resources")) { Shot.Hint = ECaptureHint::Resources; }
            else if (Hint == TEXT("rescue")) { Shot.Hint = ECaptureHint::Rescue; }
            else if (Hint == TEXT("endturn")) { Shot.Hint = ECaptureHint::EndTurn; }
            else { Error = FString::Printf(TEXT("%s has unknown hint \"%s\" (move, resources, rescue, endturn)"), *Shot.Name, *Hint); return false; }
        }
        if (Shot.Mode == ECaptureMode::Tooltip)
        {
            FString Action;
            if (!(*Entry)->TryGetStringField(TEXT("action"), Action)) { Error = Shot.Name + TEXT(" needs an \"action\" (attack, special1, special2, rescue, potion)"); return false; }
            if (Action == TEXT("attack")) { Shot.Action = ECaptureAction::Attack; }
            else if (Action == TEXT("special1")) { Shot.Action = ECaptureAction::Special1; }
            else if (Action == TEXT("special2")) { Shot.Action = ECaptureAction::Special2; }
            else if (Action == TEXT("rescue")) { Shot.Action = ECaptureAction::Rescue; }
            else if (Action == TEXT("potion")) { Shot.Action = ECaptureAction::Potion; }
            else { Error = FString::Printf(TEXT("%s has unknown action \"%s\" (attack, special1, special2, rescue, potion)"), *Shot.Name, *Action); return false; }
        }
        if (Shot.Mode == ECaptureMode::Battle || Shot.Mode == ECaptureMode::Targeting || Shot.Mode == ECaptureMode::Tooltip || Shot.Mode == ECaptureMode::Hint || Shot.Mode == ECaptureMode::Impact || Shot.Mode == ECaptureMode::Roll || Shot.Mode == ECaptureMode::Pose)
        {
            Shot.bHasCamera = true;
            double Yaw = 0.0;
            if (!Number(*Entry, TEXT("yaw"), Yaw) || Yaw < -360.0 || Yaw > 360.0) { Error = Shot.Name + TEXT(" needs a \"yaw\" from -360 to 360"); return false; }
            Shot.Yaw = static_cast<float>(Yaw);
            const TSharedPtr<FJsonValue> Distance = (*Entry)->TryGetField(TEXT("distance"));
            FString Word;
            double Number = 0.0;
            if (Distance.IsValid() && Distance->TryGetString(Word) && Word.Equals(TEXT("default"), ESearchCase::CaseSensitive)) { Shot.Distance = 0.0f; }
            else if (Distance.IsValid() && Distance->TryGetString(Word) && Word.Equals(TEXT("far"), ESearchCase::CaseSensitive)) { Shot.Distance = FarDistance; }
            else if (Distance.IsValid() && Distance->Type == EJson::Number && Distance->TryGetNumber(Number) && Number >= MinDistance && Number <= MaxDistance) { Shot.Distance = static_cast<float>(Number); }
            else { Error = FString::Printf(TEXT("%s needs a \"distance\" of \"default\", \"far\" or a number from %.0f to %.0f"), *Shot.Name, MinDistance, MaxDistance); return false; }
            // Optional "pan": {"right": cm, "away": cm}: the view pans after the camera is placed (the pan bound still applies), so a shot can show a pan stop.
            const TSharedPtr<FJsonObject>* PanObject = nullptr;
            if ((*Entry)->TryGetObjectField(TEXT("pan"), PanObject) && PanObject)
            {
                double Right = 0.0, Away = 0.0;
                if (!(*PanObject)->TryGetNumberField(TEXT("right"), Right) || !(*PanObject)->TryGetNumberField(TEXT("away"), Away) || FMath::Abs(Right) > 100000.0 || FMath::Abs(Away) > 100000.0)
                { Error = Shot.Name + TEXT(" needs a \"pan\" with numeric \"right\" and \"away\" within 100000"); return false; }
                Shot.bHasPan = true;
                Shot.Pan = FVector2D(Right, Away);
            }
            else if ((*Entry)->HasField(TEXT("pan"))) { Error = Shot.Name + TEXT(" has a \"pan\" that is not an object"); return false; }
        }
        if (Shot.Mode == ECaptureMode::Impact && Shot.Frames > Out.SettleFrames)
        { Error = FString::Printf(TEXT("%s has \"frames\" %d beyond settleFrames %d"), *Shot.Name, Shot.Frames, Out.SettleFrames); return false; }
        Out.Shots.Add(Shot);
    }
    return true;
}

bool ExpeditionCapture::Requested(const TCHAR* CommandLine)
{
    FString PosesPath;
    return CommandLine && FParse::Value(CommandLine, TEXT("ExpeditionCapture="), PosesPath) && !PosesPath.IsEmpty();
}

void FExpeditionCaptureDirector::DrainCompilation()
{
#if WITH_EDITOR
    // Editor builds swap a finished shader map onto its material asynchronously and free the old one before cached mesh draw
    // commands are rebuilt, so a parallel draw can read freed shader parameter info. Settle every compile first (#174).
    // Asset compiles can queue shader work, so shaders are finished again after them.
    if (GShaderCompilingManager) { GShaderCompilingManager->FinishAllCompilation(); }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (GShaderCompilingManager) { GShaderCompilingManager->FinishAllCompilation(); }
    FlushRenderingCommands();
#endif
}

TSharedPtr<FExpeditionCaptureDirector> FExpeditionCaptureDirector::TryCreate(AExpeditionPlayerController* Controller, AExpeditionBattleView* View)
{
    FString PosesPath;
    if (!FParse::Value(FCommandLine::Get(), TEXT("ExpeditionCapture="), PosesPath)) { return nullptr; }
    FString OutputDir;
    if (!FParse::Value(FCommandLine::Get(), TEXT("ExpeditionCaptureOut="), OutputDir) || OutputDir.IsEmpty())
    { OutputDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExpeditionCapture")); }
    FString Fixture;
    FParse::Value(FCommandLine::Get(), TEXT("ExpeditionCaptureFixture="), Fixture);
    return Create(Controller, View, PosesPath, OutputDir, Fixture);
}

TSharedPtr<FExpeditionCaptureDirector> FExpeditionCaptureDirector::Create(AExpeditionPlayerController* Controller, AExpeditionBattleView* View, const FString& PosesPath, const FString& OutputDir, const FString& Fixture)
{
    TSharedPtr<FExpeditionCaptureDirector> Director = MakeShared<FExpeditionCaptureDirector>();
    Director->Controller = Controller;
    Director->View = View;
    Director->PosesPath = PosesPath;
    Director->OutputDir = FPaths::ConvertRelativePathToFull(OutputDir);
    Director->Fixture = Fixture;
    Director->Drain = &FExpeditionCaptureDirector::DrainCompilation;
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

bool FExpeditionCaptureDirector::ProjectBox(FVector2D Cell, float HeightCm, float HalfCm, FIntRect& Out) const
{
    const AExpeditionBattleView* Board = View.Get();
    const AExpeditionPlayerController* Player = Controller.Get();
    ULocalPlayer* Local = Player ? Player->GetLocalPlayer() : nullptr;
    if (!Board || !Local || !Local->ViewportClient || !Local->ViewportClient->Viewport) { return false; }
    FSceneViewProjectionData Data;
    if (!Local->GetProjectionData(Local->ViewportClient->Viewport, Data)) { return false; }
    const FMatrix ViewProjection = Data.ComputeViewProjectionMatrix();
    const FIntRect ViewRect = Data.GetConstrainedViewRect();
    const FBoardMetric& Metric = Board->Metric();
    const FVector Centre = Board->BoardPosition(Cell, Metric.Scaled(HeightCm));
    const float Half = Metric.Scaled(HalfCm);
    FVector2D Low(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
    FVector2D High(-TNumericLimits<double>::Max(), -TNumericLimits<double>::Max());
    for (int32 Corner = 0; Corner < 8; ++Corner)
    {
        const FVector Point = Centre + FVector((Corner & 1) ? Half : -Half, (Corner & 2) ? Half : -Half, (Corner & 4) ? Half : -Half);
        FVector2D Pixel;
        if (!FSceneView::ProjectWorldToScreen(Board->GetActorTransform().TransformPosition(Point), ViewRect, ViewProjection, Pixel)) { return false; }
        Low = FVector2D(FMath::Min(Low.X, Pixel.X), FMath::Min(Low.Y, Pixel.Y));
        High = FVector2D(FMath::Max(High.X, Pixel.X), FMath::Max(High.Y, Pixel.Y));
    }
    Out = FIntRect(FMath::Max(ViewRect.Min.X, FMath::FloorToInt(Low.X)), FMath::Max(ViewRect.Min.Y, FMath::FloorToInt(Low.Y)),
        FMath::Min(ViewRect.Max.X, FMath::CeilToInt(High.X)), FMath::Min(ViewRect.Max.Y, FMath::CeilToInt(High.Y)));
    return Out.Width() > 0 && Out.Height() > 0;
}

bool FExpeditionCaptureDirector::ProjectFootprint(int32 UnitId, FIntRect& Out) const
{
    const AExpeditionBattleView* Board = View.Get();
    const AExpeditionPlayerController* Player = Controller.Get();
    ULocalPlayer* Local = Player ? Player->GetLocalPlayer() : nullptr;
    if (!Board || !Local || !Local->ViewportClient || !Local->ViewportClient->Viewport) { return false; }
    const FUnitDisplay* Unit = Board->Playback().Unit(UnitId);
    FSceneViewProjectionData Data;
    if (!Unit || !Local->GetProjectionData(Local->ViewportClient->Viewport, Data)) { return false; }
    const FMatrix ViewProjection = Data.ComputeViewProjectionMatrix();
    const FIntRect ViewRect = Data.GetConstrainedViewRect();
    const FBoardMetric& Metric = Board->Metric();
    const float Half = 0.5f * Metric.FootprintDiameter();
    const FVector Feet = Board->BoardPosition(Unit->Position + Unit->Lean, 0.0f);
    FVector2D Low(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
    FVector2D High(-TNumericLimits<double>::Max(), -TNumericLimits<double>::Max());
    for (int32 Corner = 0; Corner < 8; ++Corner)
    {
        const FVector Point = Feet + FVector((Corner & 1) ? Half : -Half, (Corner & 2) ? Half : -Half, (Corner & 4) ? Metric.FigureCm : 0.0f);
        FVector2D Pixel;
        if (!FSceneView::ProjectWorldToScreen(Board->GetActorTransform().TransformPosition(Point), ViewRect, ViewProjection, Pixel)) { return false; }
        Low = FVector2D(FMath::Min(Low.X, Pixel.X), FMath::Min(Low.Y, Pixel.Y));
        High = FVector2D(FMath::Max(High.X, Pixel.X), FMath::Max(High.Y, Pixel.Y));
    }
    Out = FIntRect(FMath::Max(ViewRect.Min.X, FMath::FloorToInt(Low.X)), FMath::Max(ViewRect.Min.Y, FMath::FloorToInt(Low.Y)),
        FMath::Min(ViewRect.Max.X, FMath::CeilToInt(High.X)), FMath::Min(ViewRect.Max.Y, FMath::CeilToInt(High.Y)));
    return Out.Width() > 0 && Out.Height() > 0;
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
    Player->ClearHintPreview();
    Board->ClearImpactPreview();
    Board->ClearRollPreview();
    Board->ClearPosePreview();
    switch (Shot.Mode)
    {
    case ECaptureMode::Battle:
    case ECaptureMode::Targeting:
    case ECaptureMode::Tooltip:
    case ECaptureMode::Hint:
    case ECaptureMode::Impact:
    case ECaptureMode::Roll:
    case ECaptureMode::Pose:
    {
        if (!Board->InCombat()) { Skip = TEXT("the checkpoint is not in an encounter"); return false; }
        // Default is the play default at that yaw: figure-sized (88 px at 1080), or the whole-board fit where that is closer (#148 gate 2). Far is the end of the pitch ramp (FarStop of the home fit), never nearer than the whole-board fit at that yaw, which can exceed it on a wide board.
        const float Wanted = Shot.Distance == ExpeditionCapture::FarDistance ? Board->FarFramedDistance(Shot.Yaw) : Shot.Distance > 0.0f ? Shot.Distance : Board->FramedDistance(Shot.Yaw);
        Board->SnapCamera(Shot.Yaw, Wanted);
        if (Shot.bHasPan) { Board->Pan(static_cast<float>(Shot.Pan.X), static_cast<float>(Shot.Pan.Y)); }
        if (Shot.Mode == ECaptureMode::Battle) { return true; }
        if (Shot.Mode == ECaptureMode::Pose)
        {
            // A party hero of the class, held in the clip at the time: no command is submitted and the battle is untouched (UpdateAnimation leaves the figure alone).
            const FCombatantState* Hero = nullptr;
            for (const FCombatantState& Unit : Board->Rules().Snapshot().Encounter.Combatants)
            { if (Unit.Side == ECombatSide::Party && UDiceRollTheme::ClassKey(Unit.AdventurerRole).ToString() == Shot.Role) { Hero = &Unit; break; } }
            if (!Hero) { Skip = FString::Printf(TEXT("no %s in the party"), *Shot.Role); return false; }
            if (!Board->PreviewPose(Hero->Id, Shot.Pose, Shot.Time)) { Skip = FString::Printf(TEXT("the %s is not a rigged figure"), *Shot.Role); return false; }
            Pending.TargetId = Hero->Id;
            Pending.TargetName = Hero->Name;
            Pending.TargetCell = Hero->Cell;
            return true;
        }
        if (Shot.Mode == ECaptureMode::Impact)
        {
            // The struck unit is the standing opponent nearest to whoever holds the turn. The hit itself is fired later, a fixed number of frames before the screenshot (Tick).
            const FCombatantState* Holder = Board->Rules().Find(Board->Rules().ActiveId());
            if (!Holder) { Skip = TEXT("nobody holds the turn"); return false; }
            const FCombatantState* Struck = nullptr;
            int32 Nearest = MAX_int32;
            for (const FCombatantState& Unit : Board->Rules().Snapshot().Encounter.Combatants)
            {
                const int32 Distance = FMath::Abs(Unit.Cell.X - Holder->Cell.X) + FMath::Abs(Unit.Cell.Y - Holder->Cell.Y);
                if (Unit.Side != Holder->Side && Unit.Life == ECombatLife::Conscious && Distance < Nearest) { Nearest = Distance; Struck = &Unit; }
            }
            if (!Struck) { Skip = TEXT("no standing opponent to strike"); return false; }
            Pending.TargetId = Struck->Id;
            Pending.TargetName = Struck->Name;
            Pending.TargetCell = Struck->Cell;
            Pending.ImpactFrom = FVector2D(Holder->Cell.X, Holder->Cell.Y);
            if (!Shot.Role.IsEmpty())
            {
                // A named shooter instead of whoever holds the turn: the party hero of that class, against the nearest standing foe.
                const FCombatantState* Shooter = nullptr;
                for (const FCombatantState& Unit : Board->Rules().Snapshot().Encounter.Combatants)
                { if (Unit.Side == ECombatSide::Party && UDiceRollTheme::ClassKey(Unit.AdventurerRole).ToString() == Shot.Role) { Shooter = &Unit; break; } }
                if (!Shooter) { Skip = FString::Printf(TEXT("no %s in the party"), *Shot.Role); return false; }
                Struck = nullptr; Nearest = MAX_int32;
                for (const FCombatantState& Unit : Board->Rules().Snapshot().Encounter.Combatants)
                {
                    const int32 Distance = FMath::Abs(Unit.Cell.X - Shooter->Cell.X) + FMath::Abs(Unit.Cell.Y - Shooter->Cell.Y);
                    if (Unit.Side == ECombatSide::Enemy && Unit.Life == ECombatLife::Conscious && Distance < Nearest) { Nearest = Distance; Struck = &Unit; }
                }
                if (!Struck) { Skip = TEXT("no standing opponent to strike"); return false; }
                Pending.TargetId = Struck->Id;
                Pending.TargetName = Struck->Name;
                Pending.TargetCell = Struck->Cell;
                Pending.ShooterId = Shooter->Id;
                Pending.ShooterName = Shooter->Name;
                Pending.ImpactFrom = FVector2D(Shooter->Cell.X, Shooter->Cell.Y);
            }
            return true;
        }
        if (Shot.Mode == ECaptureMode::Roll)
        {
            // The card an attack shows (#190): the acting hero against the nearest standing enemy, a d20 and a damage die landed, from
            // the same facts the playback reads. Presentation only: no command is submitted and the battle is untouched.
            const FCombatantState* Hero = Board->Rules().Find(Board->Rules().ActiveId());
            if (!Hero || Hero->Side != ECombatSide::Party) { Skip = TEXT("an enemy holds the turn"); return false; }
            const FCombatantState* Foe = nullptr;
            int32 Nearest = MAX_int32;
            for (const FCombatantState& Unit : Board->Rules().Snapshot().Encounter.Combatants)
            {
                const int32 Distance = FMath::Abs(Unit.Cell.X - Hero->Cell.X) + FMath::Abs(Unit.Cell.Y - Hero->Cell.Y);
                if (Unit.Side == ECombatSide::Enemy && Unit.Life != ECombatLife::Dead && Distance < Nearest) { Nearest = Distance; Foe = &Unit; }
            }
            if (!Foe) { Skip = TEXT("no enemy is standing"); return false; }
            FDiceTray Tray;
            Tray.Title = FString::Printf(TEXT("%s attacks %s"), *Hero->Name, *Foe->Name);
            Tray.Dice.Add({20, 17, 0.0f, FLinearColor(0.95f, 0.92f, 0.84f)});
            Tray.Dice.Add({8, 6, 0.05f, FLinearColor(1.0f, 0.55f, 0.45f)});
            Tray.Detail = FString::Printf(TEXT("d20 17 + 5 = 22  vs  AC %d   ·   damage 1d8+3 = 9"), Foe->ArmorClass);
            Tray.Verdict = TEXT("HIT");
            Tray.VerdictColor = FLinearColor(1.0f, 0.55f, 0.35f);
            Board->PreviewRoll(Tray);
            return true;
        }
        if (Shot.Mode == ECaptureMode::Hint)
        {
            // The hint is raised exactly as play raises it, from the same facts, so a fixture that does not show its cause skips.
            ECombatHint Hint = ECombatHint::MoveSelectConfirm;
            switch (Shot.Hint)
            {
            case ECaptureHint::Resources: Hint = ECombatHint::ScarceResources; break;
            case ECaptureHint::Rescue: Hint = ECombatHint::DownedRescue; break;
            case ECaptureHint::EndTurn: Hint = ECombatHint::EndingTurn; break;
            case ECaptureHint::Move: break;
            }
            return Player->PreviewHint(Hint, Skip);
        }
        if (Shot.Mode == ECaptureMode::Tooltip)
        {
            const FCombatantState* Hero = Board->Rules().Find(Board->Rules().ActiveId());
            if (!Hero || Hero->Side != ECombatSide::Party) { Skip = TEXT("an enemy holds the turn"); return false; }
            ECombatCommandType Action = ECombatCommandType::Attack;
            switch (Shot.Action)
            {
            case ECaptureAction::Special1: Action = Player->SpecialAction(0); break;
            case ECaptureAction::Special2: Action = Player->SpecialAction(1); break;
            case ECaptureAction::Rescue: Action = ECombatCommandType::Rescue; break;
            case ECaptureAction::Potion: Action = ECombatCommandType::Potion; break;
            case ECaptureAction::Attack: Action = ECombatCommandType::Attack; break;
            }
            if (Shot.Action != ECaptureAction::Attack && Action == ECombatCommandType::Attack) { Skip = TEXT("the active hero has no such action"); return false; }
            Player->PreviewMode(Action);
            return true;
        }
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
    case ECaptureMode::Start:
        if (Board->HasRun())
        {
            if (!Board->CanReturnToMenu()) { Skip = TEXT("the run cannot be put away"); return false; }
            Board->ReturnToMenu();
        }
        if (!Shot.Build.IsEmpty() && !Player->SelectBuild(Shot.Build)) { Skip = FString::Printf(TEXT("the build picker cannot select %s"), *Shot.Build); return false; }
        Board->SnapCameraToTargets();
        return true;
    }
    Skip = TEXT("unknown capture mode");
    return false;
}

void FExpeditionCaptureDirector::Tick()
{
    AExpeditionBattleView* Board = View.Get();
    if (Stage == EStage::Done) { return; }
    if (!Board || !Controller.IsValid()) { Fail(TEXT("The battle view disappeared")); return; }
    ++Frames; ++StageFrames;
    // Before anything is drawn from the start-screen board (#174).
    if (!bDrainedAtStart) { bDrainedAtStart = true; Drain(); }
    switch (Stage)
    {
    case EStage::Loading:
        if (!bResumed)
        {
            // A few frames first so the world, HUD and viewport are fully up.
            if (Frames < 10) { return; }
            // Resume rebuilds the board and loads new materials; no compile may land while a frame draws them (#174).
            Drain();
            Board->Resume();
            Drain();
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
        FString Skip;
        if (Apply(Pending.Shot, Skip)) { Pending.Status = TEXT("captured"); Stage = EStage::Settling; StageFrames = 0; }
        else { Pending.Status = TEXT("skipped"); Pending.Reason = Skip; Results.Add(Pending); ++NextShot; StageFrames = 0; }
        return;
    }
    case EStage::Settling:
        if (Pending.Shot.Mode == ECaptureMode::Impact)
        {
            // The hit lands Frames frames before the screenshot, once the pose has settled; its flash is held at the peak from then on (#189).
            if (!Pending.bImpactFired && StageFrames >= Poses.SettleFrames - Pending.Shot.Frames)
            {
                Board->PreviewImpact(Pending.TargetId, Pending.Shot.Effect, Pending.ImpactFrom, Pending.Shot.Moment);
                if (Pending.ShooterId != INDEX_NONE)
                {
                    // The shooter in its Ranged clip at the moment: the release frame, then a little after it (the follow-through) for the flight and the landing.
                    const float Release = FExpeditionPlayback::RangedReleaseSeconds;
                    const float Held = Pending.Shot.Moment == EImpactMoment::Release ? Release : Pending.Shot.Moment == EImpactMoment::Flight ? Release + 0.15f : Release + 0.3f;
                    if (!Board->PreviewPose(Pending.ShooterId, EUnitPose::Ranged, Held)) { Fail(FString::Printf(TEXT("%s is not a rigged figure with a Ranged clip"), *Pending.ShooterName)); return; }
                }
                Pending.bImpactFired = true;
            }
            if (StageFrames < Poses.SettleFrames) { return; }
        }
        else if (StageFrames < Poses.SettleFrames || !Idle()) { if (StageFrames > Poses.SettleFrames + IdleTimeoutFrames) { Fail(TEXT("Playback never went idle while settling")); } return; }
#if WITH_EDITOR
        // Shaders or textures still compiling would be captured as placeholders (a changed material needs new
        // shader permutations): finish them, then settle a few frames more so the shot uses the real ones.
        if ((GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) || FAssetCompilingManager::Get().GetNumRemainingAssets() > 0)
        {
            if (GShaderCompilingManager) { GShaderCompilingManager->FinishAllCompilation(); }
            FAssetCompilingManager::Get().FinishAllCompilation();
            StageFrames = FMath::Max(0, Poses.SettleFrames - 10);
            return;
        }
#endif
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
            Pending.Units.Reset();
            if (Pending.Shot.Mode == ECaptureMode::Battle || Pending.Shot.Mode == ECaptureMode::Targeting || Pending.Shot.Mode == ECaptureMode::Pose)
            {
                const UEnum* RoleEnum = StaticEnum<EAdventurerRole>();
                for (const FCombatantState& Combatant : Board->Rules().Snapshot().Encounter.Combatants)
                {
                    FResult::FUnitBox Entry;
                    if (!ProjectFootprint(Combatant.Id, Entry.Box)) { continue; }
                    Entry.Id = Combatant.Id;
                    Entry.Name = Combatant.Name;
                    Entry.bParty = Combatant.Side == ECombatSide::Party;
                    Entry.Role = Entry.bParty && RoleEnum ? RoleEnum->GetNameStringByValue(static_cast<int64>(Combatant.AdventurerRole)) : TEXT("Foe");
                    Entry.bRigged = Board->UnitAnimation(Combatant.Id).bSkeletal;
                    Pending.Units.Add(Entry);
                }
            }
            if (Pending.Shot.Mode == ECaptureMode::Impact)
            {
                Pending.bHasFootprint = ProjectFootprint(Pending.TargetId, Pending.Footprint);
                if (Pending.ShooterId != INDEX_NONE) { Pending.bHasShooterBox = ProjectFootprint(Pending.ShooterId, Pending.ShooterBox); }
                if (Pending.Shot.Moment == EImpactMoment::Flight)
                {
                    // Half way along the path, at the bolt's height (about 55 plus the arc's 20 at the midpoint for a lobbed bolt).
                    Pending.bHasBolt = ProjectBox(0.5f * (Pending.ImpactFrom + FVector2D(Pending.TargetCell.X, Pending.TargetCell.Y)), 75.0f, 45.0f, Pending.Bolt);
                }
            }
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
    // Shot distances are reference units (110 cm cell); the camera scales them by cellCm / 110.
    if (View.IsValid()) { Root->SetNumberField(TEXT("cellCm"), View->Metric().CellCm); }
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
        if (Result.Shot.Mode == ECaptureMode::Tooltip) { Shot->SetStringField(TEXT("action"), ExpeditionCapture::ActionName(Result.Shot.Action)); }
        if (Result.Shot.Mode == ECaptureMode::Hint) { Shot->SetStringField(TEXT("hint"), ExpeditionCapture::HintName(Result.Shot.Hint)); }
        if (Result.Shot.Mode == ECaptureMode::Impact && Result.Status != TEXT("skipped"))
        {
            Shot->SetStringField(TEXT("effect"), ExpeditionCapture::EffectName(Result.Shot.Effect));
            Shot->SetNumberField(TEXT("frames"), Result.Shot.Frames);
            Shot->SetStringField(TEXT("phase"), ExpeditionCapture::PhaseName(Result.Shot.Moment));
            // The struck combatant and its footprint box in screenshot pixels, for tools/impact_flash.py (#189).
            TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
            Target->SetNumberField(TEXT("id"), Result.TargetId);
            Target->SetStringField(TEXT("name"), Result.TargetName);
            Target->SetNumberField(TEXT("cellX"), Result.TargetCell.X);
            Target->SetNumberField(TEXT("cellY"), Result.TargetCell.Y);
            if (Result.bHasFootprint)
            {
                TSharedRef<FJsonObject> Box = MakeShared<FJsonObject>();
                Box->SetNumberField(TEXT("x0"), Result.Footprint.Min.X); Box->SetNumberField(TEXT("y0"), Result.Footprint.Min.Y);
                Box->SetNumberField(TEXT("x1"), Result.Footprint.Max.X); Box->SetNumberField(TEXT("y1"), Result.Footprint.Max.Y);
                Target->SetObjectField(TEXT("footprint"), Box);
            }
            if (Result.bHasBolt)
            {
                // An in-flight shot's bolt box, measured with the target's (tools/impact_flash.py).
                TSharedRef<FJsonObject> Bolt = MakeShared<FJsonObject>();
                Bolt->SetNumberField(TEXT("x0"), Result.Bolt.Min.X); Bolt->SetNumberField(TEXT("y0"), Result.Bolt.Min.Y);
                Bolt->SetNumberField(TEXT("x1"), Result.Bolt.Max.X); Bolt->SetNumberField(TEXT("y1"), Result.Bolt.Max.Y);
                Target->SetObjectField(TEXT("bolt"), Bolt);
            }
            Shot->SetObjectField(TEXT("target"), Target);
            if (Result.ShooterId != INDEX_NONE)
            {
                // The shooter of an arrow or bolt shot and its footprint box (tools/impact_flash.py measures it, #198).
                TSharedRef<FJsonObject> Shooter = MakeShared<FJsonObject>();
                Shooter->SetNumberField(TEXT("id"), Result.ShooterId);
                Shooter->SetStringField(TEXT("name"), Result.ShooterName);
                Shooter->SetStringField(TEXT("role"), Result.Shot.Role);
                if (Result.bHasShooterBox)
                {
                    TSharedRef<FJsonObject> Box = MakeShared<FJsonObject>();
                    Box->SetNumberField(TEXT("x0"), Result.ShooterBox.Min.X); Box->SetNumberField(TEXT("y0"), Result.ShooterBox.Min.Y);
                    Box->SetNumberField(TEXT("x1"), Result.ShooterBox.Max.X); Box->SetNumberField(TEXT("y1"), Result.ShooterBox.Max.Y);
                    Shooter->SetObjectField(TEXT("footprint"), Box);
                }
                Shot->SetObjectField(TEXT("shooter"), Shooter);
            }
        }
        if (Result.Shot.Mode == ECaptureMode::Pose && Result.Status != TEXT("skipped"))
        {
            // The hero and the clip the shot held (#150).
            TSharedRef<FJsonObject> Held = MakeShared<FJsonObject>();
            Held->SetNumberField(TEXT("id"), Result.TargetId);
            Held->SetStringField(TEXT("name"), Result.TargetName);
            Held->SetStringField(TEXT("role"), Result.Shot.Role);
            Held->SetStringField(TEXT("pose"), ExpeditionPose::ClipName(Result.Shot.Pose));
            Held->SetNumberField(TEXT("time"), Result.Shot.Time);
            Shot->SetObjectField(TEXT("held"), Held);
        }
        if (!Result.Units.IsEmpty() && Result.Status != TEXT("skipped"))
        {
            // Every figure's footprint box in screenshot pixels (#150), for measuring a class against the ground behind it.
            TArray<TSharedPtr<FJsonValue>> Units;
            for (const FResult::FUnitBox& Entry : Result.Units)
            {
                TSharedRef<FJsonObject> Unit = MakeShared<FJsonObject>();
                Unit->SetNumberField(TEXT("id"), Entry.Id);
                Unit->SetStringField(TEXT("name"), Entry.Name);
                Unit->SetStringField(TEXT("role"), Entry.Role);
                Unit->SetStringField(TEXT("side"), Entry.bParty ? TEXT("party") : TEXT("enemy"));
                Unit->SetBoolField(TEXT("rigged"), Entry.bRigged);
                TSharedRef<FJsonObject> Box = MakeShared<FJsonObject>();
                Box->SetNumberField(TEXT("x0"), Entry.Box.Min.X); Box->SetNumberField(TEXT("y0"), Entry.Box.Min.Y);
                Box->SetNumberField(TEXT("x1"), Entry.Box.Max.X); Box->SetNumberField(TEXT("y1"), Entry.Box.Max.Y);
                Unit->SetObjectField(TEXT("box"), Box);
                Units.Add(MakeShared<FJsonValueObject>(Unit));
            }
            Shot->SetArrayField(TEXT("units"), Units);
        }
        if (!Result.Shot.Build.IsEmpty()) { Shot->SetStringField(TEXT("build"), Result.Shot.Build); }
        Shot->SetStringField(TEXT("status"), Result.Status);
        if (Result.Status == TEXT("skipped")) { Shot->SetStringField(TEXT("reason"), Result.Reason); }
        else
        {
            Shot->SetStringField(TEXT("file"), Result.File);
            Shot->SetNumberField(TEXT("yaw"), Result.Yaw);
            Shot->SetNumberField(TEXT("distance"), Result.Distance);
            if (Result.Shot.bHasPan)
            {
                TSharedRef<FJsonObject> Pan = MakeShared<FJsonObject>();
                Pan->SetNumberField(TEXT("right"), Result.Shot.Pan.X);
                Pan->SetNumberField(TEXT("away"), Result.Shot.Pan.Y);
                Shot->SetObjectField(TEXT("pan"), Pan);
            }
            TSharedRef<FJsonObject> Camera = MakeShared<FJsonObject>();
            Camera->SetObjectField(TEXT("location"), Vector(Result.Location, TEXT("x"), TEXT("y"), TEXT("z")));
            Camera->SetObjectField(TEXT("rotation"), Vector(FVector(Result.Rotation.Pitch, Result.Rotation.Yaw, Result.Rotation.Roll), TEXT("pitch"), TEXT("yaw"), TEXT("roll")));
            Camera->SetNumberField(TEXT("fov"), Result.Fov);
            Shot->SetObjectField(TEXT("camera"), Camera);
        }
        Shots.Add(MakeShared<FJsonValueObject>(Shot));
    }
    Root->SetArrayField(TEXT("shots"), Shots);
    FString Text;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
    FJsonSerializer::Serialize(Root, Writer);
    FFileHelper::SaveStringToFile(Text, *FPaths::Combine(OutputDir, TEXT("manifest.json")));
}
