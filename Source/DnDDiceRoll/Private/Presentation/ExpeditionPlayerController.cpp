#include "Presentation/ExpeditionPlayerController.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Combat/ActionFacts.h"
#include "Presentation/ExpeditionCapture.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerInput.h"
#include "Presentation/ExpeditionHUD.h"
#include "Components/InputComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Combat/ActionFacts.h"
#include "Party/BuildCatalog.h"

void AExpeditionPlayerController::BeginPlay()
{
    Super::BeginPlay();
    Preset();
    BattleView = GetWorld()->SpawnActor<AExpeditionBattleView>();
    if (!IsValid(BattleView)) { return; }
    SetViewTarget(BattleView);
    Capture = FExpeditionCaptureDirector::TryCreate(this, BattleView);
    if (Capture.IsValid())
    {
        bCapturing = true;
        if (PlayerInput) { PlayerInput->DebugExecBindings.Reset(); }
        // No console either: typed commands could change rendering between shots.
        if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport()) { Viewport->ViewportConsole = nullptr; }
        // The viewport handles F11 and Alt+Enter before any player input; switch both off for this
        // process only (in memory, never saved to config) so a shot cannot change window mode.
        UInputSettings* Settings = GetMutableDefault<UInputSettings>();
        Settings->bF11TogglesFullscreen = false;
        Settings->bAltEnterTogglesFullscreen = false;
    }
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
    // A capture run only looks: with no bindings, no key or click can submit a command or save,
    // and the engine's debug shortcuts (F1 wireframe, F2 unlit, ...) cannot change later shots.
    if (ExpeditionCapture::Requested(FCommandLine::Get()))
    {
        bCapturing = true;
        if (PlayerInput) { PlayerInput->DebugExecBindings.Reset(); }
        return;
    }
    auto Bind = [this](FKey Key, void (AExpeditionPlayerController::*Handler)())
    { InputComponent->BindKey(Key, IE_Pressed, this, Handler); };
    Bind(EKeys::LeftMouseButton, &AExpeditionPlayerController::Click);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AExpeditionPlayerController::BeginDrag);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AExpeditionPlayerController::EndDrag);
    InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &AExpeditionPlayerController::BeginPan);
    InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Released, this, &AExpeditionPlayerController::EndPan);
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
    // Hold Alt for the faint full grid (#142). Not a letter: Shift+W/A/S/D pan the camera, and Shift-Tab moves menu focus.
    // Alt only sets a flag, so Alt+Enter still reaches the viewport's fullscreen toggle and no button reads Alt.
    for (const FKey& Key : {EKeys::LeftAlt, EKeys::RightAlt})
    {
        InputComponent->BindKey(Key, IE_Pressed, this, &AExpeditionPlayerController::GridDown);
        InputComponent->BindKey(Key, IE_Released, this, &AExpeditionPlayerController::GridUp);
    }
    Bind(EKeys::A, &AExpeditionPlayerController::AttackKey);
    // D puts a visible hint away (#158). Not a gameplay key: Shift+D pans the camera, and that stays the camera's.
    Bind(EKeys::D, &AExpeditionPlayerController::DismissHintKey);
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
    // Menu (#146): I is free of every other binding. Esc closes it; every item keeps its own hotkey.
    Bind(EKeys::I, &AExpeditionPlayerController::ToggleMenu);
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
        SelectedId = INDEX_NONE; InspectedId = INDEX_NONE; bKeyboardCursor = false; SelectedAction = ECombatCommandType::Attack; bModeTip = false;
        SelectPartyIndex(0, false); GridCursor = {1, 1};
    }
    ResolvePendingReturn();
    const int32 ActiveId = Rules.ActiveId();
    if (ActiveId != LastActiveId)
    {
        const FCombatantState* Active = Rules.Find(ActiveId);
        if (Active && Active->Side == ECombatSide::Party) { SelectedId = ActiveId; GridCursor = Active->Cell; }
        LastActiveId = ActiveId;
        InspectedId = INDEX_NONE;
        SelectedAction = ECombatCommandType::Attack;
        bModeTip = false;
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
    ObserveHints();
    float MouseX = 0, MouseY = 0;
    const bool bHasMouse = PointerPosition(MouseX, MouseY);
    // Pointer motion hands the board back from the keyboard cursor, so the pointed cell is previewed before a click can act on it (#171).
    if (bHasMouse)
    {
        const FVector2D Pointer(MouseX, MouseY);
        if (!bPointerSeen) { bPointerSeen = true; PointerAnchor = Pointer; }
        else if (FVector2D::DistSquared(Pointer, PointerAnchor) > PointerMoveSlop * PointerMoveSlop) { PointerAnchor = Pointer; bKeyboardCursor = false; }
    }
    else { bPointerSeen = false; }
    if (bDragging && bHasMouse)
    {
        BattleView->Orbit((MouseX - DragX) * 0.35f);
        DragX = MouseX;
    }
    if (bPanning && bHasMouse)
    {
        // Drag the board under the pointer: about the visible width per screen width, in reference cm per pixel.
        const float PerPixel = BattleView->CameraDistance() * 0.0005f;
        BattleView->Pan(-(MouseX - PanX) * PerPixel, (MouseY - PanY) * PerPixel);
        PanX = MouseX; PanY = MouseY;
    }
    // Shift + W/A/S/D pans the focus (plain A is the attack mode); the view clamps it to the board.
    if (AllowsGameplayInput() && (IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift)))
    {
        const float Step = BattleView->CameraDistance() * 0.6f * DeltaTime;
        const float Right = (IsInputKeyDown(EKeys::D) ? 1.0f : 0.0f) - (IsInputKeyDown(EKeys::A) ? 1.0f : 0.0f);
        const float Away = (IsInputKeyDown(EKeys::W) ? 1.0f : 0.0f) - (IsInputKeyDown(EKeys::S) ? 1.0f : 0.0f);
        if (Right != 0.0f || Away != 0.0f) { BattleView->Pan(Right * Step, Away * Step); }
    }

    FBoardHighlights Highlights;
    Highlights.SelectedId = SelectedId;
    // The full grid is presentation only; an open overlay owns the screen, so it hides the grid too.
    Highlights.bFullGrid = (bGridHeld || bGridPreview) && !OverlayOpen();
    HoverUnitId = HoverTargetId = INDEX_NONE;
    Path.Reset();
    FIntPoint Hover = FIntPoint::ZeroValue;
    const AExpeditionHUD* Hud = Cast<AExpeditionHUD>(GetHUD());
    const bool bOverPanel = !bKeyboardCursor && bHasMouse && Hud && Hud->CoversBoard(FVector2D(MouseX, MouseY));
    const bool bHover = !bOverPanel && AllowsGameplayInput() && HoverCell(Hover);
    // A click acts only on what this frame shows under the pointer, and only while targeting is visible: during playback,
    // an enemy turn or an overlay no target is drawn, so nothing there may be committed unseen.
    bPointerPreviewed = bHover && !bKeyboardCursor && ShowsTargeting();
    PointerPreview = Hover;
    PreviewStamp = TargetingStamp();
    bPreviewPoseValid = ProjectionPose(PreviewPose);
    // The same for the keyboard cursor: Enter acts only on a cell the last frame drew marked while targeting was visible.
    bKeyboardPreviewed = bKeyboardCursor && ShowsTargeting();
    KeyboardPreview = GridCursor;
    KeyboardStamp = PreviewStamp;
    if (bHover)
    {
        Highlights.bHasHover = true;
        Highlights.Hover = Hover;
        for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants)
        { if (Unit.Cell == Hover && !(Unit.Side == ECombatSide::Enemy && Unit.Life == ECombatLife::Dead)) { HoverUnitId = Unit.Id; } }
    }
    const int32 ActorId = Actor();
    ReachCells = FActionReach();
    if (ShowsTargeting())
    {
        if (SelectedAction == ECombatCommandType::Attack) { Highlights.Moves = Rules.LegalMoves(ActorId); }
        // Reach describes an action the hero can still take: once the Action, the Bonus action or a resource it costs is spent, the
        // contour would suggest otherwise (#171). Out of range, no target or no sight keep it, since the outline is what explains those.
        const FActionFacts Facts = FActionFactsQuery::For(Rules, ActorId, SelectedAction);
        if (!Facts.Blocks(EActionBlocker::ActionSpent) && !Facts.Blocks(EActionBlocker::BonusSpent) && !Facts.Blocks(EActionBlocker::ResourceShort))
        {
            ReachCells = Rules.ActionReach(ActorId, SelectedAction);
            Highlights.Reach = ReachCells;
        }
        Highlights.Targets = Rules.LegalTargets(ActorId, SelectedAction);
        Highlights.bFriendlyTargets = ExpeditionStyle::IsFriendlyAction(SelectedAction, Rules.Find(ActorId));
        if (const FCombatantState* Actor = Rules.Find(ActorId)) { Highlights.bHasOrigin = true; Highlights.Origin = Actor->Cell; }
        if (bHover)
        {
            if (Highlights.Moves.Contains(Hover)) { Path = Rules.PathTo(ActorId, Hover); Highlights.Path = Path; }
            if (Highlights.Targets.Contains(HoverUnitId)) { HoverTargetId = HoverUnitId; Highlights.HoverTargetId = HoverUnitId; }
        }
        // Area victims: the same forecast the HUD labels, so the board frames exactly the labelled units.
        for (const FTargetEffect& Effect : Rules.Forecast(ActorId, SelectedAction, HoverTargetId).Effects)
        { if (Effect.Effect == EAbilityEffect::Strike) { Highlights.Victims.AddUnique(Effect.TargetId); } }
    }
    TargetIds = Highlights.Targets;
    BattleView->SetHighlights(Highlights);
    if (Capture.IsValid()) { Capture->Tick(); }
}

