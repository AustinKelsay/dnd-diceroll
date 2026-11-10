#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"
#include "Presentation/CombatHints.h"
#include "Run/ExpeditionRun.h"
#include "Presentation/ExpeditionAutoplay.h"
#include "GameFramework/PlayerController.h"
#include "ExpeditionPlayerController.generated.h"

class AExpeditionBattleView;
class FExpeditionCaptureDirector;
class FExpeditionAutoplayDirector;

/** Turns mouse, keyboard and HUD buttons into view commands. Holds selection only. */
UCLASS()
class DNDDICEROLL_API AExpeditionPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;
    AExpeditionBattleView* GetBattleView() const { return BattleView; }
    int32 Selection() const { return SelectedId; }
    /** Who acts on the next command: the active adventurer in combat, else the selection. */
    int32 Actor() const;
    bool HoverCell(FIntPoint& Cell) const;
    bool KeyboardCell(FIntPoint& Cell) const;
    ECombatCommandType Action() const { return SelectedAction; }
    /**
     * True while the action chosen with its key keeps its tooltip up, so the keyboard shows what hover shows (#58).
     * Cleared by a turn change, an accepted command, ending the turn, choosing a mode with the pointer and the aim preview;
     * a rejected command leaves it up, since the tooltip holds the reason.
     */
    bool ModeTipActive() const { return bModeTip; }
    /** The acting adventurer's role ability, or Attack for the rogue (Sneak Attack is automatic). */
    ECombatCommandType RoleAction() const;
    /** Selects the Nth party member; with bInspect, choosing the already selected hero toggles their full status. */
    void SelectPartyIndex(int32 Index, bool bInspect = true);
    /** The hero whose full status the player asked for by choosing them again; cleared each turn. */
    int32 Inspected() const { return InspectedId; }
    void HandleButton(FName Id);
    EFighterSubclass StartupSubclass() const;
    const TArray<FPartyBuildSelection>& NextParty() const { return Party; }
    int32 BuilderSlot() const { return Slot; }
    ECombatCommandType SpecialAction(int32 Index) const;
    bool ShowingHelp() const { return bShowHelp; }
    /** True while the mid-encounter "Return to menu?" question is open. */
    bool ConfirmingReturn() const { return bConfirmReturn; }
    /** True while a requested return waits for the party's turn and playback to finish. */
    bool ReturnPending() const { return bReturnPending; }
    /**
     * The top-bar Menu (#146) lists Save, Resume, Records, Speed, Help and Return to start. It is
     * a transient layer, not an overlay: gameplay keys keep working while it is open.
     */
    bool ShowingMenu() const { return bShowMenu && MenuAvailable(); }
    /** The Menu button exists on every expedition screen except the start screen and the endings. */
    bool MenuAvailable() const;
    /** Help, Records or the return question covers the screen and owns input. */
    bool OverlayOpen() const;
    /**
     * The one gate for targeting feedback (reach markers, target chips, highlights): a party
     * member has the turn in a fight, no overlay covers the board and no playback is running.
     */
    bool ShowsTargeting() const;
    /** Unit under the cursor, if any. */
    int32 HoveredUnit() const { return HoverUnitId; }
    /** Legal target under the cursor for the selected action. */
    int32 HoveredTarget() const { return HoverTargetId; }
    const TArray<FIntPoint>& PreviewPath() const { return Path; }
    /** Legal targets for the selected action this frame. */
    const TArray<int32>& Targets() const { return TargetIds; }
    /** Where the selected action can reach this frame; empty unless ShowsTargeting(). */
    const FActionReach& Reach() const { return ReachCells; }
    /** True when the board should show where the keyboard cursor is. */
    bool UsingKeyboardCursor() const { return bKeyboardCursor; }
    FIntPoint KeyboardCursorCell() const { return GridCursor; }
    bool SelectBuild(const FString& Id);
    void StepClass(int32 Direction);
    void StepBuild(int32 Direction);
    bool ShowingBuilder() const;
    FName MenuFocus() const { return Focus.Id; }
    /** Refresh after the HUD registers this frame's visible, enabled controls. */
    void UpdateMenuFocus();
    /** Capture support: Attack mode, with the keyboard cursor on a cell (or cleared), so reach and target feedback show. */
    void PreviewAim(bool bAim, FIntPoint Cell = FIntPoint::ZeroValue);
    /** Capture and test support: choose a mode as its key does, so its tooltip shows. */
    void PreviewMode(ECombatCommandType Action);
    /** Capture and test support: show the full grid as if the hold key were down. */
    void PreviewFullGrid(bool bShow);
    /** True while the hold-for-full-grid key (Alt) is down or a preview asks for it. */
    bool ShowsFullGrid() const { return bGridHeld || bGridPreview; }
    /** True in a capture run: no input is bound and the physical pointer is ignored. */
    bool IsCapturing() const { return bCapturing; }
    /**
     * The click path: acts on a cell as a left click does for the selected mode (a move, an attack, an ability or a
     * rescue on whoever stands there). Returns whether the rules accepted the command. Autoplay (#148) and tests use it.
     */
    bool ActAtCell(FIntPoint Cell);
    /** Starts autoplay in this process (-ExpeditionAutoplay starts it from the command line). Tests pass bExitOnFinish false. */
    TSharedPtr<FExpeditionAutoplayDirector> StartAutoplay(const ExpeditionAutoplay::FAutoplayScript& Script, bool bResume, bool bExitOnFinish);
    /**
     * The hint drawn this frame (#158), or Count. A hint is non-modal and takes no focus; it shows only on the party's
     * turn at rest, never under Help, Records, the return question, the Menu, a playback or an enemy turn.
     */
    ECombatHint VisibleHint() const;
    FHintContent VisibleHintContent() const { return HintState.Content(); }
    const FCombatHints& Hints() const { return HintState; }
    /** Capture and test support: raise exactly this hint if it applies to the acting hero now; Why says what is missing. */
    bool PreviewHint(ECombatHint Hint, FString& Why);
    /** Capture support: put the previewed hint away (a capture run never arms hints on its own). */
    void ClearHintPreview() { HintState.Disarm(); }
    /** The pointer's viewport position, or false when there is none (and always in a capture run, so a focused window cannot change a shot). */
    bool PointerPosition(float& X, float& Y) const;
    /**
     * Test support: the pointer rests at this viewport position, over this board cell, as a real pointer reports.
     * A transient test world has no viewport to project through, so the cell is given. Moving it again is pointer motion.
     */
    void SimulatePointer(FVector2D ViewportPosition, FIntPoint Cell);
    /**
     * Test support: the pointer rests at this viewport pixel of a viewport of this size, and the cell under it is found by
     * casting a ray through the battle view's real camera (its eased transform and field of view), so orbiting, panning and
     * zooming through the controller move the cell under a resting pointer the way they do on screen.
     */
    void SimulatePointerThroughCamera(FVector2D ViewportPosition, FIntPoint ViewportSize);
    /** Test support: back to the physical pointer. */
    void ReleaseSimulatedPointer() { bSimulatedPointer = false; }
    /**
     * The cell the last frame previewed under the pointer, the only one a click may act on (#171). False unless targeting is
     * visible now (no keyboard cursor, panel, overlay or playback owns the board) and the battle state, actor and mode are
     * still the ones the preview was drawn for.
     */
    bool PreviewedPointerCell(FIntPoint& Cell) const;
