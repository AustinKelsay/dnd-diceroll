#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"
#include "Run/ExpeditionRun.h"
#include "GameFramework/PlayerController.h"
#include "ExpeditionPlayerController.generated.h"

class AExpeditionBattleView;

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
private:
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
    bool bDragging = false;
    float DragX = 0.0f;
    TArray<FPartyBuildSelection> Party;
    int32 Slot = 0;
    bool bNextParty = false;
    /** A focused menu control and the screen it belongs to. Focus the player did not choose with Tab follows that screen's primary choice. */
    struct FMenuFocus { FName Id; uint32 Screen = MAX_uint32; bool bChosen = false; };
    /** Saved while Help or Records covers the screen; restored only onto the same screen. */
    FMenuFocus Focus, SavedFocus;
    uint32 MenuScreen() const;
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
