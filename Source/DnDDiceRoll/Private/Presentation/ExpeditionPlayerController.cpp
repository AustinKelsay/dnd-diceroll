#include "Presentation/ExpeditionPlayerController.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/ExpeditionCapture.h"
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
    Capture = FExpeditionCaptureDirector::TryCreate(this, BattleView);
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
    // Return to menu (#132). Not Esc: Esc closes what is open and must never leave a run.
    Bind(EKeys::BackSpace, &AExpeditionPlayerController::RequestReturnToMenu);
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
    ResolvePendingReturn();
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
    const bool bHover = !bOverPanel && AllowsGameplayInput() && HoverCell(Hover);
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
    if (Capture.IsValid()) { Capture->Tick(); }
}

void AExpeditionPlayerController::PreviewAim(bool bAim, FIntPoint Cell)
{
    SelectedAction = ECombatCommandType::Attack;
    bKeyboardCursor = bAim;
    if (bAim) { GridCursor = Cell; }
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
    if (CanSelectSubclass()) { if (ScreenOffset.Y) { StepClass(-ScreenOffset.Y); } else { StepBuild(ScreenOffset.X); } return; }
    if (!BattleView->InCombat()) { return; }
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
uint32 AExpeditionPlayerController::MenuScreenKey() const
{
    // Phase is offset by one so that "no run yet" (0) differs from every phase.
    const uint32 Phase = BattleView->HasRun() ? uint32(BattleView->Progress().Phase) + 1 : 0;
    return uint32(bShowHelp) | uint32(BattleView->ShowingRecords()) << 1 | uint32(ShowingBuilder()) << 2 | uint32(bConfirmReturn) << 3 | Phase << 4;
}
void AExpeditionPlayerController::SaveOverlayFocus() { SavedFocus = Focus; }
void AExpeditionPlayerController::RestoreOverlayFocus()
{
    Focus = IsValid(BattleView) && SavedFocus.Screen == MenuScreenKey() ? SavedFocus : FMenuFocus();
}
void AExpeditionPlayerController::UpdateMenuFocus()
{
    if (!IsValid(BattleView)) { return; }
    // A new screen (phase, builder or overlay) never inherits the previous screen's focus.
    const uint32 Screen = MenuScreenKey();
    if (Focus.Screen != Screen) { Focus = {NAME_None, Screen, false}; }
    if (BattleView->InCombat() && AllowsGameplayInput()) { Focus.Id = NAME_None; return; }
    const auto* Hud = Cast<AExpeditionHUD>(GetHUD());
    const auto Items = Hud ? Hud->MenuButtons() : TArray<FName>();
    if (Focus.bTabChosen && Items.Contains(Focus.Id)) { return; }
    // Focus only controls actually drawn and enabled. Automatic focus is the screen's
    // primary choice; while a result is still playing and that choice is not drawn yet,
    // nothing is focused, so Enter cannot fall through to a system button such as Help.
    FName Preferred = TEXT("Builder");
    if (bConfirmReturn) { Preferred = TEXT("ReturnConfirm"); }
    else if (OverlayOpen()) { Preferred = TEXT("Close"); }
    // With an unfinished checkpoint New Run would be refused, so the start screen's primary choice is Resume.
    else if (ShowingBuilder()) { Preferred = !BattleView->HasRun() && BattleView->HasUnfinishedCheckpoint() ? TEXT("Resume") : TEXT("New"); }
    else
    {
        switch (BattleView->Progress().Phase)
        {
        case EExpeditionPhase::Route: Preferred = TEXT("Route0"); break;
        case EExpeditionPhase::Reward: Preferred = TEXT("Claim"); break;
        case EExpeditionPhase::Recovery: Preferred = TEXT("Camp"); break;
        default: break;
        }
    }
    Focus.Id = Items.Contains(Preferred) ? Preferred : NAME_None;
    Focus.bTabChosen = false;
}

void AExpeditionPlayerController::ConfirmCell()
{
    if (!IsValid(BattleView)) { return; }
    if (!BattleView->InCombat() || !AllowsGameplayInput())
    {
        const auto* Hud = Cast<AExpeditionHUD>(GetHUD());
        if (Hud && Hud->MenuButtons().Contains(Focus.Id)) { HandleButton(Focus.Id); }
        return;
    }
    bKeyboardCursor = true;
    ActAtCell(GridCursor);
}

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
    if (OverlayOpen()) { return; }
    bKeyboardCursor = false;
    FIntPoint Cell;
    if (HoverCell(Cell)) { ActAtCell(Cell); }
}

