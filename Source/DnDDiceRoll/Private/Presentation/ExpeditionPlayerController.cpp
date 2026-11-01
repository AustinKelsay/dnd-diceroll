#include "Presentation/ExpeditionPlayerController.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/ExpeditionHUD.h"
#include "Components/InputComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Party/BuildCatalog.h"

void AExpeditionPlayerController::BeginPlay()
{
    Super::BeginPlay();
    Preset();
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
    auto Bind = [this](FKey Key, void (AExpeditionPlayerController::*Handler)())
    { InputComponent->BindKey(Key, IE_Pressed, this, Handler); };
    Bind(EKeys::LeftMouseButton, &AExpeditionPlayerController::Click);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AExpeditionPlayerController::BeginDrag);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AExpeditionPlayerController::EndDrag);
    Bind(EKeys::SpaceBar, &AExpeditionPlayerController::EndTurn);
    Bind(EKeys::Q, &AExpeditionPlayerController::RotateLeft);
    Bind(EKeys::E, &AExpeditionPlayerController::RotateRight);
    Bind(EKeys::MouseScrollUp, &AExpeditionPlayerController::ZoomIn);
    Bind(EKeys::MouseScrollDown, &AExpeditionPlayerController::ZoomOut);
    Bind(EKeys::One, &AExpeditionPlayerController::SelectOne);
    Bind(EKeys::Two, &AExpeditionPlayerController::SelectTwo);
    Bind(EKeys::Three, &AExpeditionPlayerController::SelectThree);
    Bind(EKeys::Four, &AExpeditionPlayerController::SelectFour);
    Bind(EKeys::Up, &AExpeditionPlayerController::CursorUp);
    Bind(EKeys::Down, &AExpeditionPlayerController::CursorDown);
    Bind(EKeys::Left, &AExpeditionPlayerController::CursorLeft);
    Bind(EKeys::Right, &AExpeditionPlayerController::CursorRight);
    Bind(EKeys::G, &AExpeditionPlayerController::Preset);
    Bind(EKeys::A, &AExpeditionPlayerController::AttackMode);
    Bind(EKeys::B, &AExpeditionPlayerController::BattleMaster);
    Bind(EKeys::J, &AExpeditionPlayerController::Champion);
    Bind(EKeys::M, &AExpeditionPlayerController::ManeuverMode);
    Bind(EKeys::F, &AExpeditionPlayerController::RoleMode);
    Bind(EKeys::R, &AExpeditionPlayerController::RescueMode);
    Bind(EKeys::P, &AExpeditionPlayerController::PotionMode);
    Bind(EKeys::L, &AExpeditionPlayerController::Loot);
    Bind(EKeys::Z, &AExpeditionPlayerController::FirstRoute);
    Bind(EKeys::X, &AExpeditionPlayerController::SecondRoute);
    Bind(EKeys::C, &AExpeditionPlayerController::Camp);
    Bind(EKeys::V, &AExpeditionPlayerController::SkipCamp);
    Bind(EKeys::N, &AExpeditionPlayerController::NewRun);
    Bind(EKeys::K, &AExpeditionPlayerController::Save);
    Bind(EKeys::O, &AExpeditionPlayerController::Resume);
    Bind(EKeys::H, &AExpeditionPlayerController::History);
    Bind(EKeys::PageUp, &AExpeditionPlayerController::OlderRecords);
    Bind(EKeys::PageDown, &AExpeditionPlayerController::NewerRecords);
    Bind(EKeys::Enter, &AExpeditionPlayerController::ConfirmCell);
    Bind(EKeys::Tab, &AExpeditionPlayerController::Skip);
    Bind(EKeys::T, &AExpeditionPlayerController::Speed);
    // Not F1: development builds reserve F1-F4 for engine view modes (F1 is wireframe).
    Bind(EKeys::Slash, &AExpeditionPlayerController::ToggleHelp);
    Bind(EKeys::Escape, &AExpeditionPlayerController::CloseOverlays);
}

void AExpeditionPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (!IsValid(BattleView)) { return; }
    const FCombatBattle& Rules = BattleView->Rules();
    if (LastRevision != BattleView->PresentationRevision())
    {
        LastRevision = BattleView->PresentationRevision(); LastActiveId = INDEX_NONE;
        SelectedId = INDEX_NONE; bKeyboardCursor = false; SelectedAction = ECombatCommandType::Attack;
        SelectPartyIndex(0); GridCursor = {1, 1};
    }
    const int32 ActiveId = Rules.ActiveId();
    if (ActiveId != LastActiveId)
    {
        const FCombatantState* Active = Rules.Find(ActiveId);
        if (Active && Active->Side == ECombatSide::Party) { SelectedId = ActiveId; GridCursor = Active->Cell; }
        LastActiveId = ActiveId;
        SelectedAction = ECombatCommandType::Attack;
    }
    // Loot goes to a living survivor; keep the selection on one.
    if (BattleView->HasRun() && BattleView->Progress().Phase == EExpeditionPhase::Reward)
    {
        const FCombatantState* Selected = Rules.Find(SelectedId);
        if (!Selected || Selected->Life != ECombatLife::Conscious)
        {
            for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants)
            { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { SelectedId = Unit.Id; break; } }
        }
    }
    float MouseX = 0, MouseY = 0;
    const bool bHasMouse = GetMousePosition(MouseX, MouseY);
    if (bDragging && bHasMouse)
    {
        BattleView->Orbit((MouseX - DragX) * 0.35f);
        DragX = MouseX;
    }

    FBoardHighlights Highlights;
    Highlights.SelectedId = SelectedId;
    HoverUnitId = HoverTargetId = INDEX_NONE;
    Path.Reset();
    FIntPoint Hover;
    const AExpeditionHUD* Hud = Cast<AExpeditionHUD>(GetHUD());
    const bool bOverPanel = !bKeyboardCursor && bHasMouse && Hud && Hud->CoversBoard(FVector2D(MouseX, MouseY));
    const bool bHover = !bOverPanel && !bShowHelp && HoverCell(Hover);
    if (bHover)
    {
        Highlights.bHasHover = true;
        Highlights.Hover = Hover;
        for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants)
        { if (Unit.Cell == Hover && !(Unit.Side == ECombatSide::Enemy && Unit.Life == ECombatLife::Dead)) { HoverUnitId = Unit.Id; } }
    }
    const int32 ActorId = Actor();
    if (BattleView->InCombat() && !BattleView->IsPresenting() && ActorId == ActiveId)
    {
        if (SelectedAction == ECombatCommandType::Attack) { Highlights.Moves = Rules.LegalMoves(ActorId); }
        Highlights.Targets = Rules.LegalTargets(ActorId, SelectedAction);
        Highlights.bFriendlyTargets = ExpeditionStyle::IsFriendlyAction(SelectedAction, Rules.Find(ActorId));
        if (bHover)
        {
            if (Highlights.Moves.Contains(Hover)) { Path = Rules.PathTo(ActorId, Hover); Highlights.Path = Path; }
            if (Highlights.Targets.Contains(HoverUnitId)) { HoverTargetId = HoverUnitId; Highlights.HoverTargetId = HoverUnitId; }
        }
    }
    TargetIds = Highlights.Targets;
    BattleView->SetHighlights(Highlights);
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

void AExpeditionPlayerController::MoveCursor(FIntPoint ScreenOffset)
{
    if (!AllowsGameplayInput()) { return; }
    if (CanSelectSubclass()) { if (ScreenOffset.Y) { CycleClass(); } else { CycleBuild(); } return; }
    // Arrows follow the screen: map "up" to whichever board axis faces away from the camera.
    const float Radians = FMath::DegreesToRadians(BattleView->CameraYaw());
    const FVector2D Forward(-FMath::Cos(Radians), -FMath::Sin(Radians));
    const FVector2D Right(-Forward.Y, Forward.X);
    const FVector2D Wanted = Forward * ScreenOffset.Y + Right * ScreenOffset.X;
    const FIntPoint Step = FMath::Abs(Wanted.X) >= FMath::Abs(Wanted.Y) ? FIntPoint(FMath::Sign(Wanted.X), 0) : FIntPoint(0, FMath::Sign(Wanted.Y));
    const FCombatEncounter& Encounter = BattleView->Rules().Snapshot().Encounter;
    if (bKeyboardCursor)
    {
        GridCursor.X = FMath::Clamp(GridCursor.X + Step.X, 0, Encounter.Width - 1);
        GridCursor.Y = FMath::Clamp(GridCursor.Y + Step.Y, 0, Encounter.Height - 1);
    }
    bKeyboardCursor = true;
}
void AExpeditionPlayerController::CursorUp() { MoveCursor({0, 1}); }
void AExpeditionPlayerController::CursorDown() { MoveCursor({0, -1}); }
void AExpeditionPlayerController::CursorLeft() { MoveCursor({-1, 0}); }
void AExpeditionPlayerController::CursorRight() { MoveCursor({1, 0}); }
void AExpeditionPlayerController::ConfirmCell() { if (AllowsGameplayInput() && BattleView->InCombat()) { bKeyboardCursor = true; ActAtCell(GridCursor); } }