FHintObservation AExpeditionPlayerController::BuildHintObservation(const FCombatBattle& Rules, int32 ActorId) const
{
    FHintObservation Now;
    // The hints read the same facts the dock draws: a resource cue, and whether anything but End Turn is available.
    Now.bOnlyEndTurn = true;
    for (const ECombatCommandType Command : FActionFactsQuery::BarFor(Rules, ActorId))
    {
        const FActionFacts Facts = FActionFactsQuery::For(Rules, ActorId, Command);
        if (Command != ECombatCommandType::EndTurn && Facts.bAvailable) { Now.bOnlyEndTurn = false; }
        for (const FActionResourceCost& Cost : Facts.Resources)
        {
            // Potion bottles have a pack cap, not a starting count: only running short is a cue for them.
            const bool bSpent = Cost.Resource != EActionResource::Potion && Cost.Cap > 0 && Cost.Held < Cost.Cap;
            if (!Now.bResourceCue && (bSpent || Cost.Held < Cost.Amount)) { Now.bResourceCue = true; Now.Resource = Cost; }
        }
    }
    for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants)
    {
        if (Unit.Side != ECombatSide::Party || Unit.Life != ECombatLife::Downed || Unit.RescueTurnsLeft <= 0) { continue; }
        if (!Now.bDowned || Unit.RescueTurnsLeft < Now.RescueTurnsLeft) { Now.bDowned = true; Now.DownedName = Unit.Name; Now.RescueTurnsLeft = Unit.RescueTurnsLeft; }
    }
    return Now;
}