void AExpeditionPlayerController::HandleButton(FName Id)
{
    if (!IsValid(BattleView)) { return; }
    const FString Name = Id.ToString();
    // The confirmation owns every click and key: only its two choices act.
    if (bConfirmReturn)
    {
        if (Name == TEXT("ReturnConfirm")) { ConfirmReturn(); }
        else if (Name == TEXT("KeepPlaying")) { CloseOverlays(); }
        return;
    }
    if (!AllowsGameplayInput() && Name != TEXT("Close") && Name != TEXT("Help") && Name != TEXT("Records") && Name != TEXT("Older") && Name != TEXT("Newer")) { return; }
    if (Name == TEXT("Builder") && BattleView->HasRun() && (BattleView->Progress().Phase == EExpeditionPhase::Victory || BattleView->Progress().Phase == EExpeditionPhase::Defeat)) { bNextParty = true; return; }
    if (Name.StartsWith(TEXT("Class:")))
    {
        if (!CanSelectSubclass() || Party.Num() != 4) { return; }
        const FString ClassId = Name.RightChop(6);
        const auto* Current = FBuildCatalog::Find(Party[Slot].BuildId);
        if (Current && Current->Profile.ClassId == ClassId) { return; }
        for (const auto& Build : FBuildCatalog::All())
        {
            if (Build.Profile.ClassId == ClassId) { SelectBuild(Build.Id); break; }
        }
        return;
    }
    if (Name.StartsWith(TEXT("Build:"))) { SelectBuild(Name.RightChop(6)); return; }
    if (Name == TEXT("ClassNext")) { CycleClass(); return; }
    if (Name == TEXT("BuildNext")) { CycleBuild(); return; }
    if (Name == TEXT("Preset")) { Preset(); return; }
    if (Name.StartsWith(TEXT("Party"))) { SelectPartyIndex(FCString::Atoi(*Name.RightChop(5))); BattleView->PlayCue(EExpeditionCue::Click); return; }
    if (Name == TEXT("Champion")) { Champion(); }
    else if (Name == TEXT("BattleMaster")) { BattleMaster(); }
    else if (Name == TEXT("Maneuver")) { ManeuverMode(); }
    else if (Name == TEXT("Save")) { Save(); }
    else if (Name == TEXT("Resume")) { Resume(); }
    else if (Name == TEXT("Menu")) { RequestReturnToMenu(); }
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
    const TCHAR* Builds[] = {TEXT("fighter.champion"), TEXT("rogue.thief"), TEXT("cleric.life"), TEXT("wizard.evocation")};
    for (int32 I = 0; I < 4; ++I) { FPartyBuildSelection P; P.Id = I + 1; P.Name = Names[I]; P.BuildId = Builds[I]; Party.Add(P); }
}
void AExpeditionPlayerController::CycleClass() { StepClass(1); }
void AExpeditionPlayerController::StepClass(int32 Direction)
{
    if (!CanSelectSubclass() || Party.Num() != 4) { return; }
    TArray<FString> Classes;
    for (const auto& B : FBuildCatalog::All()) { Classes.AddUnique(B.Profile.ClassId); }
    const auto* Current = FBuildCatalog::Find(Party[Slot].BuildId);
    const FString Next = Classes[(Classes.IndexOfByKey(Current->Profile.ClassId) + Classes.Num() + FMath::Sign(Direction)) % Classes.Num()];
    for (const auto& B : FBuildCatalog::All()) { if (B.Profile.ClassId == Next) { Party[Slot].BuildId = B.Id; break; } }
}
void AExpeditionPlayerController::CycleBuild() { StepBuild(1); }
void AExpeditionPlayerController::StepBuild(int32 Direction)
{
    if (!CanSelectSubclass() || Party.Num() != 4) { return; }
    TArray<FString> Builds;
    const auto* Current = FBuildCatalog::Find(Party[Slot].BuildId);
    for (const auto& B : FBuildCatalog::All()) { if (B.Profile.ClassId == Current->Profile.ClassId) { Builds.Add(B.Id); } }
    Party[Slot].BuildId = Builds[(Builds.IndexOfByKey(Current->Id) + Builds.Num() + FMath::Sign(Direction)) % Builds.Num()];
}

