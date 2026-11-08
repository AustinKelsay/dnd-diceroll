#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"
#include "Run/ExpeditionRun.h"
#include "GameFramework/PlayerController.h"
#include "ExpeditionPlayerController.generated.h"

class AExpeditionBattleView;
class FExpeditionCaptureDirector;

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
    /** The acting adventurer's role ability, or Attack for the rogue (Sneak Attack is automatic). */
    ECombatCommandType RoleAction() const;
    void SelectPartyIndex(int32 Index);
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
    /** Help, Records or the return question covers the screen and owns input. */
    bool OverlayOpen() const;
    /** Unit under the cursor, if any. */
    int32 HoveredUnit() const { return HoverUnitId; }
    /** Legal target under the cursor for the selected action. */
    int32 HoveredTarget() const { return HoverTargetId; }
    const TArray<FIntPoint>& PreviewPath() const { return Path; }
    /** Legal targets for the selected action this frame. */
    const TArray<int32>& Targets() const { return TargetIds; }
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
    /** True in a capture run: no input is bound and the physical pointer is ignored. */
    bool IsCapturing() const { return bCapturing; }
private:
    /** The mouse position, or false in a capture run so a focused window cannot change a shot. */
    bool PointerPosition(float& X, float& Y) const;
    bool bCapturing = false;
    TSharedPtr<FExpeditionCaptureDirector> Capture;
    UPROPERTY() TObjectPtr<AExpeditionBattleView> BattleView;
    ECombatCommandType SelectedAction = ECombatCommandType::Attack;
    int32 SelectedId = INDEX_NONE;
    int32 LastActiveId = INDEX_NONE;
    int32 LastRevision = INDEX_NONE;
    int32 HoverUnitId = INDEX_NONE;
    int32 HoverTargetId = INDEX_NONE;
    TArray<FIntPoint> Path;
    TArray<int32> TargetIds;
    FIntPoint GridCursor = FIntPoint::ZeroValue;
    bool bKeyboardCursor = false;
    bool bShowHelp = false;
    bool bConfirmReturn = false;
    bool bReturnPending = false;
    /** The phase a pending return was requested in; it is dropped if the phase changes. */
    EExpeditionPhase PendingPhase = EExpeditionPhase::Route;
    bool bDragging = false;
    float DragX = 0.0f;
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
    void ActAtCell(FIntPoint Cell);
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
    void BeginDrag();
    void EndDrag();
    void SetMode(ECombatCommandType Action);
    bool AllowsGameplayInput() const;
    bool CanSelectSubclass() const;
};