void AExpeditionPlayerController::TrackHintSpending(const FCombatBattle& Rules)
{
    // Successive snapshots: any resource a party member holds less of than a moment ago was spent, a Potion included.
    TMap<int32, int32> Next;
    for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants)
    {
        if (Unit.Side != ECombatSide::Party) { continue; }
        for (const ECombatCommandType Command : FActionFactsQuery::BarFor(Rules, Unit.Id))
        {
            for (const FActionResourceCost& Cost : FActionFactsQuery::For(Rules, Unit.Id, Command).Resources)
            {
                const int32 Key = Unit.Id * 8 + int32(Cost.Resource);
                if (const int32* Before = HintHeld.Find(Key); Before && *Before > Cost.Held) { bHintSpentSeen = true; HintSpent = Cost; }
                Next.Add(Key, Cost.Held);
            }
        }
    }
    HintHeld = MoveTemp(Next);
}

void AExpeditionPlayerController::ObserveHints()
{
    FHintObservation Now;
    const bool bAtRest = IsValid(BattleView) && ShowsTargeting() && !ShowingMenu();
    if (IsValid(BattleView) && BattleView->InCombat())
    {
        const FCombatBattle& Rules = BattleView->Rules();
        const int32 ActorId = Actor();
        // The facts depend only on the combat state, so they are rebuilt when it changes, not every frame.
        if (!bHintFactsValid || HintActor != ActorId || !FCombatSnapshot::StaticStruct()->CompareScriptStruct(&HintSnapshot, &Rules.Snapshot(), 0))
        {
            HintFacts = BuildHintObservation(Rules, ActorId);
            TrackHintSpending(Rules);
            HintSnapshot = Rules.Snapshot();
            HintActor = ActorId;
            bHintFactsValid = true;
        }
        Now = HintFacts;
        if (bHintSpentSeen && !Now.bResourceCue) { Now.bResourceCue = true; Now.Resource = HintSpent; }
    }
    Now.bAtRest = bAtRest;
    HintState.Observe(Now);
}

ECombatHint AExpeditionPlayerController::VisibleHint() const
{
    // Re-checked here: the state was observed at the start of the frame, and a key or click may land after an overlay opened.
    return IsValid(BattleView) && ShowsTargeting() && !ShowingMenu() ? HintState.Visible() : ECombatHint::Count;
}

