#pragma once

#include "CoreMinimal.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/ExpeditionPlayback.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "ExpeditionBattleView.generated.h"

class UAudioComponent;
class UCameraComponent;
class UExponentialHeightFogComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UPostProcessComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class APlayerController;
class UDiceRollTheme;

UENUM()
enum class EBoardTheme : uint8 { Meadow, Ruins, Vault };

UENUM()
enum class EPlaybackSpeed : uint8 { Normal, Fast, Instant };

/** One colored primitive in a presentation group. */
USTRUCT()
struct FExpeditionPart
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
    UPROPERTY() FLinearColor Base = FLinearColor::White;
    FLinearColor Shown = FLinearColor::Transparent;
    void SetColor(const FLinearColor& Color);
};

USTRUCT()
struct FExpeditionUnitRig
{
    GENERATED_BODY()
    int32 Id = INDEX_NONE;
    float Phase = 0.0f;
    UPROPERTY() TObjectPtr<USceneComponent> Root;
    UPROPERTY() TArray<FExpeditionPart> Parts;
};

USTRUCT()
struct FExpeditionTile
{
    GENERATED_BODY()
    FIntPoint Cell = FIntPoint::ZeroValue;
    UPROPERTY() FExpeditionPart Part;
};

/** A themed prop that sways (banners, cages) or flickers (flames) in place. */
USTRUCT()
struct FExpeditionSway
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<USceneComponent> Component;
    FRotator Base = FRotator::ZeroRotator;
    FVector Scale = FVector::OneVector;
    float Phase = 0.0f;
    float Degrees = 0.0f;
    float Flicker = 0.0f;
};

/** What the controller wants emphasised on the board this frame. */
struct FBoardHighlights
{
    TArray<FIntPoint> Moves;
    TArray<FIntPoint> Path;
    TArray<int32> Targets;
    /** Selected action's reach: cells it can strike, and cells in range but blocked or out of sight. */
    TArray<FIntPoint> InRange;
    TArray<FIntPoint> Obscured;
    int32 SelectedId = INDEX_NONE;
    int32 HoverTargetId = INDEX_NONE;
    bool bHasHover = false;
    FIntPoint Hover = FIntPoint::ZeroValue;
    bool bFriendlyTargets = false;
};

/**
 * Runtime board adapter. FExpeditionSession owns the rules and checkpoints; this actor
 * submits commands, then replays committed results through FExpeditionPlayback. Meshes come
 * from the dark-fantasy theme set when it is available and engine primitives otherwise;
 * lights and sounds are built at runtime. With the theme, the mission overview (title and
 * route choice) is a candlelit tabletop and each encounter is a full surrounding world.
 */
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
    void ShortRest(int32 UnitId);
    void ArcaneRecovery(int32 UnitId);
    void MagicalCunning(int32 UnitId);
    void CampActivity(int32 UnitId, ECampActivity Activity);
    void NewRun(const TArray<FPartyBuildSelection>& Party);
    void NewRun(EFighterSubclass Subclass = EFighterSubclass::Champion);
    void Save();
    void Resume();
    void ToggleRecords();
    bool HasRun() const { return Session.HasRun(); }
    bool ShowingRecords() const { return bShowRecords; }
    bool InCombat() const;
    /** True while the tabletop mission overview is shown instead of a battle board. */
    bool ShowingOverview() const { return bOverview; }
    const TArray<FExpeditionEnding>& Records() const { return EndingRecords; }
    static constexpr int32 RecordsPerPage = 4;
    int32 RecordOffset() const { return RecordsOffset; }
    void PageRecords(int32 Direction);
    int32 PresentationRevision() const { return Revision; }
    /** Explicit local storage selection before play; automation uses isolated fixtures. */
    void ConfigureStorage(const FString& Path);

    // Presentation.
    const FExpeditionPlayback& Playback() const { return Show; }
    bool IsPresenting() const { return Show.IsBusy() || (GetWorld() && GetWorld()->GetTimeSeconds() < SpellFXHoldUntil); }
    void SkipPresentation();
    void CycleSpeed();
    EPlaybackSpeed Speed() const { return PlaybackSpeed; }
    FString LocationName() const;
    void SetHighlights(const FBoardHighlights& Value) { Highlights = Value; }
    void PlayCue(EExpeditionCue Cue);

    // Camera and board geometry.
    void Orbit(float Degrees);
    void Zoom(float Amount);
    float CameraYaw() const { return TargetYaw; }
    FVector CellPosition(FIntPoint Cell) const;
    FVector BoardPosition(FVector2D Cell, float Height = 0.0f) const;
    /** Where a combatant currently appears, including playback motion. */
    FVector UnitPosition(int32 Id, float Height = 0.0f) const;
    bool CursorCell(APlayerController* Controller, FIntPoint& Cell) const;