void AExpeditionPlayerController::SelectPartyIndex(int32 Index)
{
    if (!AllowsGameplayInput()) { return; }
    if (CanSelectSubclass()) { Slot = FMath::Clamp(Index, 0, 3); return; }
    int32 PartyIndex = 0;
    for (const FCombatantState& Unit : BattleView->Rules().Snapshot().Encounter.Combatants)
    {
        if (Unit.Side == ECombatSide::Party && PartyIndex++ == Index) { SelectedId = Unit.Id; return; }
    }
}

void AExpeditionPlayerController::Click()
{
    if (!IsValid(BattleView)) { return; }
    float MouseX = 0, MouseY = 0;
    const bool bHasMouse = GetMousePosition(MouseX, MouseY);
    if (const AExpeditionHUD* Hud = Cast<AExpeditionHUD>(GetHUD()); Hud && bHasMouse)
    {
        FName Button;
        if (Hud->ButtonAt(FVector2D(MouseX, MouseY), Button)) { HandleButton(Button); return; }
        if (Hud->CoversBoard(FVector2D(MouseX, MouseY))) { return; }
    }
    if (bShowHelp || BattleView->ShowingRecords()) { return; }
    bKeyboardCursor = false;
    FIntPoint Cell;
    if (HoverCell(Cell)) { ActAtCell(Cell); }
}

void AExpeditionPlayerController::HandleButton(FName Id)
{
    if (!IsValid(BattleView)) { return; }
    const FString Name = Id.ToString();
    if (!AllowsGameplayInput() && Name != TEXT("Close") && Name != TEXT("Help") && Name != TEXT("Records") && Name != TEXT("Older") && Name != TEXT("Newer")) { return; }
    if (Name == TEXT("ClassNext")) { CycleClass(); return; }
    if (Name == TEXT("BuildNext")) { CycleBuild(); return; }
    if (Name == TEXT("Preset")) { Preset(); return; }
    if (Name.StartsWith(TEXT("Party"))) { SelectPartyIndex(FCString::Atoi(*Name.RightChop(5))); BattleView->PlayCue(EExpeditionCue::Click); return; }
    if (Name == TEXT("Champion")) { Champion(); }
    else if (Name == TEXT("BattleMaster")) { BattleMaster(); }
    else if (Name == TEXT("Maneuver")) { ManeuverMode(); }
    else if (Name == TEXT("Save")) { Save(); }
    else if (Name == TEXT("Resume")) { Resume(); }
    else if (Name == TEXT("Records")) { History(); }
    else if (Name == TEXT("Speed")) { Speed(); }
    else if (Name == TEXT("Help")) { ToggleHelp(); }
    else if (Name == TEXT("Skip")) { Skip(); }
    else if (Name == TEXT("New")) { NewRun(); }
    else if (Name == TEXT("Route0")) { FirstRoute(); }
    else if (Name == TEXT("Route1")) { SecondRoute(); }
    else if (Name == TEXT("Claim")) { Loot(); }
    else if (Name == TEXT("Camp")) { Camp(); }
    else if (Name == TEXT("SkipCamp")) { SkipCamp(); }
    else if (Name == TEXT("ModeAttack")) { AttackMode(); }
    else if (Name == TEXT("ModeRole")) { RoleMode(); }
    else if (Name == TEXT("ModeRescue")) { RescueMode(); }
    else if (Name == TEXT("ModePotion")) { PotionMode(); }
    else if (Name == TEXT("EndTurn")) { EndTurn(); }
    else if (Name == TEXT("Older")) { OlderRecords(); }
    else if (Name == TEXT("Newer")) { NewerRecords(); }
    else if (Name == TEXT("Close")) { CloseOverlays(); }
}

int32 AExpeditionPlayerController::Actor() const
{
    if (!IsValid(BattleView)) { return SelectedId; }
    const FCombatantState* Active = BattleView->Rules().Find(BattleView->Rules().ActiveId());
    return BattleView->InCombat() && Active && Active->Side == ECombatSide::Party ? Active->Id : SelectedId;
}