bool AExpeditionPlayerController::PreviewHint(ECombatHint Hint, FString& Why)
{
    if (!IsValid(BattleView) || !ShowsTargeting()) { Why = TEXT("the party does not have a turn at rest"); return false; }
    FHintObservation Now = BuildHintObservation(BattleView->Rules(), Actor());
    Now.bAtRest = true;
    if (!HintState.Show(Hint, Now))
    {
        switch (Hint)
        {
        case ECombatHint::ScarceResources: Why = TEXT("no resource is spent or short"); break;
        case ECombatHint::DownedRescue: Why = TEXT("no party member is Downed"); break;
        case ECombatHint::EndingTurn: Why = TEXT("the active hero still has an action besides End Turn"); break;
        case ECombatHint::MoveSelectConfirm: case ECombatHint::Count: Why = TEXT("the hint does not apply"); break;
        }
        return false;
    }
    bHintFactsValid = false;
    return true;
}

void AExpeditionPlayerController::DismissHintKey()
{
    // Shift+D pans the camera; a plain D is only ever the hint's key.
    if (IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift)) { return; }
    DismissHint();
}
void AExpeditionPlayerController::DismissHint()
{
    if (VisibleHint() == ECombatHint::Count) { return; }
    HintState.Dismiss();
    BattleView->PlayCue(EExpeditionCue::Click);
}
void AExpeditionPlayerController::SkipHints()
{
    if (VisibleHint() == ECombatHint::Count) { return; }
    HintState.SkipTips();
    BattleView->PlayCue(EExpeditionCue::Click);
}
void AExpeditionPlayerController::ReplayHints()
{
    // Only Help's button reaches this; it re-arms the hints and closes Help. No game state changes.
    if (!bShowHelp) { return; }
    HintState.Replay();
    bHintFactsValid = false;
    bShowHelp = false;
    RestoreOverlayFocus();
    if (IsValid(BattleView)) { BattleView->PlayCue(EExpeditionCue::Click); }
}

bool AExpeditionPlayerController::PointerPosition(float& X, float& Y) const
{
    if (bCapturing) { return false; }
    if (bSimulatedPointer) { X = SimulatedPointer.X; Y = SimulatedPointer.Y; return true; }
    return GetMousePosition(X, Y);
}

bool AExpeditionPlayerController::ProjectionPose(FTransform& Pose) const
{
    if (PlayerCameraManager) { const FMinimalViewInfo& View = PlayerCameraManager->GetCameraCacheView(); Pose = FTransform(View.Rotation, View.Location); return true; }
    if (!IsValid(BattleView)) { return false; }
    Pose = BattleView->CameraTransform();
    return true;
}

void AExpeditionPlayerController::SimulatePointerThroughCamera(FVector2D ViewportPosition, FIntPoint ViewportSize)
{
    bSimulatedPointer = true; bSimulatedCamera = true;
    SimulatedPointer = ViewportPosition; SimulatedViewport = ViewportSize;
}

void AExpeditionPlayerController::SimulatePointer(FVector2D ViewportPosition, FIntPoint Cell)
{
    bSimulatedPointer = true; bSimulatedCamera = false;
    SimulatedPointer = ViewportPosition;
    SimulatedCell = Cell;
}

uint32 AExpeditionPlayerController::TargetingStamp() const
{
    // Everything a previewed target depends on: the mode, who acts and the whole battle state. A change of any of it voids the preview.
    uint32 Stamp = HashCombine(GetTypeHash(int32(SelectedAction)), GetTypeHash(Actor()));
    if (!IsValid(BattleView)) { return Stamp; }
    const FCombatSnapshot& Snapshot = BattleView->Rules().Snapshot();
    Stamp = HashCombine(Stamp, GetTypeHash(BattleView->PresentationRevision()));
    Stamp = HashCombine(Stamp, GetTypeHash(Snapshot.RandomState));
    Stamp = HashCombine(Stamp, GetTypeHash(Snapshot.TurnIndex));
    Stamp = HashCombine(Stamp, GetTypeHash(Snapshot.Round));
    Stamp = HashCombine(Stamp, GetTypeHash(Snapshot.MovementLeft));
    Stamp = HashCombine(Stamp, GetTypeHash(uint32(Snapshot.bActionAvailable) | uint32(Snapshot.bBonusActionAvailable) << 1));
    for (const FCombatantState& Unit : Snapshot.Encounter.Combatants)
    {
        Stamp = HashCombine(Stamp, GetTypeHash(Unit.Id));
        Stamp = HashCombine(Stamp, GetTypeHash(Unit.Cell));
        Stamp = HashCombine(Stamp, GetTypeHash(Unit.HP));
        Stamp = HashCombine(Stamp, GetTypeHash(int32(Unit.Life)));
    }
    return Stamp;
}

