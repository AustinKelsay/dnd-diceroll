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

namespace
{
constexpr float CellSize = 110.0f;
constexpr float HUDDesignWidth = 1280.0f;
constexpr float PartyPanelWidth = 245.0f;
constexpr float PartyCardsTop = 100.0f;
constexpr float PartyCardHeight = 60.0f;
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
    FString Error;
    if (!Battle.Start(FCombatBattle::TutorialEncounter(), 20261030, Error))
    {
        UE_LOG(LogTemp, Error, TEXT("Battle startup failed: %s"), *Error);
        Feedback.Add(Error);
        return;
    }
    // The starter fixture has no baked lighting. Runtime presentation uses dynamic lights.
    for (TActorIterator<ALight> Light(GetWorld()); Light; ++Light)
    {
        if (ULightComponent* Component = Light->GetLightComponent()) { Component->SetMobility(EComponentMobility::Movable); }
    }
    const FCombatEncounter& Encounter = Battle.Snapshot().Encounter;
    for (int32 Y = 0; Y < Encounter.Height; ++Y)
    {
        for (int32 X = 0; X < Encounter.Width; ++X)
        {
            const FIntPoint Cell(X, Y);
            MakeMesh(Cube, CellPosition(Cell) - FVector(0, 0, 14), FVector(1.04f, 1.04f, 0.2f),
                (X + Y) % 2 == 0 ? FLinearColor(0.17f, 0.24f, 0.28f) : FLinearColor(0.23f, 0.31f, 0.34f));
            if (Encounter.BlockedCells.Contains(Cell))
            {
                MakeMesh(Cube, CellPosition(Cell) + FVector(0, 0, 35), FVector(0.9f, 0.9f, 0.8f), FLinearColor(0.32f, 0.27f, 0.22f));
            }
        }
    }
    for (const FCombatantState& Unit : Encounter.Combatants)
    {
        UnitMeshes.Add(MakeMesh(Cylinder, CellPosition(Unit.Cell), FVector(0.55f, 0.55f, 0.75f),
            Unit.Side == ECombatSide::Party ? PartyColor(Unit.Id) : FLinearColor(0.8f, 0.17f, 0.12f)));
    }
    Feedback.Add(TEXT("Briar crossing | Defeat the raiders. Select the active adventurer to act."));
    RefreshUnits(); UpdateCamera();
}

FVector AExpeditionBattleView::CellPosition(FIntPoint Cell) const
{
    return FVector(Cell.X * CellSize, Cell.Y * CellSize, 35.0f);
}

void AExpeditionBattleView::RefreshUnits()
{
    const TArray<FCombatantState>& Units = Battle.Snapshot().Encounter.Combatants;
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
    const FCombatantState* Unit = Battle.Find(Command.ActorId);
    if (!Unit || Unit->Side != ECombatSide::Party) { return; }
    Record(Battle.Submit(Command));
    RefreshUnits();
}

void AExpeditionBattleView::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const FCombatantState* Active = Battle.Find(Battle.ActiveId());
    if (Battle.Snapshot().Phase != EBattlePhase::Fighting || !Active || Active->Side != ECombatSide::Enemy) { EnemyDelay = 0; return; }
    EnemyDelay += DeltaSeconds;
    if (EnemyDelay >= 1.0f)
    {
        for (const FCombatResult& Result : Battle.PlayEnemyTurn()) { Record(Result); }
        RefreshUnits(); EnemyDelay = 0;
    }
}

void AExpeditionBattleView::UpdateCamera()
{
    const FVector Focus((Battle.Snapshot().Encounter.Width - 1) * CellSize / 2,
        (Battle.Snapshot().Encounter.Height - 1) * CellSize / 2 - 100, 0);
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
    return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Battle.Snapshot().Encounter.Width && Cell.Y < Battle.Snapshot().Encounter.Height;
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
    InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AExpeditionPlayerController::ConfirmCell);
}

void AExpeditionPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (!IsValid(BattleView)) { return; }
    const int32 ActiveId = BattleView->Rules().ActiveId();
    if (ActiveId != LastActiveId)
    {
        const FCombatantState* Active = BattleView->Rules().Find(ActiveId);
        if (Active && Active->Side == ECombatSide::Party) { SelectedId = ActiveId; GridCursor = Active->Cell; }
        LastActiveId = ActiveId;
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
    FIntPoint Cell;
    if (!HoverCell(Cell)) { return; }
    ActAtCell(Cell);
}
void AExpeditionPlayerController::ActAtCell(FIntPoint Cell)
{
    for (const FCombatantState& Unit : BattleView->Rules().Snapshot().Encounter.Combatants)
    {
        if (Unit.Cell != Cell) { continue; }
        if (Unit.Side == ECombatSide::Party) { SelectedId = Unit.Id; }
        else { BattleView->Execute({ECombatCommandType::Attack, SelectedId, {}, Unit.Id}); }
        return;
    }
    BattleView->Execute({ECombatCommandType::Move, SelectedId, Cell});
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
    DrawText(Text, Color, X * HUDScale, Y * HUDScale, GEngine->GetMediumFont(), Scale * HUDScale);
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
    const FCombatantState* Active = Rules.Find(Rules.ActiveId());
    const FCombatantState* Selected = Rules.Find(Controller->Selection());
    Panel(FLinearColor(0.025f, 0.04f, 0.055f, 0.94f), 8, 8, PartyPanelWidth - 8, 470);
    Label(TEXT("EXPEDITION"), 20, 18, FLinearColor(1, 0.8f, 0.4f), 1.15f);
    Label(FString::Printf(TEXT("ROUND %d"), Snapshot.Round), 20, 44);
    Label(Active ? TEXT("TURN: ") + Active->Name : TEXT("No active turn"), 20, 65, FLinearColor::Yellow);
    int32 PartyIndex = 0;
    for (const FCombatantState& Unit : Snapshot.Encounter.Combatants)
    {
        if (Unit.Side == ECombatSide::Party)
        {
            const float Y = PartyCardsTop + PartyIndex * PartyCardHeight;
            if (Selected && Selected->Id == Unit.Id) { Panel(FLinearColor(0.15f, 0.24f, 0.3f, 1), 14, Y, PartyPanelWidth - 20, PartyCardHeight - 5); }
            Label(FString::Printf(TEXT("[%d] %s / %s"), ++PartyIndex, *Unit.Name, *Unit.Role), 20, Y + 4, PartyColor(Unit.Id));
            Label(FString::Printf(TEXT("HP %d/%d  AC %d%s"), Unit.HP, Unit.MaxHP, Unit.ArmorClass, Unit.HP == 0 ? TEXT("  INACTIVE") : TEXT("")), 20, Y + 25);
        }
        FVector2D Screen;
        if (Controller->ProjectWorldLocationToScreen(View->CellPosition(Unit.Cell) + FVector(0, 0, 90), Screen))
        {
            Label(FString::Printf(TEXT("%s %d/%d"), *Unit.Name, Unit.HP, Unit.MaxHP), Screen.X / HUDScale - 38, Screen.Y / HUDScale,
                Unit.HP == 0 ? FLinearColor(0.6f,0.6f,0.6f) : Unit.Side == ECombatSide::Party ? PartyColor(Unit.Id) : FLinearColor(1,0.45f,0.3f));
        }
        if (Active && Unit.Id == Active->Id) { OutlineCell(Controller, View, Unit.Cell, FLinearColor::Yellow, 4); }
    }
    Label(TEXT("Click / arrows+Enter: move / attack"), 20, 350);
    Label(TEXT("1-4: select | SPACE: end turn"), 20, 372);
    Label(TEXT("Q / E: rotate | Wheel: zoom"), 20, 394);
    Label(TEXT("Cyan: reachable | Red: legal target"), 20, 416, FLinearColor(0.25f,0.9f,1));
    Label(TEXT("Yellow: turn | White: selection"), 20, 438);
    const TArray<FIntPoint> Moves = Rules.LegalMoves(Controller->Selection());
    const TArray<int32> Targets = Rules.LegalTargets(Controller->Selection());
    for (FIntPoint Cell : Moves) { OutlineCell(Controller, View, Cell, FLinearColor(0.15f,0.8f,0.9f), 1.5f); }
    for (int32 Id : Targets) { OutlineCell(Controller, View, Rules.Find(Id)->Cell, FLinearColor(1,0.18f,0.12f), 3); }
    if (Selected) { OutlineCell(Controller, View, Selected->Cell, FLinearColor::White, 2); }
    FIntPoint KeyboardCursor;
    if (Controller->KeyboardCell(KeyboardCursor)) { OutlineCell(Controller, View, KeyboardCursor, FLinearColor(1, 0.6f, 0), 5); }
    float Width = Canvas->SizeX / HUDScale;
    float Height = Canvas->SizeY / HUDScale;
    Panel(FLinearColor(0.025f, 0.04f, 0.055f, 0.94f), 8, Height - 185, Width - 16, 177);
    FString Preview;
    if (Snapshot.Phase != EBattlePhase::Fighting)
    {
        Preview = Snapshot.Phase == EBattlePhase::Victory ? TEXT("VICTORY - The crossing is clear.") : TEXT("DEFEAT - The party can no longer fight.");
    }
    else if (Selected && Active && Selected->Id == Active->Id)
    {
        Preview = FString::Printf(TEXT("%s | Move %d | Attack %s | Range %d cells | d20 %+d, damage d%d +%d"),
            *Selected->Name, Snapshot.MovementLeft, Snapshot.bActionAvailable ? TEXT("READY") : TEXT("SPENT"), Selected->Range,
            Selected->AttackBonus, Selected->DamageDie, Selected->DamageBonus);
        FIntPoint Hover;
        if (Controller->HoverCell(Hover))
        {
            if (Moves.Contains(Hover)) { Preview += TEXT(" | Click cyan cell to move"); }
            for (int32 Id : Targets)
            {
                const FCombatantState* Target = Rules.Find(Id);
                if (Target->Cell == Hover) { Preview += FString::Printf(TEXT(" | ATTACK %s vs AC %d"), *Target->Name, Target->ArmorClass); }
            }
        }
    }
    else { Preview = TEXT("Inspecting party. Select the yellow active adventurer to act; enemies play automatically."); }
    Label(Preview, 20, Height - 176, FLinearColor::Yellow, 1.05f);
    float Y = Height - 145;
    for (const FString& Message : View->Messages()) { Label(Message, 20, Y); Y += 22; }
    FString Initiative = TEXT("Initiative: ");
    for (int32 Id : Snapshot.TurnOrder)
    {
        const FCombatantState* Unit = Rules.Find(Id);
        Initiative += FString::Printf(TEXT("%s%s(%d)  "), Id == Rules.ActiveId() ? TEXT(">") : TEXT(""), *Unit->Name, Unit->Initiative);
    }
    Label(Initiative, 260, 18, FLinearColor::White);
}