void AExpeditionPlayerController::ActAtCell(FIntPoint Cell)
{
    if (!AllowsGameplayInput()) { return; }
    const int32 ActorId = Actor();
    for (const FCombatantState& Unit : BattleView->Rules().Snapshot().Encounter.Combatants)
    {
        if (Unit.Cell != Cell || (Unit.Side == ECombatSide::Enemy && Unit.Life == ECombatLife::Dead)) { continue; }
        // Clicking an ally in attack mode inspects them; their card stays highlighted.
        if (Unit.Side == ECombatSide::Party && SelectedAction == ECombatCommandType::Attack) { SelectedId = Unit.Id; BattleView->PlayCue(EExpeditionCue::Click); }
        else { BattleView->Execute({SelectedAction, ActorId, {}, Unit.Id}); }
        return;
    }
    if (SelectedAction == ECombatCommandType::Attack) { BattleView->Execute({ECombatCommandType::Move, ActorId, Cell}); }
    else { BattleView->Execute({SelectedAction, ActorId, {}, INDEX_NONE}); }
}

ECombatCommandType AExpeditionPlayerController::RoleAction() const
{
    return SpecialAction(0);
}
ECombatCommandType AExpeditionPlayerController::SpecialAction(int32 Index) const
{
    const auto* Unit = IsValid(BattleView) ? BattleView->Rules().Find(Actor()) : nullptr;
    const auto Abilities = Unit ? FBuildCatalog::Abilities(*Unit) : TArray<FAbilityFacts>();
    return Abilities.IsValidIndex(Index) ? Abilities[Index].Command : ECombatCommandType::Attack;
}
EFighterSubclass AExpeditionPlayerController::StartupSubclass() const
{
    return Party.Num() == 4 && Party[0].BuildId == TEXT("fighter.battle-master") ? EFighterSubclass::BattleMaster : EFighterSubclass::Champion;
}
void AExpeditionPlayerController::Preset()
{
    if (IsValid(BattleView) && !CanSelectSubclass()) { return; }
    Party.Reset();
    const TCHAR* Names[] = {TEXT("Arden"), TEXT("Nix"), TEXT("Sera"), TEXT("Ivo")};
    const TCHAR* Builds[] = {TEXT("fighter.champion"), TEXT("rogue.base"), TEXT("cleric.base"), TEXT("wizard.base")};
    for (int32 I = 0; I < 4; ++I) { FPartyBuildSelection P; P.Id = I + 1; P.Name = Names[I]; P.BuildId = Builds[I]; Party.Add(P); }
}
void AExpeditionPlayerController::CycleClass()
{
    if (!CanSelectSubclass() || Party.Num() != 4) { return; }
    TArray<FString> Classes;
    for (const auto& B : FBuildCatalog::All()) { Classes.AddUnique(B.Profile.ClassId); }
    const auto* Current = FBuildCatalog::Find(Party[Slot].BuildId);
    const FString Next = Classes[(Classes.IndexOfByKey(Current->Profile.ClassId) + 1) % Classes.Num()];
    for (const auto& B : FBuildCatalog::All()) { if (B.Profile.ClassId == Next) { Party[Slot].BuildId = B.Id; break; } }
}
void AExpeditionPlayerController::CycleBuild()
{
    if (!CanSelectSubclass() || Party.Num() != 4) { return; }
    TArray<FString> Builds;
    const auto* Current = FBuildCatalog::Find(Party[Slot].BuildId);
    for (const auto& B : FBuildCatalog::All()) { if (B.Profile.ClassId == Current->Profile.ClassId) { Builds.Add(B.Id); } }
    Party[Slot].BuildId = Builds[(Builds.IndexOfByKey(Current->Id) + 1) % Builds.Num()];
}

