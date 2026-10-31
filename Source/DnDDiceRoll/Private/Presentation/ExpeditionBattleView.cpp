#include "Presentation/ExpeditionBattleView.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Light.h"
#include "Components/LightComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
constexpr float CellSize = 110.0f;
constexpr float HUDDesignWidth = 1280.0f;
constexpr float PartyPanelWidth = 245.0f;
constexpr float PartyCardsTop = 100.0f;
constexpr float PartyCardHeight = 67.0f;
constexpr int32 PartyCardCount = 4;
FLinearColor PartyColor(int32 Id)
{
    const FLinearColor Colors[] = {{0.15f, 0.55f, 1.0f}, {0.3f, 0.85f, 0.5f}, {1.0f, 0.8f, 0.25f}, {0.7f, 0.4f, 1.0f}};
    return Colors[FMath::Clamp(Id - 1, 0, 3)];
}
}

AExpeditionBattleView::AExpeditionBattleView()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("BattleCamera"));
    Camera->SetupAttachment(RootComponent);
    Camera->FieldOfView = 55.0f;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    Cube = CubeAsset.Object; Cylinder = CylinderAsset.Object; BaseMaterial = MaterialAsset.Object;
}

UStaticMeshComponent* AExpeditionBattleView::MakeMesh(UStaticMesh* Mesh, FVector Position, FVector Scale, FLinearColor Color)
{
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
    AddInstanceComponent(Component);
    Component->SetupAttachment(RootComponent);
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetStaticMesh(Mesh);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetRelativeLocation(Position);
    Component->SetRelativeScale3D(Scale);
    if (BaseMaterial)
    {
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Component->SetMaterial(0, Material);
    }
    Component->RegisterComponent();
    return Component;
}

void AExpeditionBattleView::BeginPlay()
{
    Super::BeginPlay();
    FString SaveRoot;
    if (!FParse::Value(FCommandLine::Get(), TEXT("ExpeditionSaveRoot="), SaveRoot))
    { SaveRoot = FPlatformMisc::GetEnvironmentVariable(TEXT("EXPEDITION_SAVE_ROOT")); }
    if (!SaveRoot.IsEmpty()) { ConfigureStorage(FPaths::Combine(SaveRoot, TEXT("current-v1.json"))); }
    // The starter fixture has no baked lighting. Runtime presentation uses dynamic lights.
    for (TActorIterator<ALight> Light(GetWorld()); Light; ++Light)
    {
        if (ULightComponent* Component = Light->GetLightComponent()) { Component->SetMobility(EComponentMobility::Movable); }
    }
    Feedback.Add(TEXT("Welcome. N: new expedition | O: resume last checkpoint | H: ending records."));
    UpdateCamera();
}

void AExpeditionBattleView::RebuildBoard()
{
    ++Revision;
    for (auto& Mesh : BoardMeshes) { if (IsValid(Mesh)) { RemoveInstanceComponent(Mesh); Mesh->DestroyComponent(); } }
    for (auto& Mesh : UnitMeshes) { if (IsValid(Mesh)) { RemoveInstanceComponent(Mesh); Mesh->DestroyComponent(); } }
    BoardMeshes.Reset(); UnitMeshes.Reset();
    const FCombatEncounter& Encounter = Rules().Snapshot().Encounter;
    for (int32 Y = 0; Y < Encounter.Height; ++Y)
    {
        for (int32 X = 0; X < Encounter.Width; ++X)
        {
            const FIntPoint Cell(X, Y);
            BoardMeshes.Add(MakeMesh(Cube, CellPosition(Cell) - FVector(0, 0, 14), FVector(1.04f, 1.04f, 0.2f),
                (X + Y) % 2 == 0 ? FLinearColor(0.17f, 0.24f, 0.28f) : FLinearColor(0.23f, 0.31f, 0.34f)));
            if (Encounter.BlockedCells.Contains(Cell))
            {
                BoardMeshes.Add(MakeMesh(Cube, CellPosition(Cell) + FVector(0, 0, 35), FVector(0.9f, 0.9f, 0.8f), FLinearColor(0.32f, 0.27f, 0.22f)));
            }
        }
    }
    for (const FCombatantState& Unit : Encounter.Combatants)
    {
        UnitMeshes.Add(MakeMesh(Cylinder, CellPosition(Unit.Cell), FVector(0.55f, 0.55f, 0.75f),
            Unit.Side == ECombatSide::Party ? PartyColor(Unit.Id) : FLinearColor(0.8f, 0.17f, 0.12f)));
    }
    CameraDistance = 1700.0f + ((Encounter.Width > 8 || Encounter.Height > 7) ? 100.0f : 0.0f);
    RefreshUnits(); UpdateCamera();
}

FVector AExpeditionBattleView::CellPosition(FIntPoint Cell) const
{
    return FVector(Cell.X * CellSize, Cell.Y * CellSize, 35.0f);
}