private:
    /** Pointer motion beyond this many viewport pixels hands the board back from the keyboard cursor to the pointer. */
    static constexpr float PointerMoveSlop = 3.0f;
    bool bCapturing = false;
    bool bSimulatedPointer = false;
    bool bSimulatedCamera = false;
    FIntPoint SimulatedViewport = FIntPoint::ZeroValue;
    FVector2D SimulatedPointer = FVector2D::ZeroVector;
    FIntPoint SimulatedCell = FIntPoint::ZeroValue;
    /** Where the pointer was when motion was last counted; motion is measured from here so slow drift still counts. */
    bool bPointerSeen = false;
    FVector2D PointerAnchor = FVector2D::ZeroVector;
    bool bPointerPreviewed = false;
    FIntPoint PointerPreview = FIntPoint::ZeroValue;
    /** What the preview was drawn for; a change before the click voids it. */
    uint32 PreviewStamp = 0;
    /**
     * The camera the pointer is projected through (the player camera's cache, else the view's own camera). The hover cell is found
     * with the pose of one frame and the camera eases on its own, so a preview is trusted only while the pose has not moved since.
     */
    bool ProjectionPose(FTransform& Pose) const;
    bool bPreviewPoseValid = false;
    FTransform PreviewPose;
    /** Camera movement under this much (cm, degrees) moves nothing under the pointer by a pixel. */
    static constexpr float PoseSlopCm = 0.5f, PoseSlopDegrees = 0.02f;
    /** The keyboard cursor's cell, as the last frame drew it with targeting visible, and the state it was drawn for (#171 review). */
    bool bKeyboardPreviewed = false;
    FIntPoint KeyboardPreview = FIntPoint::ZeroValue;
    uint32 KeyboardStamp = 0;
    /** True while the keyboard cursor is showing on its current cell for the current state with targeting visible: the only time Enter may act. */
    bool KeyboardPreviewValid() const;
    uint32 TargetingStamp() const;
    /** Shows the keyboard cursor; the pointer is measured afresh from then on, so only later motion hands control back. */
    void UseKeyboardCursor() { bKeyboardCursor = true; bPointerSeen = false; }
    TSharedPtr<FExpeditionCaptureDirector> Capture;
    TSharedPtr<FExpeditionAutoplayDirector> Autoplay;
    UPROPERTY() TObjectPtr<AExpeditionBattleView> BattleView;
    ECombatCommandType SelectedAction = ECombatCommandType::Attack;
    int32 SelectedId = INDEX_NONE;
    int32 InspectedId = INDEX_NONE;
    int32 LastActiveId = INDEX_NONE;
    int32 LastRevision = INDEX_NONE;
    int32 HoverUnitId = INDEX_NONE;
    int32 HoverTargetId = INDEX_NONE;
    TArray<FIntPoint> Path;
    TArray<int32> TargetIds;
    FActionReach ReachCells;
    FIntPoint GridCursor = FIntPoint::ZeroValue;
    bool bKeyboardCursor = false;
    bool bModeTip = false;
    /** Contextual hints (#158): presentation only, in memory, never in the checkpoint. */
    FCombatHints HintState;
    FCombatSnapshot HintSnapshot;
    FHintObservation HintFacts;
    int32 HintActor = INDEX_NONE;
    bool bHintFactsValid = false;
    /** What each party member held of each resource in the last observed snapshot (key: unit id * 8 + resource), to see a resource being spent. */
    TMap<int32, int32> HintHeld;
    /** A resource was spent this run, whatever its cap (a Potion has none to fall below); kept until the next New Run or Resume. */
    bool bHintSpentSeen = false;
    FActionResourceCost HintSpent;
    void TrackHintSpending(const FCombatBattle& Rules);
    void ResetHintSpending() { HintHeld.Reset(); bHintSpentSeen = false; HintSpent = FActionResourceCost(); }
    void ObserveHints();
    FHintObservation BuildHintObservation(const FCombatBattle& Rules, int32 ActorId) const;
    /** The D key: puts a visible hint away; a no-op when none is up, and never a gameplay key. */
    void DismissHintKey();
    void DismissHint();
    void SkipHints();
    void ReplayHints();
    bool bShowHelp = false;
    bool bShowMenu = false;
    bool bConfirmReturn = false;
    bool bReturnPending = false;
    /** The phase a pending return was requested in; it is dropped if the phase changes. */
    EExpeditionPhase PendingPhase = EExpeditionPhase::Route;
    bool bDragging = false;
    bool bGridHeld = false;
    bool bGridPreview = false;
    float DragX = 0.0f;
    bool bPanning = false;
    float PanX = 0.0f, PanY = 0.0f;
    TArray<FPartyBuildSelection> Party;
    int32 Slot = 0;
    bool bNextParty = false;
    /** A focused menu control and the screen it belongs to. Focus the player did not choose with Tab follows that screen's primary choice. */
    struct FMenuFocus { FName Id; uint32 Screen = MAX_uint32; bool bTabChosen = false; };
    /** Saved while Help or Records covers the screen; restored only onto the same screen. */
    FMenuFocus Focus, SavedFocus;
    /** Identifies the visible menu screen: overlay, builder and expedition phase. */
    uint32 MenuScreenKey() const;
    void SaveOverlayFocus();
    void ToggleMenu();
    /** Closes the Menu and restores the focus saved when it opened. No-op when it is closed. */
    void CloseMenu();
    /** Tab and the up/down arrows move the focused menu control; Direction is +1 or -1. */
    void CycleMenuFocus(int32 Direction);
    /** Call after the overlay has closed. */
    void RestoreOverlayFocus();
    void Preset();
    void CycleClass();
    void CycleBuild();
    void Champion();
    void BattleMaster();
    void ManeuverMode();
    void Click();
    void AttackMode();
    void RoleMode();
    void RescueMode();
    void PotionMode();
    void Loot();
    void FirstRoute();
    void SecondRoute();
    void Camp();
    void SkipCamp();
    void NewRun();
    void Save();
    void Resume();
    /**
     * Backspace or the Menu button. Waits until the party has the turn and committed results
     * have played, then asks first mid-encounter or returns at once elsewhere. Again withdraws.
     */
    void RequestReturnToMenu();
    void ResolvePendingReturn();
    void ConfirmReturn();
    void LeaveExpedition();
    void History();
    void OlderRecords();
    void NewerRecords();
    void MoveCursor(FIntPoint ScreenOffset);
    void CursorUp();
    void CursorDown();
    void CursorLeft();
    void CursorRight();
    void ConfirmCell();
    void EndTurn();
    void RotateLeft();
    void RotateRight();
    void ZoomIn();
    void ZoomOut();
    void SelectOne();
    void SelectTwo();
    void SelectThree();
    void SelectFour();
    void Skip();
    void Speed();
    void ToggleHelp();
    void CloseOverlays();
    void GridDown();
    void GridUp();
    void BeginDrag();
    void EndDrag();
    void BeginPan();
    void EndPan();
    /** The A key: attack mode, except while Shift pans the camera. */
    void AttackKey();
    void SetMode(ECombatCommandType Action);
    /** A mode key: selects the mode and keeps its tooltip up. */
    void KeyMode(ECombatCommandType Action);
    void Acted(bool bAccepted);
    bool AllowsGameplayInput() const;
    bool CanSelectSubclass() const;
};