void AExpeditionPlayerController::SetMode(ECombatCommandType Action)
{
    if (!AllowsGameplayInput()) { return; }
    SelectedAction = Action;
    if (IsValid(BattleView)) { BattleView->PlayCue(EExpeditionCue::Click); }
}
bool AExpeditionPlayerController::AllowsGameplayInput() const
{
    return IsValid(BattleView) && !bShowHelp && !BattleView->ShowingRecords();
}
bool AExpeditionPlayerController::CanSelectSubclass() const
{
    if (!AllowsGameplayInput()) { return false; }
    return !BattleView->HasRun() || BattleView->Progress().Phase == EExpeditionPhase::Victory || BattleView->Progress().Phase == EExpeditionPhase::Defeat;
}
void AExpeditionPlayerController::Champion() { if (CanSelectSubclass()) { Party[0].BuildId = TEXT("fighter.champion"); } }
void AExpeditionPlayerController::BattleMaster() { if (CanSelectSubclass()) { Party[0].BuildId = TEXT("fighter.battle-master"); } }
void AExpeditionPlayerController::ManeuverMode() { if (AllowsGameplayInput()) { SetMode(SpecialAction(1)); } }
void AExpeditionPlayerController::AttackMode() { SetMode(ECombatCommandType::Attack); }
void AExpeditionPlayerController::RescueMode() { SetMode(ECombatCommandType::Rescue); }
void AExpeditionPlayerController::PotionMode() { SetMode(ECombatCommandType::Potion); }
void AExpeditionPlayerController::RoleMode() { SetMode(RoleAction()); }
void AExpeditionPlayerController::Loot() { if (AllowsGameplayInput()) { BattleView->ClaimReward(SelectedId); } }
void AExpeditionPlayerController::FirstRoute() { if (AllowsGameplayInput()) { BattleView->ChooseRoute(0); LastActiveId = INDEX_NONE; } }
void AExpeditionPlayerController::SecondRoute() { if (AllowsGameplayInput()) { BattleView->ChooseRoute(1); LastActiveId = INDEX_NONE; } }
void AExpeditionPlayerController::Camp() { if (AllowsGameplayInput()) { BattleView->Recover(true); } }
void AExpeditionPlayerController::SkipCamp() { if (AllowsGameplayInput()) { BattleView->Recover(false); } }
void AExpeditionPlayerController::NewRun() { if (AllowsGameplayInput()) { BattleView->NewRun(Party); } }
void AExpeditionPlayerController::Save() { if (IsValid(BattleView)) { BattleView->Save(); } }
void AExpeditionPlayerController::Resume() { if (AllowsGameplayInput()) { BattleView->Resume(); } }
void AExpeditionPlayerController::History() { if (IsValid(BattleView)) { bShowHelp = false; BattleView->ToggleRecords(); } }
void AExpeditionPlayerController::OlderRecords() { if (IsValid(BattleView)) { BattleView->PageRecords(1); } }
void AExpeditionPlayerController::NewerRecords() { if (IsValid(BattleView)) { BattleView->PageRecords(-1); } }
void AExpeditionPlayerController::EndTurn() { if (AllowsGameplayInput()) { BattleView->Execute({ECombatCommandType::EndTurn, Actor()}); } }
void AExpeditionPlayerController::RotateLeft() { if (IsValid(BattleView)) { BattleView->Orbit(-45); } }
void AExpeditionPlayerController::RotateRight() { if (IsValid(BattleView)) { BattleView->Orbit(45); } }
void AExpeditionPlayerController::ZoomIn() { if (IsValid(BattleView)) { BattleView->Zoom(-120); } }
void AExpeditionPlayerController::ZoomOut() { if (IsValid(BattleView)) { BattleView->Zoom(120); } }
void AExpeditionPlayerController::SelectOne() { SelectPartyIndex(0); }
void AExpeditionPlayerController::SelectTwo() { SelectPartyIndex(1); }
void AExpeditionPlayerController::SelectThree() { SelectPartyIndex(2); }
void AExpeditionPlayerController::SelectFour() { SelectPartyIndex(3); }
void AExpeditionPlayerController::Skip() { if (IsValid(BattleView)) { BattleView->SkipPresentation(); } }
void AExpeditionPlayerController::Speed() { if (IsValid(BattleView)) { BattleView->CycleSpeed(); } }
void AExpeditionPlayerController::ToggleHelp() { bShowHelp = !bShowHelp; if (IsValid(BattleView)) { BattleView->PlayCue(EExpeditionCue::Click); } }
void AExpeditionPlayerController::CloseOverlays()
{
    bShowHelp = false;
    if (IsValid(BattleView) && BattleView->ShowingRecords()) { BattleView->ToggleRecords(); }
}
void AExpeditionPlayerController::BeginDrag()
{
    float MouseX = 0, MouseY = 0;
    bDragging = GetMousePosition(MouseX, MouseY);
    DragX = MouseX;
}
void AExpeditionPlayerController::EndDrag() { bDragging = false; }