bool AExpeditionPlayerController::PreviewedPointerCell(FIntPoint& Cell) const
{
    if (!bPointerPreviewed || !ShowsTargeting() || PreviewStamp != TargetingStamp()) { return false; }
    Cell = PointerPreview; return true;
}

bool AExpeditionPlayerController::KeyboardPreviewValid() const
{
    return bKeyboardCursor && bKeyboardPreviewed && ShowsTargeting() && KeyboardPreview == GridCursor && KeyboardStamp == TargetingStamp();
}

void AExpeditionPlayerController::PreviewAim(bool bAim, FIntPoint Cell)
{
    SelectedAction = ECombatCommandType::Attack;
    bModeTip = false;
    bKeyboardCursor = bAim;
    if (bAim) { GridCursor = Cell; }
}

void AExpeditionPlayerController::PreviewMode(ECombatCommandType Action)
{
    KeyMode(Action);
}

bool AExpeditionPlayerController::HoverCell(FIntPoint& Cell) const
{
    if (KeyboardCell(Cell)) { return true; }
    if (bCapturing) { return false; }
    if (bSimulatedPointer && bSimulatedCamera)
    {
        if (!IsValid(BattleView) || SimulatedViewport.X <= 0 || SimulatedViewport.Y <= 0) { return false; }
        const FTransform Pose = BattleView->CameraTransform();
        const double HalfWidth = FMath::Tan(FMath::DegreesToRadians(BattleView->CameraFieldOfView()) * 0.5), HalfHeight = HalfWidth * SimulatedViewport.Y / SimulatedViewport.X;
        const double X = SimulatedPointer.X / SimulatedViewport.X * 2.0 - 1.0, Y = 1.0 - SimulatedPointer.Y / SimulatedViewport.Y * 2.0;
        const FVector Direction = (Pose.GetRotation().GetForwardVector() + Pose.GetRotation().GetRightVector() * (X * HalfWidth) + Pose.GetRotation().GetUpVector() * (Y * HalfHeight)).GetSafeNormal();
        return BattleView->CellAtRay(Pose.GetLocation(), Direction, Cell);
    }
    if (bSimulatedPointer) { Cell = SimulatedCell; return true; }
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
    UseKeyboardCursor();
}
void AExpeditionPlayerController::CursorUp() { if (ShowingMenu()) { CycleMenuFocus(-1); } else { MoveCursor({0, 1}); } }
void AExpeditionPlayerController::CursorDown() { if (ShowingMenu()) { CycleMenuFocus(1); } else { MoveCursor({0, -1}); } }
void AExpeditionPlayerController::CursorLeft() { MoveCursor({-1, 0}); }
void AExpeditionPlayerController::CursorRight() { MoveCursor({1, 0}); }
uint32 AExpeditionPlayerController::MenuScreenKey() const
{
    // Phase is offset by one so that "no run yet" (0) differs from every phase.
    const uint32 Phase = BattleView->HasRun() ? uint32(BattleView->Progress().Phase) + 1 : 0;
    return uint32(bShowHelp) | uint32(BattleView->ShowingRecords()) << 1 | uint32(ShowingBuilder()) << 2 | uint32(bConfirmReturn) << 3 | uint32(ShowingMenu()) << 4 | Phase << 5;
}
void AExpeditionPlayerController::SaveOverlayFocus() { SavedFocus = Focus; }
bool AExpeditionPlayerController::MenuAvailable() const
{
    return IsValid(BattleView) && !ShowingBuilder() && BattleView->HasRun() &&
        BattleView->Progress().Phase != EExpeditionPhase::Victory && BattleView->Progress().Phase != EExpeditionPhase::Defeat;
}
void AExpeditionPlayerController::ToggleMenu()
{
    if (!IsValid(BattleView)) { return; }
    if (bShowMenu) { CloseMenu(); BattleView->PlayCue(EExpeditionCue::Click); return; }
    // Overlays own the screen and the return question owns input; the start screen and endings have no Menu.
    if (OverlayOpen() || !MenuAvailable()) { return; }
    SaveOverlayFocus();
    bShowMenu = true;
    bDragging = false;
    BattleView->PlayCue(EExpeditionCue::Click);
}
void AExpeditionPlayerController::CloseMenu()
{
    if (!bShowMenu) { return; }
    // The flag drops first so the screen key matches the screen the saved focus belongs to.
    bShowMenu = false;
    RestoreOverlayFocus();
}
void AExpeditionPlayerController::CycleMenuFocus(int32 Direction)
{
    const auto* Hud = Cast<AExpeditionHUD>(GetHUD());
    const auto Items = Hud ? Hud->MenuButtons() : TArray<FName>();
    if (Items.IsEmpty()) { return; }
    const int32 Index = Items.IndexOfByKey(Focus.Id);
    Focus.Id = Index == INDEX_NONE ? Items[Direction > 0 ? 0 : Items.Num() - 1] : Items[(Index + Items.Num() + Direction) % Items.Num()];
    Focus.bTabChosen = true;
}
void AExpeditionPlayerController::RestoreOverlayFocus()
{
    Focus = IsValid(BattleView) && SavedFocus.Screen == MenuScreenKey() ? SavedFocus : FMenuFocus();
}
void AExpeditionPlayerController::UpdateMenuFocus()
{
    if (!IsValid(BattleView)) { return; }
    // The endings and the start screen have no Menu: a stale open flag must not reappear later.
    if (bShowMenu && !MenuAvailable()) { bShowMenu = false; }
    // A new screen (phase, builder or overlay) never inherits the previous screen's focus.
    const uint32 Screen = MenuScreenKey();
    if (Focus.Screen != Screen) { Focus = {NAME_None, Screen, false}; }
    if (BattleView->InCombat() && AllowsGameplayInput() && !ShowingMenu()) { Focus.Id = NAME_None; return; }
    const auto* Hud = Cast<AExpeditionHUD>(GetHUD());
    const auto Items = Hud ? Hud->MenuButtons() : TArray<FName>();
    if (Focus.bTabChosen && Items.Contains(Focus.Id)) { return; }
    // Focus only controls actually drawn and enabled. Automatic focus is the screen's
    // primary choice; while a result is still playing and that choice is not drawn yet,
    // nothing is focused, so Enter cannot fall through to a system button such as Help.
    FName Preferred = TEXT("Builder");
    if (bConfirmReturn) { Preferred = TEXT("ReturnConfirm"); }
    else if (OverlayOpen()) { Preferred = TEXT("Close"); }
    else if (ShowingMenu()) { Preferred = TEXT("Save"); }
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
    if (!BattleView->InCombat() || !AllowsGameplayInput() || ShowingMenu())
    {
        const auto* Hud = Cast<AExpeditionHUD>(GetHUD());
        if (Hud && Hud->MenuButtons().Contains(Focus.Id)) { HandleButton(Focus.Id); }
        return;
    }
    // Enter acts on the cell the keyboard cursor marks, so it must be showing: with it hidden (the pointer took over, or it
    // was never shown) the first Enter only brings it back on its cell, and a following Enter acts (#171).
    if (!bKeyboardCursor) { UseKeyboardCursor(); return; }
    // ... and it must have been drawn on this cell, for this mode and state, with targeting visible: during playback, an enemy
    // turn or an overlay nothing is marked as a target, and a cursor moved or a state changed since the last frame waits for the
    // next one to show it. One Enter acts whenever the cursor is showing as it will act.
    if (!KeyboardPreviewValid()) { return; }
    ActAtCell(GridCursor);
}