private:
    FExpeditionSession Session;
    FExpeditionPlayback Show;
    TArray<FExpeditionEnding> EndingRecords;
    TArray<FString> Feedback;
    FBoardHighlights Highlights;
    bool bShowRecords = false;
    bool bCheckpointBlocked = false;
    int32 Revision = 0;
    int32 RecordsOffset = 0;
    float EnemyDelay = 0.0f;
    float Clock = 0.0f;
    /** Spell VFX trial driver (dev only): -SpellFXTrial=<spell id> resumes the save, casts it with the active party
     *  caster and writes real HUD screenshots to -SpellFXTrialOut=<dir>, then quits. */
    void TickSpellFXTrial(float DeltaSeconds);
    FString SpellFXTrial;
    FString SpellFXTrialOut;
    int32 SpellFXTrialStage = 0;
    int32 SpellFXTrialShot = 0;
    float SpellFXTrialClock = 0.0f;
    float SpellFXTrialNextShot = 0.0f;
    float SpellFXTrialSlowmo = 1.0f;
    float SpellFXHoldUntil = -1.0f;
    EPlaybackSpeed PlaybackSpeed = EPlaybackSpeed::Normal;
    EBoardTheme BoardTheme = EBoardTheme::Meadow;
    bool bOverview = false;

    /** Art set; unset or incomplete sets fall back to the primitive presentation. */
    UPROPERTY(EditDefaultsOnly, Category = "Theme") TSoftObjectPtr<UDiceRollTheme> ThemeAsset;
    UPROPERTY() TObjectPtr<UDiceRollTheme> Theme;
    UPROPERTY() TObjectPtr<UMaterialInterface> ThemeMaterial;
    /** Loaded once with the theme; the overview needs it, so a failed load keeps primitive boards. */
    UPROPERTY() TObjectPtr<UStaticMesh> OverviewTable;
    UPROPERTY() TArray<FExpeditionSway> Swaying;

    float Yaw = -90.0f;
    float TargetYaw = -90.0f;
    float Distance = 1600.0f;
    float TargetDistance = 1600.0f;
    FVector Focus = FVector::ZeroVector;
    float ShakeAmount = 0.0f;
    bool bCameraPlaced = false;

    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Fog;
    UPROPERTY() TObjectPtr<UPostProcessComponent> Grade;
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
    UPROPERTY() TObjectPtr<UStaticMesh> Sphere;
    UPROPERTY() TObjectPtr<UStaticMesh> Cone;
    UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;
    UPROPERTY() TObjectPtr<USceneComponent> SceneryRoot;
    /** Every runtime component this view created; destroyed together on rebuild. */
    UPROPERTY() TArray<TObjectPtr<UActorComponent>> Spawned;
    UPROPERTY() TArray<FExpeditionTile> Tiles;
    UPROPERTY() TArray<FExpeditionUnitRig> Rigs;
    UPROPERTY() TArray<FExpeditionPart> Scenery;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Flames;
    /** Per-flame base candela for themed lights; zero keeps the primitive torch strength. */
    TArray<float> FlameStrength;
    UPROPERTY() TArray<FExpeditionPart> Markers;     // 0 active, 1 selected, rest targets.
    UPROPERTY() TArray<FExpeditionPart> ShotMeshes;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> ShotLights;
    UPROPERTY() TArray<FExpeditionPart> SparkMeshes;
    // Watercolor wash for spell effects only (additive layer over the theme); null falls back to BaseMaterial.
    UPROPERTY() TObjectPtr<UMaterialInterface> WashMaterial;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> ShotWash;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> SparkWash;
    UPROPERTY() TArray<FExpeditionPart> CampParts;
    UPROPERTY() TObjectPtr<UPointLightComponent> CampLight;
    UPROPERTY() TArray<FExpeditionPart> WaystoneParts;
    UPROPERTY() TObjectPtr<UPointLightComponent> WaystoneLight;
    UPROPERTY() TArray<TObjectPtr<UAudioComponent>> Voices;
    TArray<float> VoiceEnds;
    TMap<int32, TArray<int16>> SoundCache;
    int32 SoundVariation = 0;

    FExpeditionPart MakePart(UStaticMesh* Mesh, USceneComponent* Parent, FVector Location, FVector Scale, FLinearColor Color, FRotator Rotation = FRotator::ZeroRotator);
    UPointLightComponent* MakeLight(USceneComponent* Parent, FVector Location, FLinearColor Color, float Intensity, float Radius);
    void ClearPresentation();
    void RebuildBoard();
    void BuildScenery(const FCombatEncounter& Encounter, int32 Recipe, int32 Seed);
    void BuildObstacle(FIntPoint Cell, int32 Recipe, FRandomStream& Random);
    void BuildDecor(const FCombatEncounter& Encounter, FRandomStream& Random);
    void BuildRig(const FCombatantState& Unit);
    UStaticMesh* ThemeMesh(const TSoftObjectPtr<UStaticMesh>& Entry) const;
    UStaticMesh* UnitMesh(const FCombatantState& Unit) const;
    bool WantsOverview() const;
    bool BuildThemedScenery(const FCombatEncounter& Encounter, int32 Recipe, int32 Seed);
    void BuildOverview();
    void AddSway(USceneComponent* Component, float Degrees, float Flicker);
    void UpdateSway();
    void BuildEffects(const FCombatEncounter& Encounter);
    void ApplyTheme();
    void UpdateRigs();
    void UpdateTiles();
    void UpdateMarkers();
    void UpdateEffects();
    bool PaintWash(FExpeditionPart& Part, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Wash, int32 Index, EExpeditionSpell Spell, const FLinearColor& Color);
    void UpdateCamera(float DeltaSeconds);
    void Record(const FCombatResult& Result);
    void Present(const TArray<FCombatResult>& Results);
    void ExpireVoices();
};

UCLASS()
class DNDDICEROLL_API AExpeditionGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AExpeditionGameMode();
};
