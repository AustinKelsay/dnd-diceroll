#pragma once

#include "CoreMinimal.h"
#include "Persistence/ExpeditionSave.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "ExpeditionBattleView.generated.h"

class UCameraComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;

/** Runtime board adapter. Rules remain in FExpeditionRun; meshes only mirror snapshots. */
UCLASS()
class DNDDICEROLL_API AExpeditionBattleView : public AActor
{
    GENERATED_BODY()
public:
    AExpeditionBattleView();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    const FCombatBattle& Rules() const { return Session.Run().Battle(); }
    const FExpeditionProgress& Progress() const { return Session.Run().Progress(); }
    const TArray<FString>& Messages() const { return Feedback; }
    void Execute(const FCombatCommand& Command);
    void ChooseRoute(int32 Index);
    void ClaimReward(int32 RecipientId);
    void Recover(bool bTake);
    void NewRun();
    void Save();
    void Resume();
    void ToggleRecords();
    bool HasRun() const { return Session.HasRun(); }
    bool ShowingRecords() const { return bShowRecords; }
    const TArray<FExpeditionEnding>& Records() const { return EndingRecords; }
    int32 PresentationRevision() const { return Revision; }
    /** Explicit local storage selection before play; automation uses isolated fixtures. */
    void ConfigureStorage(const FString& Path);
    void Orbit(float Degrees);
    void Zoom(float Amount);
    FVector CellPosition(FIntPoint Cell) const;
    bool CursorCell(APlayerController* Controller, FIntPoint& Cell) const;
private:
    FExpeditionSession Session;
    TArray<FExpeditionEnding> EndingRecords;
    bool bShowRecords = false;
    bool bCheckpointBlocked = false;
    int32 Revision = 0;
    TArray<FString> Feedback;
    float EnemyDelay = 0.0f;
    float CameraYaw = -90.0f;
    float CameraDistance = 1700.0f;
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
    UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> UnitMeshes;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BoardMeshes;
    void RebuildBoard();
    UStaticMeshComponent* MakeMesh(UStaticMesh* Mesh, FVector Position, FVector Scale, FLinearColor Color);
    void RefreshUnits();
    void Record(const FCombatResult& Result);
    void UpdateCamera();
};

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
    bool HoverCell(FIntPoint& Cell) const;
    bool KeyboardCell(FIntPoint& Cell) const;
    ECombatCommandType Action() const { return SelectedAction; }
    FString ActionHelp() const;
    void SelectPartyIndex(int32 Index);
private:
    UPROPERTY() TObjectPtr<AExpeditionBattleView> BattleView;
    ECombatCommandType SelectedAction = ECombatCommandType::Attack;
    int32 SelectedId = INDEX_NONE;
    int32 LastActiveId = INDEX_NONE;
    int32 LastRevision = INDEX_NONE;
    FIntPoint GridCursor = FIntPoint::ZeroValue;
    bool bKeyboardCursor = false;
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
    void ActAtCell(FIntPoint Cell);
    void MoveCursor(FIntPoint Offset);
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
};

UCLASS()
class DNDDICEROLL_API AExpeditionHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
private:
    float HUDScale = 1.0f;
    void Panel(FLinearColor Color, float X, float Y, float Width, float Height);
    void Label(const FString& Text, float X, float Y, FLinearColor Color = FLinearColor::White, float Scale = 1.0f);
    void OutlineCell(APlayerController* Controller, const AExpeditionBattleView* View, FIntPoint Cell, FLinearColor Color, float Thickness);
};

UCLASS()
class DNDDICEROLL_API AExpeditionGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AExpeditionGameMode();
};