void AExpeditionPlayerController::SelectPartyIndex(int32 Index, bool bInspect)
{
    if (!AllowsGameplayInput()) { return; }
    if (CanSelectSubclass()) { Slot = FMath::Clamp(Index, 0, 3); return; }
    int32 PartyIndex = 0;
    for (const FCombatantState& Unit : BattleView->Rules().Snapshot().Encounter.Combatants)
    {
        if (Unit.Side == ECombatSide::Party && PartyIndex++ == Index)
        {
            // Choosing the selected hero again toggles their full status (the acting hero is selected by default).
            if (bInspect) { InspectedId = SelectedId == Unit.Id && InspectedId != Unit.Id ? Unit.Id : INDEX_NONE; }
            SelectedId = Unit.Id;
            return;
        }
    }
}

void AExpeditionPlayerController::Click()
{
    if (!IsValid(BattleView)) { return; }
    float MouseX = 0, MouseY = 0;
    const bool bHasMouse = PointerPosition(MouseX, MouseY);
    if (const AExpeditionHUD* Hud = Cast<AExpeditionHUD>(GetHUD()); Hud && bHasMouse)
    {
        FName Button;
        if (Hud->ButtonAt(FVector2D(MouseX, MouseY), Button)) { HandleButton(Button); return; }
        // A click beside the open Menu closes it and goes no further.
        if (ShowingMenu()) { CloseMenu(); return; }
        if (Hud->CoversBoard(FVector2D(MouseX, MouseY))) { return; }
    }
    if (OverlayOpen()) { return; }
    // The keyboard cursor owns the board until the pointer moves. A click that finds it still there only
    // hands the board back: the pointed cell was never previewed, so nothing may act on it yet (#171).
    if (bKeyboardCursor) { bKeyboardCursor = false; return; }
    FIntPoint Cell, Under;
    if (PreviewedPointerCell(Cell))
    {
        // The camera eases on its own: the cell under a resting pointer can change after the preview was drawn. Act only if it is
        // still the cell that was shown; otherwise the next frame previews the new one first.
        // The camera also moves under a resting pointer (Q/E, pan, zoom, the focus easing to a new unit): the preview was found with
        // the pose of the frame it was drawn in, so a camera that has moved since voids it, and the next frame previews afresh.
        FTransform Pose;
        const bool bCameraStill = bPreviewPoseValid && ProjectionPose(Pose) &&
            FVector::Dist(Pose.GetLocation(), PreviewPose.GetLocation()) <= PoseSlopCm &&
            FMath::RadiansToDegrees(Pose.GetRotation().AngularDistance(PreviewPose.GetRotation())) <= PoseSlopDegrees;
        if (bCameraStill && HoverCell(Under) && Under == Cell) { ActAtCell(Cell); }
        return;
    }
    // Nothing is previewed (playback, an enemy turn): a click may still choose an ally to inspect, which commits nothing.
    if (SelectedAction == ECombatCommandType::Attack && HoverCell(Cell))
    {
        for (const FCombatantState& Unit : BattleView->Rules().Snapshot().Encounter.Combatants)
        { if (Unit.Cell == Cell && Unit.Side == ECombatSide::Party) { SelectedId = Unit.Id; BattleView->PlayCue(EExpeditionCue::Click); return; } }
    }
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
    if (Name == TEXT("ReplayTips")) { ReplayHints(); return; }
    if (!AllowsGameplayInput() && Name != TEXT("Close") && Name != TEXT("Help") && Name != TEXT("Records") && Name != TEXT("Older") && Name != TEXT("Newer")) { return; }
    // The hint's own buttons: they act on the hint and nothing else, and only while it is visible.
    if (Name == TEXT("HintGotIt")) { DismissHint(); return; }
    if (Name == TEXT("HintSkip")) { SkipHints(); return; }
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
    else if (Name == TEXT("Maneuver")) { SetMode(SpecialAction(1)); }
    else if (Name == TEXT("Save")) { Save(); }
    else if (Name == TEXT("Resume")) { Resume(); }
    else if (Name == TEXT("MenuToggle")) { ToggleMenu(); }
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
    else if (Name == TEXT("ModeAttack")) { SetMode(ECombatCommandType::Attack); }
    else if (Name == TEXT("ModeRole")) { SetMode(RoleAction()); }
    else if (Name == TEXT("ModeRescue")) { SetMode(ECombatCommandType::Rescue); }
    else if (Name == TEXT("ModePotion")) { SetMode(ECombatCommandType::Potion); }
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
        else { Acted(BattleView->Execute({SelectedAction, ActorId, {}, Unit.Id})); }
        return;
    }
    if (SelectedAction == ECombatCommandType::Attack) { Acted(BattleView->Execute({ECombatCommandType::Move, ActorId, Cell})); }
    else { Acted(BattleView->Execute({SelectedAction, ActorId, {}, INDEX_NONE})); }
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
    bModeTip = false;
    if (IsValid(BattleView)) { BattleView->PlayCue(EExpeditionCue::Click); }
}
void AExpeditionPlayerController::Acted(bool bAccepted)
{
    // A rejected command leaves the mode and its tooltip up: the tooltip is where the reason is.
    if (bAccepted) { bModeTip = false; }
}
void AExpeditionPlayerController::KeyMode(ECombatCommandType Action)
{
    SetMode(Action);
    bModeTip = AllowsGameplayInput();
}
bool AExpeditionPlayerController::AllowsGameplayInput() const
{
    return IsValid(BattleView) && !OverlayOpen();
}
bool AExpeditionPlayerController::ShowsTargeting() const
{
    return IsValid(BattleView) && AllowsGameplayInput() && BattleView->InCombat() && !BattleView->IsPresenting() &&
        Actor() == BattleView->Rules().ActiveId();
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
void AExpeditionPlayerController::ManeuverMode() { KeyMode(SpecialAction(1)); }
void AExpeditionPlayerController::AttackMode() { KeyMode(ECombatCommandType::Attack); }
void AExpeditionPlayerController::AttackKey()
{
    if (IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift)) { return; }
    AttackMode();
}
void AExpeditionPlayerController::RescueMode() { KeyMode(ECombatCommandType::Rescue); }
void AExpeditionPlayerController::PotionMode() { KeyMode(ECombatCommandType::Potion); }
void AExpeditionPlayerController::RoleMode() { KeyMode(RoleAction()); }
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
    const int32 Before = BattleView->PresentationRevision();
    BattleView->NewRun(Party);
    if (BattleView->HasRun() && BattleView->Progress().Phase == EExpeditionPhase::Route) { bNextParty = false; bReturnPending = false; }
    // A fresh run is a first-time run: re-arm the hints, including after Skip tips.
    if (Before != BattleView->PresentationRevision() && BattleView->HasRun()) { HintState.Arm(); ResetHintSpending(); bHintFactsValid = false; }
}
void AExpeditionPlayerController::Save() { CloseMenu(); if (AllowsGameplayInput()) { BattleView->Save(); } }
void AExpeditionPlayerController::Resume()
{
    CloseMenu();
    if (!AllowsGameplayInput()) { return; }
    const int32 Before = BattleView->PresentationRevision();
    BattleView->Resume();
    if (Before != BattleView->PresentationRevision())
    {
        bNextParty = false; bReturnPending = false; Focus = FMenuFocus();
        // A returning player is not a first-time player: Resume does not arm hints (Replay tips in Help does).
        HintState.Disarm(); ResetHintSpending(); bHintFactsValid = false;
    }
}
void AExpeditionPlayerController::RequestReturnToMenu()
{
    CloseMenu();
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
        CloseMenu(); // The Menu may have been opened while the return waited.
        SaveOverlayFocus();
        bConfirmReturn = true;
        bDragging = false; bPanning = false;
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
    CloseMenu();
    const bool bOpening = !BattleView->ShowingRecords();
    // Records over Help keeps the focus saved when Help opened.
    if (bOpening && !bShowHelp) { SaveOverlayFocus(); }
    bShowHelp = false;
    BattleView->ToggleRecords();
    if (bOpening) { bDragging = false; bPanning = false; } else { RestoreOverlayFocus(); }
}
void AExpeditionPlayerController::OlderRecords() { if (IsValid(BattleView)) { BattleView->PageRecords(1); } }
void AExpeditionPlayerController::NewerRecords() { if (IsValid(BattleView)) { BattleView->PageRecords(-1); } }
void AExpeditionPlayerController::EndTurn() { if (AllowsGameplayInput()) { bModeTip = false; BattleView->Execute({ECombatCommandType::EndTurn, Actor()}); } }
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
    if (AllowsGameplayInput() && !ShowingMenu() && BattleView->IsPresenting()) { BattleView->SkipPresentation(); return; }
    if (!BattleView->InCombat() || !AllowsGameplayInput() || ShowingMenu())
    {
        CycleMenuFocus(IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift) ? -1 : 1);
        return;
    }
    BattleView->SkipPresentation();
}
void AExpeditionPlayerController::Speed() { CloseMenu(); if (AllowsGameplayInput()) { BattleView->CycleSpeed(); } }
void AExpeditionPlayerController::ToggleHelp()
{
    if (bConfirmReturn) { return; }
    CloseMenu();
    if (!bShowHelp)
    {
        // Help over Records keeps the focus saved when Records opened.
        if (IsValid(BattleView) && BattleView->ShowingRecords()) { BattleView->ToggleRecords(); } else { SaveOverlayFocus(); }
        bShowHelp = true;
        bDragging = false; bPanning = false;
    }
    else { bShowHelp = false; RestoreOverlayFocus(); }
    if (IsValid(BattleView)) { BattleView->PlayCue(EExpeditionCue::Click); }
}
void AExpeditionPlayerController::CloseOverlays()
{
    if (bShowMenu) { CloseMenu(); return; }
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
    bDragging = PointerPosition(MouseX, MouseY);
    DragX = MouseX;
}
void AExpeditionPlayerController::EndDrag() { bDragging = false; }
void AExpeditionPlayerController::BeginPan()
{
    if (!AllowsGameplayInput()) { return; }
    bPanning = PointerPosition(PanX, PanY);
}
void AExpeditionPlayerController::EndPan() { bPanning = false; }
void AExpeditionPlayerController::GridDown() { bGridHeld = true; }
void AExpeditionPlayerController::GridUp() { bGridHeld = false; }
void AExpeditionPlayerController::PreviewFullGrid(bool bShow) { bGridPreview = bShow; }

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