void AExpeditionBattleView::RefreshUnits()
{
    const TArray<FCombatantState>& Units = Rules().Snapshot().Encounter.Combatants;
    for (int32 Index = 0; Index < Units.Num(); ++Index)
    {
        if (!UnitMeshes.IsValidIndex(Index) || !IsValid(UnitMeshes[Index])) { continue; }
        const bool bActive = Units[Index].HP > 0;
        UnitMeshes[Index]->SetRelativeLocation(CellPosition(Units[Index].Cell) + FVector(0, 0, bActive ? 20 : -20));
        UnitMeshes[Index]->SetRelativeScale3D(FVector(0.55f, 0.55f, bActive ? 0.75f : 0.12f));
    }
}

void AExpeditionBattleView::Record(const FCombatResult& Result)
{
    if (Result.Feedback.IsEmpty()) { return; }
    Feedback.Add((Result.bAccepted ? TEXT("") : TEXT("Cannot act: ")) + Result.Feedback);
    if (Feedback.Num() > 6) { Feedback.RemoveAt(0); }
}

void AExpeditionBattleView::Execute(const FCombatCommand& Command)
{
    if (bShowRecords) { return; }
    const FCombatantState* Unit = Rules().Find(Command.ActorId);
    if (!Unit || Unit->Side != ECombatSide::Party) { return; }
    Record(Session.Submit(Command));
    RefreshUnits();
}

void AExpeditionBattleView::ChooseRoute(int32 Index)
{
    if (bShowRecords) { return; }
    FString Error;
    const auto& Offers = Session.Run().Progress().Offers;
    const int32 Id = Offers.IsValidIndex(Index) ? Offers[Index].Id : INDEX_NONE;
    if (Session.ChooseRoute(Id, Error))
    {
        Record({true, TEXT("Route committed. Injuries, uses and permanent deaths carry; no automatic rest.")});
        EnemyDelay = 0; RebuildBoard();
    }
    else { Record({false, Error}); }
}

void AExpeditionBattleView::ClaimReward(int32 RecipientId)
{
    if (bShowRecords) { return; }
    FString Error;
    if (Session.ClaimReward(RecipientId, Error)) { Record({true, TEXT("Reward claimed once for selected survivor. Continue at camp or choose the next route.")}); }
    else { Record({false, Error}); }
}

void AExpeditionBattleView::Recover(bool bTake)
{
    if (bShowRecords) { return; }
    FString Error;
    if (Session.Recover(bTake, Error)) { Record({true, bTake ? TEXT("Camp used: ") + FExpeditionRun::RecoveryDescription() : TEXT("Recovery skipped for this run. Choose the next route.")}); RefreshUnits(); }
    else { Record({false, Error}); }
}

void AExpeditionBattleView::NewRun()
{
    if (HasRun() && Progress().Phase != EExpeditionPhase::Victory && Progress().Phase != EExpeditionPhase::Defeat)
    { Record({false, TEXT("Finish this expedition before starting a new run.")}); return; }
    FString Error;
    const int32 NextSeed = HasRun() ? static_cast<int32>(static_cast<uint32>(Progress().Seed) + 1u) : 20261030;
    if (Session.NewRun(NextSeed, Error)) { Feedback.Reset(); Record({true, TEXT("Fresh expedition: party power/resources reset. Choose Z / X.")}); EnemyDelay = 0; CameraYaw = -90; bCheckpointBlocked = false; bShowRecords = false; RebuildBoard(); }
    else { Record({false, Error}); }
}

void AExpeditionBattleView::ConfigureStorage(const FString& Path)
{
    if (!HasRun()) { Session = FExpeditionSession(Path); }
}
void AExpeditionBattleView::Save()
{
    FString Error;
    if (Session.Save(Error)) { bCheckpointBlocked = false; Record({true, TEXT("Expedition saved. Every accepted action is also checkpointed.")}); }
    else { Record({false, Error}); }
}
void AExpeditionBattleView::Resume()
{
    FString Error;
    if (Session.Resume(Error))
    {
        EnemyDelay = 0; bCheckpointBlocked = false; bShowRecords = false; CameraYaw = -90;
        Feedback.Reset(); Record({true, TEXT("Expedition resumed: latest turns, resources and permanent losses retained.")}); RebuildBoard();
    }
    else { Record({false, Error}); }
}
void AExpeditionBattleView::ToggleRecords()
{
    if (bShowRecords) { bShowRecords = false; return; }
    FString Error;
    if (Session.ReadRecords(EndingRecords, Error)) { bShowRecords = true; RecordsOffset = 0; }
    else { Record({false, Error}); }
}

void AExpeditionBattleView::PageRecords(int32 Direction)
{
    if (!bShowRecords) { return; }
    const int32 LastPage = EndingRecords.IsEmpty() ? 0 : ((EndingRecords.Num() - 1) / RecordsPerPage) * RecordsPerPage;
    RecordsOffset = FMath::Clamp(RecordsOffset + FMath::Clamp(Direction, -1, 1) * RecordsPerPage, 0, LastPage);
}