void AExpeditionPlayerController::SetMode(ECombatCommandType Action)
{
    if (!AllowsGameplayInput()) { return; }
    SelectedAction = Action;
    if (IsValid(BattleView)) { BattleView->PlayCue(EExpeditionCue::Click); }
}
bool AExpeditionPlayerController::AllowsGameplayInput() const
{
    return IsValid(BattleView) && !OverlayOpen();
}
bool AExpeditionPlayerController::OverlayOpen() const
{
    return bShowHelp || bConfirmReturn || (IsValid(BattleView) && BattleView->ShowingRecords());
}
bool AExpeditionPlayerController::CanSelectSubclass() const
{
    if (!AllowsGameplayInput()) { return false; }
    return ShowingBuilder();
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
void AExpeditionPlayerController::NewRun()
{
    if (!AllowsGameplayInput()) { return; }
    if (!ShowingBuilder() && (BattleView->Progress().Phase == EExpeditionPhase::Victory || BattleView->Progress().Phase == EExpeditionPhase::Defeat))
    {
        bNextParty = true;
        return;
    }
    BattleView->NewRun(Party);
    if (BattleView->HasRun() && BattleView->Progress().Phase == EExpeditionPhase::Route) { bNextParty = false; bReturnPending = false; }
}
void AExpeditionPlayerController::Save() { if (AllowsGameplayInput()) { BattleView->Save(); } }
void AExpeditionPlayerController::Resume()
{
    if (!AllowsGameplayInput()) { return; }
    const int32 Before = BattleView->PresentationRevision();
    BattleView->Resume();
    if (Before != BattleView->PresentationRevision()) { bNextParty = false; bReturnPending = false; Focus = FMenuFocus(); }
}
void AExpeditionPlayerController::RequestReturnToMenu()
{
    if (!AllowsGameplayInput() || !BattleView->CanReturnToMenu()) { return; }
    if (bReturnPending) { bReturnPending = false; BattleView->PlayCue(EExpeditionCue::Click); return; }
    bReturnPending = true;
    PendingPhase = BattleView->Progress().Phase;
    ResolvePendingReturn();
    // One click per press: leaving plays the view's cue and opening the question plays its own.
    if (bReturnPending) { BattleView->PlayCue(EExpeditionCue::Click); }
}
void AExpeditionPlayerController::ResolvePendingReturn()
{
    if (!bReturnPending || !AllowsGameplayInput()) { return; }
    if (!BattleView->CanReturnToMenu() || BattleView->Progress().Phase != PendingPhase) { bReturnPending = false; return; }
    // Everything committed is shown before leaving: the wait covers queued playback of committed
    // actions (IsPresenting) and enemy turns. Decorative reward/camp sparkles, floating text and
    // banners are not queued playback and never delay it. On the party's turn enemies cannot act,
    // so nothing resolves behind the question.
    if (BattleView->IsPresenting()) { return; }
    if (BattleView->InCombat())
    {
        const FCombatantState* Active = BattleView->Rules().Find(BattleView->Rules().ActiveId());
        // After a failed checkpoint write enemies are held too, so their turn would never end:
        // ask now rather than wait forever.
        if ((!Active || Active->Side != ECombatSide::Party) && !BattleView->CheckpointBlocked()) { return; }
        // Mid-encounter a half-entered move would be dropped, so ask first, like an overlay.
        bReturnPending = false;
        SaveOverlayFocus();
        bConfirmReturn = true;
        bDragging = false;
        BattleView->PlayCue(EExpeditionCue::Click);
        return;
    }
    bReturnPending = false;
    LeaveExpedition();
}
void AExpeditionPlayerController::ConfirmReturn()
{
    if (!bConfirmReturn) { return; }
    bConfirmReturn = false;
    // Defensive: the question only opens on the party's turn, so the run cannot end under it.
    if (IsValid(BattleView) && BattleView->CanReturnToMenu()) { LeaveExpedition(); } else { RestoreOverlayFocus(); }
}
void AExpeditionPlayerController::LeaveExpedition()
{
    // The rebuilt board's new revision resets selection, action and cursor in PlayerTick.
    BattleView->ReturnToMenu();
    if (!BattleView->HasRun()) { bNextParty = false; }
}
void AExpeditionPlayerController::History()
{
    if (!IsValid(BattleView) || bConfirmReturn) { return; }
    const bool bOpening = !BattleView->ShowingRecords();
    // Records over Help keeps the focus saved when Help opened.
    if (bOpening && !bShowHelp) { SaveOverlayFocus(); }
    bShowHelp = false;
    BattleView->ToggleRecords();
    if (bOpening) { bDragging = false; } else { RestoreOverlayFocus(); }
}
void AExpeditionPlayerController::OlderRecords() { if (IsValid(BattleView)) { BattleView->PageRecords(1); } }
void AExpeditionPlayerController::NewerRecords() { if (IsValid(BattleView)) { BattleView->PageRecords(-1); } }
void AExpeditionPlayerController::EndTurn() { if (AllowsGameplayInput()) { BattleView->Execute({ECombatCommandType::EndTurn, Actor()}); } }
void AExpeditionPlayerController::RotateLeft() { if (AllowsGameplayInput()) { BattleView->Orbit(-45); } }
void AExpeditionPlayerController::RotateRight() { if (AllowsGameplayInput()) { BattleView->Orbit(45); } }
void AExpeditionPlayerController::ZoomIn() { if (AllowsGameplayInput()) { BattleView->Zoom(-120); } }
void AExpeditionPlayerController::ZoomOut() { if (AllowsGameplayInput()) { BattleView->Zoom(120); } }
void AExpeditionPlayerController::SelectOne() { SelectPartyIndex(0); }
void AExpeditionPlayerController::SelectTwo() { SelectPartyIndex(1); }
void AExpeditionPlayerController::SelectThree() { SelectPartyIndex(2); }
void AExpeditionPlayerController::SelectFour() { SelectPartyIndex(3); }
void AExpeditionPlayerController::Skip()
{
    if (!IsValid(BattleView)) { return; }
    if (AllowsGameplayInput() && BattleView->IsPresenting()) { BattleView->SkipPresentation(); return; }
    if (!BattleView->InCombat() || !AllowsGameplayInput())
    {
        const auto* Hud = Cast<AExpeditionHUD>(GetHUD());
        const auto Items = Hud ? Hud->MenuButtons() : TArray<FName>();
        const int32 Direction = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift) ? -1 : 1;
        if (!Items.IsEmpty())
        {
            const int32 Index = Items.IndexOfByKey(Focus.Id);
            Focus.Id = Index == INDEX_NONE ? Items[Direction > 0 ? 0 : Items.Num() - 1] : Items[(Index + Items.Num() + Direction) % Items.Num()];
            Focus.bTabChosen = true;
        }
        return;
    }
    BattleView->SkipPresentation();
}
void AExpeditionPlayerController::Speed() { if (AllowsGameplayInput()) { BattleView->CycleSpeed(); } }
void AExpeditionPlayerController::ToggleHelp()
{
    if (bConfirmReturn) { return; }
    if (!bShowHelp)
    {
        // Help over Records keeps the focus saved when Records opened.
        if (IsValid(BattleView) && BattleView->ShowingRecords()) { BattleView->ToggleRecords(); } else { SaveOverlayFocus(); }
        bShowHelp = true;
        bDragging = false;
    }
    else { bShowHelp = false; RestoreOverlayFocus(); }
    if (IsValid(BattleView)) { BattleView->PlayCue(EExpeditionCue::Click); }
}
void AExpeditionPlayerController::CloseOverlays()
{
    if (bConfirmReturn) { bConfirmReturn = false; RestoreOverlayFocus(); return; }
    if (!bShowHelp && IsValid(BattleView) && !BattleView->ShowingRecords())
    {
        if (bReturnPending) { bReturnPending = false; return; }
        if (ShowingBuilder() && BattleView->HasRun()) { bNextParty = false; }
        return;
    }
    bShowHelp = false;
    if (IsValid(BattleView) && BattleView->ShowingRecords()) { BattleView->ToggleRecords(); }
    RestoreOverlayFocus();
}
void AExpeditionPlayerController::BeginDrag()
{
    if (!AllowsGameplayInput()) { return; }
    float MouseX = 0, MouseY = 0;
    bDragging = GetMousePosition(MouseX, MouseY);
    DragX = MouseX;
}
void AExpeditionPlayerController::EndDrag() { bDragging = false; }

bool AExpeditionPlayerController::ShowingBuilder() const
{
    return IsValid(BattleView) && (!BattleView->HasRun() || (bNextParty &&
        (BattleView->Progress().Phase == EExpeditionPhase::Victory || BattleView->Progress().Phase == EExpeditionPhase::Defeat)));
}
bool AExpeditionPlayerController::SelectBuild(const FString& Id)
{
    if (!CanSelectSubclass() || Party.Num() != 4 || !FBuildCatalog::Find(Id)) { return false; }
    Party[Slot].BuildId = Id;
    return true;
}