void AExpeditionBattleView::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasRun() || bShowRecords || bCheckpointBlocked) { return; }
    const FCombatantState* Active = Rules().Find(Rules().ActiveId());
    if (Progress().Phase != EExpeditionPhase::Combat || Rules().Snapshot().Phase != EBattlePhase::Fighting || !Active || Active->Side != ECombatSide::Enemy) { EnemyDelay = 0; return; }
    EnemyDelay += DeltaSeconds;
    if (EnemyDelay >= 1.0f)
    {
        for (const FCombatResult& Result : Session.PlayEnemyTurn()) { Record(Result); if (!Result.bAccepted) { bCheckpointBlocked = true; } }
        RefreshUnits(); EnemyDelay = 0;
    }
}

void AExpeditionBattleView::UpdateCamera()
{
    const FVector Focus((Rules().Snapshot().Encounter.Width - 1) * CellSize / 2,
        (Rules().Snapshot().Encounter.Height - 1) * CellSize / 2 - 150, 0);
    const float Radians = FMath::DegreesToRadians(CameraYaw);
    const FVector Offset(FMath::Cos(Radians) * CameraDistance * 0.65f,
        FMath::Sin(Radians) * CameraDistance * 0.65f, CameraDistance);
    Camera->SetWorldLocation(Focus + Offset);
    Camera->SetWorldRotation((-Offset).Rotation());
}
void AExpeditionBattleView::Orbit(float Degrees) { CameraYaw = FMath::Fmod(CameraYaw + Degrees, 360.0f); UpdateCamera(); }
void AExpeditionBattleView::Zoom(float Amount) { CameraDistance = FMath::Clamp(CameraDistance + Amount, 1000.0f, 1800.0f); UpdateCamera(); }

bool AExpeditionBattleView::CursorCell(APlayerController* Controller, FIntPoint& Cell) const
{
    FVector Origin, Direction;
    if (!Controller || !Controller->DeprojectMousePositionToWorld(Origin, Direction) || Direction.Z >= -KINDA_SMALL_NUMBER) { return false; }
    const FVector Intersection = Origin + Direction * ((35.0f - Origin.Z) / Direction.Z);
    Cell = FIntPoint(FMath::RoundToInt(Intersection.X / CellSize), FMath::RoundToInt(Intersection.Y / CellSize));
    return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Rules().Snapshot().Encounter.Width && Cell.Y < Rules().Snapshot().Encounter.Height;
}

void AExpeditionPlayerController::BeginPlay()
{
    Super::BeginPlay();
    BattleView = GetWorld()->SpawnActor<AExpeditionBattleView>();
    if (!IsValid(BattleView)) { return; }
    SetViewTarget(BattleView);
    bShowMouseCursor = true;
    FInputModeGameOnly Mode;
    Mode.SetConsumeCaptureMouseDown(false);
    SetInputMode(Mode);
    if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
    {
        Viewport->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);
        Viewport->SetMouseLockMode(EMouseLockMode::DoNotLock);
    }
}

void AExpeditionPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AExpeditionPlayerController::Click);
    InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AExpeditionPlayerController::EndTurn);
    InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AExpeditionPlayerController::RotateLeft);
    InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AExpeditionPlayerController::RotateRight);
    InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AExpeditionPlayerController::ZoomIn);
    InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AExpeditionPlayerController::ZoomOut);
    InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AExpeditionPlayerController::SelectOne);
    InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AExpeditionPlayerController::SelectTwo);
    InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AExpeditionPlayerController::SelectThree);
    InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AExpeditionPlayerController::SelectFour);
    InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AExpeditionPlayerController::CursorUp);
    InputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AExpeditionPlayerController::CursorDown);
    InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &AExpeditionPlayerController::CursorLeft);
    InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AExpeditionPlayerController::CursorRight);
    InputComponent->BindKey(EKeys::A, IE_Pressed, this, &AExpeditionPlayerController::AttackMode);
    InputComponent->BindKey(EKeys::F, IE_Pressed, this, &AExpeditionPlayerController::RoleMode);
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AExpeditionPlayerController::RescueMode);
    InputComponent->BindKey(EKeys::P, IE_Pressed, this, &AExpeditionPlayerController::PotionMode);
    InputComponent->BindKey(EKeys::L, IE_Pressed, this, &AExpeditionPlayerController::Loot);
    InputComponent->BindKey(EKeys::Z, IE_Pressed, this, &AExpeditionPlayerController::FirstRoute);
    InputComponent->BindKey(EKeys::X, IE_Pressed, this, &AExpeditionPlayerController::SecondRoute);
    InputComponent->BindKey(EKeys::C, IE_Pressed, this, &AExpeditionPlayerController::Camp);
    InputComponent->BindKey(EKeys::V, IE_Pressed, this, &AExpeditionPlayerController::SkipCamp);
    InputComponent->BindKey(EKeys::N, IE_Pressed, this, &AExpeditionPlayerController::NewRun);
    InputComponent->BindKey(EKeys::K, IE_Pressed, this, &AExpeditionPlayerController::Save);
    InputComponent->BindKey(EKeys::O, IE_Pressed, this, &AExpeditionPlayerController::Resume);
    InputComponent->BindKey(EKeys::H, IE_Pressed, this, &AExpeditionPlayerController::History);
    InputComponent->BindKey(EKeys::PageUp, IE_Pressed, this, &AExpeditionPlayerController::OlderRecords);
    InputComponent->BindKey(EKeys::PageDown, IE_Pressed, this, &AExpeditionPlayerController::NewerRecords);
    InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AExpeditionPlayerController::ConfirmCell);
}

void AExpeditionPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (!IsValid(BattleView)) { return; }
    if (LastRevision != BattleView->PresentationRevision())
    {
        LastRevision = BattleView->PresentationRevision(); LastActiveId = INDEX_NONE;
        SelectedId = INDEX_NONE; bKeyboardCursor = false; SelectedAction = ECombatCommandType::Attack;
        SelectPartyIndex(0); GridCursor = {1,1};
    }
    const int32 ActiveId = BattleView->Rules().ActiveId();
    if (ActiveId != LastActiveId)
    {
        const FCombatantState* Active = BattleView->Rules().Find(ActiveId);
        if (Active && Active->Side == ECombatSide::Party) { SelectedId = ActiveId; GridCursor = Active->Cell; }
        LastActiveId = ActiveId;
        SelectedAction = ECombatCommandType::Attack;
    }
}
bool AExpeditionPlayerController::HoverCell(FIntPoint& Cell) const
{
    if (KeyboardCell(Cell)) { return true; }
    return IsValid(BattleView) && BattleView->CursorCell(const_cast<AExpeditionPlayerController*>(this), Cell);
}
bool AExpeditionPlayerController::KeyboardCell(FIntPoint& Cell) const
{
    if (!bKeyboardCursor) { return false; }
    Cell = GridCursor; return true;
}
void AExpeditionPlayerController::MoveCursor(FIntPoint Offset)
{
    if (!IsValid(BattleView)) { return; }
    const FCombatEncounter& Encounter = BattleView->Rules().Snapshot().Encounter;
    GridCursor.X = FMath::Clamp(GridCursor.X + Offset.X, 0, Encounter.Width - 1);
    GridCursor.Y = FMath::Clamp(GridCursor.Y + Offset.Y, 0, Encounter.Height - 1);
    bKeyboardCursor = true;
}
// Arrow directions follow the board as seen from the initial camera orientation.
void AExpeditionPlayerController::CursorUp() { MoveCursor({0, 1}); }
void AExpeditionPlayerController::CursorDown() { MoveCursor({0, -1}); }
void AExpeditionPlayerController::CursorLeft() { MoveCursor({1, 0}); }
void AExpeditionPlayerController::CursorRight() { MoveCursor({-1, 0}); }
void AExpeditionPlayerController::ConfirmCell() { if (IsValid(BattleView)) { bKeyboardCursor = true; ActAtCell(GridCursor); } }
void AExpeditionPlayerController::SelectPartyIndex(int32 Index)
{
    if (!IsValid(BattleView)) { return; }
    int32 PartyIndex = 0;
    for (const FCombatantState& Unit : BattleView->Rules().Snapshot().Encounter.Combatants)
    {
        if (Unit.Side == ECombatSide::Party && PartyIndex++ == Index) { SelectedId = Unit.Id; return; }
    }
}
void AExpeditionPlayerController::Click()
{
    if (!IsValid(BattleView)) { return; }
    bKeyboardCursor = false;
    float MouseX = 0, MouseY = 0;
    int32 ViewWidth = 0, ViewHeight = 0;
    GetViewportSize(ViewWidth, ViewHeight);
    const float Scale = FMath::Max(0.5f, ViewWidth / HUDDesignWidth);
    const bool bHasMouse = GetMousePosition(MouseX, MouseY);
    MouseX /= Scale; MouseY /= Scale;
    if (bHasMouse && MouseX < PartyPanelWidth && MouseY >= PartyCardsTop && MouseY < PartyCardsTop + PartyCardCount * PartyCardHeight)
    {
        SelectPartyIndex(FMath::FloorToInt((MouseY - PartyCardsTop) / PartyCardHeight)); return;
    }
    if (bHasMouse && MouseX >= 260 && MouseX < 860 && MouseY >= 90 && MouseY < 116)
    {
        if (MouseX < 410) { Save(); } else if (MouseX < 560) { Resume(); } else if (MouseX < 710) { NewRun(); } else { History(); }
        return;
    }
    FIntPoint Cell;
    if (!HoverCell(Cell)) { return; }
    ActAtCell(Cell);
}
void AExpeditionPlayerController::ActAtCell(FIntPoint Cell)
{
    for (const FCombatantState& Unit : BattleView->Rules().Snapshot().Encounter.Combatants)
    {
        if (Unit.Cell != Cell) { continue; }
        if (Unit.Side == ECombatSide::Party && SelectedAction == ECombatCommandType::Attack) { SelectedId = Unit.Id; }
        else { BattleView->Execute({SelectedAction, SelectedId, {}, Unit.Id}); }
        return;
    }
    if (SelectedAction == ECombatCommandType::Attack) { BattleView->Execute({ECombatCommandType::Move, SelectedId, Cell}); }
    else { BattleView->Execute({SelectedAction, SelectedId, {}, INDEX_NONE}); }
}
void AExpeditionPlayerController::AttackMode() { SelectedAction = ECombatCommandType::Attack; }
void AExpeditionPlayerController::RescueMode() { SelectedAction = ECombatCommandType::Rescue; }
void AExpeditionPlayerController::PotionMode() { SelectedAction = ECombatCommandType::Potion; }
void AExpeditionPlayerController::RoleMode()
{
    if (!IsValid(BattleView)) { return; }
    const FCombatantState* Unit = BattleView->Rules().Find(SelectedId);
    if (!Unit) { return; }
    SelectedAction = Unit->AdventurerRole == EAdventurerRole::Fighter ? ECombatCommandType::SecondWind :
        Unit->AdventurerRole == EAdventurerRole::Cleric ? ECombatCommandType::CureWounds :
        Unit->AdventurerRole == EAdventurerRole::Wizard ? ECombatCommandType::MagicMissile : ECombatCommandType::Attack;
}
void AExpeditionPlayerController::Loot() { if (IsValid(BattleView)) { BattleView->ClaimReward(SelectedId); } }
void AExpeditionPlayerController::FirstRoute() { if (IsValid(BattleView)) { BattleView->ChooseRoute(0); LastActiveId = INDEX_NONE; } }
void AExpeditionPlayerController::SecondRoute() { if (IsValid(BattleView)) { BattleView->ChooseRoute(1); LastActiveId = INDEX_NONE; } }
void AExpeditionPlayerController::Camp() { if (IsValid(BattleView)) { BattleView->Recover(true); } }
void AExpeditionPlayerController::SkipCamp() { if (IsValid(BattleView)) { BattleView->Recover(false); } }
void AExpeditionPlayerController::NewRun() { if (IsValid(BattleView)) { BattleView->NewRun(); } }
void AExpeditionPlayerController::Save() { if (IsValid(BattleView)) { BattleView->Save(); } }
void AExpeditionPlayerController::Resume() { if (IsValid(BattleView)) { BattleView->Resume(); } }
void AExpeditionPlayerController::History() { if (IsValid(BattleView)) { BattleView->ToggleRecords(); } }
void AExpeditionPlayerController::OlderRecords() { if (IsValid(BattleView)) { BattleView->PageRecords(1); } }
void AExpeditionPlayerController::NewerRecords() { if (IsValid(BattleView)) { BattleView->PageRecords(-1); } }
FString AExpeditionPlayerController::ActionHelp() const
{
    return FCombatBattle::DescribeAction(SelectedAction);
}
void AExpeditionPlayerController::EndTurn() { if (IsValid(BattleView)) { BattleView->Execute({ECombatCommandType::EndTurn, SelectedId}); } }
void AExpeditionPlayerController::RotateLeft() { if (IsValid(BattleView)) { BattleView->Orbit(-30); } }
void AExpeditionPlayerController::RotateRight() { if (IsValid(BattleView)) { BattleView->Orbit(30); } }
void AExpeditionPlayerController::ZoomIn() { if (IsValid(BattleView)) { BattleView->Zoom(-100); } }
void AExpeditionPlayerController::ZoomOut() { if (IsValid(BattleView)) { BattleView->Zoom(100); } }
void AExpeditionPlayerController::SelectOne() { SelectPartyIndex(0); }
void AExpeditionPlayerController::SelectTwo() { SelectPartyIndex(1); }
void AExpeditionPlayerController::SelectThree() { SelectPartyIndex(2); }
void AExpeditionPlayerController::SelectFour() { SelectPartyIndex(3); }

AExpeditionGameMode::AExpeditionGameMode()
{
    PlayerControllerClass = AExpeditionPlayerController::StaticClass();
    HUDClass = AExpeditionHUD::StaticClass();
    DefaultPawnClass = nullptr;
}

void AExpeditionHUD::Label(const FString& Text, float X, float Y, FLinearColor Color, float Scale)
{
    float TextWidth = 0, TextHeight = 0;
    GetTextSize(Text, TextWidth, TextHeight, GEngine->GetMediumFont(), Scale);
    const float Available = Canvas ? Canvas->SizeX / HUDScale - X - 14 : TextWidth;
    const float Fit = TextWidth > Available && TextWidth > 0 ? Available / TextWidth : 1.0f;
    DrawText(Text, Color, X * HUDScale, Y * HUDScale, GEngine->GetMediumFont(), Scale * Fit * HUDScale);
}
void AExpeditionHUD::Panel(FLinearColor Color, float X, float Y, float Width, float Height)
{
    DrawRect(Color, X * HUDScale, Y * HUDScale, Width * HUDScale, Height * HUDScale);
}
void AExpeditionHUD::OutlineCell(APlayerController* Controller, const AExpeditionBattleView* View, FIntPoint Cell, FLinearColor Color, float Thickness)
{
    const FVector Center = View->CellPosition(Cell) + FVector(0, 0, 3);
    const FVector Offsets[] = {{-50,-50,0}, {50,-50,0}, {50,50,0}, {-50,50,0}};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        FVector2D A, B;
        if (Controller->ProjectWorldLocationToScreen(Center + Offsets[Index], A) &&
            Controller->ProjectWorldLocationToScreen(Center + Offsets[(Index + 1) % 4], B))
        { DrawLine(A.X, A.Y, B.X, B.Y, Color, Thickness); }
    }
}

void AExpeditionHUD::DrawHUD()
{
    Super::DrawHUD();
    AExpeditionPlayerController* Controller = Cast<AExpeditionPlayerController>(GetOwningPlayerController());
    if (!Controller || !IsValid(Controller->GetBattleView())) { return; }
    HUDScale = FMath::Max(0.5f, Canvas->SizeX / HUDDesignWidth);
    const AExpeditionBattleView* View = Controller->GetBattleView();
    const FCombatBattle& Rules = View->Rules();
    const FCombatSnapshot& Snapshot = Rules.Snapshot();
    const FExpeditionProgress& Run = View->Progress();
    const bool bCombat = View->HasRun() && Run.Phase == EExpeditionPhase::Combat && !View->ShowingRecords();
    const FCombatantState* Active = Rules.Find(Rules.ActiveId());
    const FCombatantState* Selected = Rules.Find(Controller->Selection());
    Panel(FLinearColor(0.025f, 0.04f, 0.055f, 0.94f), 8, 8, PartyPanelWidth - 8, 495);
    Label(TEXT("EXPEDITION"), 20, 18, FLinearColor(1, 0.8f, 0.4f), 1.15f);
    Label(bCombat ? FString::Printf(TEXT("ROUND %d"), Snapshot.Round) : TEXT("EXPEDITION STATUS"), 20, 44);
    Label(bCombat && Active ? TEXT("TURN: ") + Active->Name : TEXT("Choose the next step"), 20, 65, FLinearColor::Yellow);
    int32 PartyIndex = 0;
    for (const FCombatantState& Unit : Snapshot.Encounter.Combatants)
    {
        if (Unit.Side == ECombatSide::Party)
        {
            const float Y = PartyCardsTop + PartyIndex * PartyCardHeight;
            if (Selected && Selected->Id == Unit.Id) { Panel(FLinearColor(0.15f, 0.24f, 0.3f, 1), 14, Y, PartyPanelWidth - 20, PartyCardHeight - 5); }
            Label(FString::Printf(TEXT("[%d] %s / %s"), ++PartyIndex, *Unit.Name, *Unit.Role), 20, Y + 4, PartyColor(Unit.Id));
            const FString Fate = Unit.Life == ECombatLife::Dead ? TEXT("DEAD - permanent") : Unit.Life == ECombatLife::Downed ? FString::Printf(TEXT("DOWNED: %d turns"), Unit.RescueTurnsLeft) : FString::Printf(TEXT("HP %d/%d  AC %d"), Unit.HP, Unit.MaxHP, Unit.ArmorClass);
            Label(Fate, 20, Y + 24, Unit.Life == ECombatLife::Conscious ? FLinearColor::White : FLinearColor(1,0.5f,0.3f));
            Label(FString::Printf(TEXT("Wind %d | Slots %d | Pot %d | Gear +%d"), Unit.SecondWindUses, Unit.SpellSlots, Unit.Potions, Unit.WeaponRank),20,Y+44,FLinearColor(0.7f,0.85f,0.9f),0.85f);
        }
        FVector2D Screen;
        if (Controller->ProjectWorldLocationToScreen(View->CellPosition(Unit.Cell) + FVector(0, 0, 90), Screen))
        {
            Label(FString::Printf(TEXT("%s %d/%d"), *Unit.Name, Unit.HP, Unit.MaxHP), Screen.X / HUDScale - 38, Screen.Y / HUDScale,
                Unit.HP == 0 ? FLinearColor(0.6f,0.6f,0.6f) : Unit.Side == ECombatSide::Party ? PartyColor(Unit.Id) : FLinearColor(1,0.45f,0.3f));
        }
        if (Active && Unit.Id == Active->Id) { OutlineCell(Controller, View, Unit.Cell, FLinearColor::Yellow, 4); }
    }
    Label(TEXT("Arrows+Enter / Click: target"), 20, 372);
    Label(TEXT("1-4: select | SPACE: end turn"), 20, 390);
    Label(TEXT("A: attack/move | F: role action"), 20, 408);
    Label(TEXT("R: rescue | P: potion"), 20, 426);
    Label(TEXT("Q/E: rotate | Wheel: zoom"), 20, 444);
    Label(TEXT("Red: legal target | Cyan: move"), 20, 462, FLinearColor(0.25f,0.9f,1));
    Label(TEXT("Z/X: route | L: loot | C/V: camp"), 20, 480);
    const TArray<FIntPoint> Moves = bCombat ? Rules.LegalMoves(Controller->Selection()) : TArray<FIntPoint>();
    const TArray<int32> Targets = bCombat ? Rules.LegalTargets(Controller->Selection(), Controller->Action()) : TArray<int32>();
    for (FIntPoint Cell : Moves) { OutlineCell(Controller, View, Cell, FLinearColor(0.15f,0.8f,0.9f), 1.5f); }
    for (int32 Id : Targets) { OutlineCell(Controller, View, Rules.Find(Id)->Cell, FLinearColor(1,0.18f,0.12f), 3); }
    if (Selected) { OutlineCell(Controller, View, Selected->Cell, FLinearColor::White, 2); }
    FIntPoint KeyboardCursor;
    if (Controller->KeyboardCell(KeyboardCursor)) { OutlineCell(Controller, View, KeyboardCursor, FLinearColor(1, 0.6f, 0), 5); }
    float Width = Canvas->SizeX / HUDScale;
    float Height = Canvas->SizeY / HUDScale;
    Panel(FLinearColor(0.025f, 0.04f, 0.055f, 0.94f), 8, Height - 215, Width - 16, 207);
    FString Preview;
    if (!bCombat)
    {
        if (!View->HasRun()) { Preview = TEXT("N: begin a fresh expedition | O: resume the latest checkpoint | H: inspect records"); }
        else switch (Run.Phase)
        {
        case EExpeditionPhase::Route: Preview = TEXT("Choose Z: guarded route or X: dangerous route. Offers stay fixed until chosen."); break;
        case EExpeditionPhase::Reward: Preview = TEXT("ENCOUNTER WON | Unrescued allies are DEAD. Select survivor (1-4), L: claim displayed loot."); break;
        case EExpeditionPhase::Recovery: Preview = TEXT("ONE RECOVERY | C: recover survivors | V: skip permanently. Then choose a route."); break;
        case EExpeditionPhase::Victory: Preview = TEXT("ARTIFACT RECOVERED | Expedition complete: three victories. N: fresh run, no permanent power."); break;
        case EExpeditionPhase::Defeat: Preview = TEXT("PARTY WIPE | Expedition lost. Deaths are permanent. N: fresh party and a new seed."); break;
        default: break;
        }
    }
    else if (Selected && Active && Selected->Id == Active->Id)
    {
        Preview = FString::Printf(TEXT("%s | Move %d | Action %s | Range %d cells | d20 %+d, damage d%d +%d"),
            *Selected->Name, Snapshot.MovementLeft, Snapshot.bActionAvailable ? TEXT("READY") : TEXT("SPENT"), Selected->Range,
            Selected->AttackBonus, Selected->DamageDie, Selected->DamageBonus + Selected->WeaponRank);
        Preview += Snapshot.bBonusActionAvailable ? TEXT(" | Bonus READY") : TEXT(" | Bonus SPENT");
        FIntPoint Hover;
        if (Controller->HoverCell(Hover))
        {
            if (Moves.Contains(Hover)) { Preview += TEXT(" | Click cyan cell to move"); }
            for (int32 Id : Targets)
            {
                const FCombatantState* Target = Rules.Find(Id);
                if (Target->Cell == Hover) { Preview += FString::Printf(TEXT(" | TARGET %s (AC %d)"), *Target->Name, Target->ArmorClass); }
            }
        }
    }
    else { Preview = TEXT("Inspecting party. Select the yellow active adventurer to act; enemies play automatically."); }
    FIntPoint Cursor;
    if (bCombat && Controller->KeyboardCell(Cursor)) { Preview += FString::Printf(TEXT(" | Cursor %d,%d"), Cursor.X, Cursor.Y); }
    Label(bCombat ? Controller->ActionHelp() : Run.Phase == EExpeditionPhase::Recovery ? FExpeditionRun::RecoveryDescription() : TEXT("Objective: recover the Waystone in combat 3 | Z/X route, L loot, C/V camp, N new run at ending"), 20, Height - 206, FLinearColor(0.3f,0.9f,1), 1.0f);
    Label(Preview, 20, Height - 182, FLinearColor::Yellow, 1.05f);
    float Y = Height - 145;
    for (const FString& Message : View->Messages()) { Label(Message, 20, Y); Y += 22; }
    FString Initiative = TEXT("Initiative: ");
    for (int32 Id : Snapshot.TurnOrder)
    {
        const FCombatantState* Unit = Rules.Find(Id);
        Initiative += FString::Printf(TEXT("%s%s(%d)  "), Id == Rules.ActiveId() ? TEXT(">") : TEXT(""), *Unit->Name, Unit->Initiative);
    }
    if (bCombat) { Label(Initiative, 260, 18, FLinearColor::White, 0.85f); }
    Label(FString::Printf(TEXT("%s | Seed %d | Victories %d/3 | Camp %s"), *Run.RunId.Left(8), Run.Seed, Run.EncountersCompleted,
        Run.bCampUsed ? TEXT("USED") : Run.bCampResolved ? TEXT("SKIPPED") : TEXT("AVAILABLE after fight 1")), 260, 42, FLinearColor(1,0.8f,0.4f));
    if (!Run.History.IsEmpty())
    {
        const auto& Record = Run.History.Last();
        Label(FString::Printf(TEXT("Chosen: %s | %s | %d survivors"), *Record.Route.Name,
            Record.Outcome == EBattlePhase::Victory ? TEXT("WON") : Record.Outcome == EBattlePhase::Defeat ? TEXT("LOST") : TEXT("IN PROGRESS"), Record.Survivors), 260, 72, FLinearColor(0.7f,0.85f,0.9f), 0.9f);
    }
    Panel(FLinearColor(0.08f,0.16f,0.2f,0.98f), 255, 90, Width - 265, 26);
    Label(TEXT("[K] Save"), 265, 94, FLinearColor(0.3f,0.9f,1), 0.9f);
    Label(TEXT("[O] Resume"), 415, 94, FLinearColor(0.3f,0.9f,1), 0.9f);
    Label(TEXT("[N] New run"), 565, 94, FLinearColor(0.3f,0.9f,1), 0.9f);
    Label(TEXT("[H] Records"), 715, 94, FLinearColor(0.3f,0.9f,1), 0.9f);
    if (!View->HasRun())
    {
        Panel(FLinearColor(0.025f,0.04f,0.055f,0.97f), 255, 120, Width - 265, 110);
        Label(TEXT("RECOVER THE WAYSTONE"), 275, 136, FLinearColor(1,0.8f,0.4f), 1.2f);
        Label(TEXT("N: Start fresh with four adventurers. O: Continue your saved expedition."), 275, 172);
        Label(TEXT("Progress saves after every accepted action. Deaths and resolved outcomes persist."), 275, 198, FLinearColor(0.7f,0.85f,0.9f),0.9f);
    }
    else if (Run.Phase == EExpeditionPhase::Route)
    {
        Panel(FLinearColor(0.025f,0.04f,0.055f,0.97f), 255, 120, Width - 265, 152);
        for (int32 Index = 0; Index < Run.Offers.Num(); ++Index)
        {
            const auto& Offer = Run.Offers[Index];
            Label((Index == 0 ? TEXT("[Z] ") : TEXT("[X] ")) + Offer.Name, 270, 128 + Index * 68, FLinearColor(1,0.8f,0.4f));
            Label(Offer.Preview, 270, 152 + Index * 68, FLinearColor::White, 0.9f);
        }
    }
    else if (Run.Phase == EExpeditionPhase::Reward)
    {
        Panel(FLinearColor(0.025f,0.04f,0.055f,0.97f), 255, 120, Width - 265, 44);
        Label(FString::Printf(TEXT("LOOT: +1 gear rank / +%d potions | Select survivor + L"), Run.History.Last().Route.RewardPotions), 270, 128, FLinearColor(1,0.8f,0.4f));
    }
    if (View->ShowingRecords())
    {
        Panel(FLinearColor(0.025f,0.04f,0.055f,0.99f), 255, 120, Width - 265, FMath::Max(160.0f, Height - 350));
        Label(TEXT("ENDING RECORDS | H: return | PgUp/PgDn: older/newer | No combat power"), 275, 134, FLinearColor(1,0.8f,0.4f));
        const auto& Records = View->Records();
        Label(Records.IsEmpty() ? TEXT("No completed expeditions yet.") : FString::Printf(TEXT("%d completed expeditions | Showing %d-%d (newest first)"), Records.Num(), View->RecordOffset()+1, FMath::Min(Records.Num(), View->RecordOffset()+View->RecordsPerPage)), 275, 164);
        float Row = 196;
        for (int32 I=Records.Num()-1-View->RecordOffset(); I>=FMath::Max(0, Records.Num()-View->RecordOffset()-View->RecordsPerPage); --I)
        {
            const auto& R=Records[I];
            Label(FString::Printf(TEXT("%s | Seed %d | %s"), *R.RunId.Left(8), R.Seed, *R.Summary), 275, Row, FLinearColor(0.8f,0.9f,1),0.9f); Row += 28;
        }
    }

}
