#pragma once

#include "CoreMinimal.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/BoardMetric.h"
#include "Presentation/CameraRig.h"
#include "Presentation/BoardOverlay.h"
#include "Presentation/ExpeditionPlayback.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "ExpeditionBattleView.generated.h"

class UAnimSequence;
class UAudioComponent;
class UCameraComponent;
class UExponentialHeightFogComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UPostProcessComponent;
class USceneComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class APlayerController;
class UDiceRollTheme;
struct FDiceRollCharacter;

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
    /** The hollow footprint ring on the board plane under the figure; it stays flat when the figure falls. */
    UPROPERTY() FExpeditionPart Ring;
    /** Rigged figures only (#147): the skinned mesh, driven by single-node animation, and its nine clips by EUnitPose. */
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> Skin;
    UPROPERTY() TArray<TObjectPtr<UAnimSequence>> Clips;
    /** The clip on the figure and its time. The view advances the time itself (at playback speed), so poses are deterministic. */
    EUnitPose Pose = EUnitPose::Idle;
    float PoseTime = 0.0f;
    float Stamp = 0.0f;
    int32 PoseSerial = -1;
    /** A natural return to Idle waits for a one-shot clip that is still playing. */
    bool bIdleAfter = false;
};

/** What a figure's animation shows right now (tests and tools). */
struct FUnitAnimationState
{
    bool bSkeletal = false;
    EUnitPose Pose = EUnitPose::Idle;
    /** Seconds into the clip, and the clip's length; a one-shot clip that has finished has Time == Length. */
    float Time = 0.0f;
    float Length = 0.0f;
    /** The clip asset's name on the component, empty when it has none. */
    FString Clip;
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
    FActionReach Reach;
    TArray<FIntPoint> Moves;
    TArray<FIntPoint> Path;
    TArray<int32> Targets;
    int32 SelectedId = INDEX_NONE;
    int32 HoverTargetId = INDEX_NONE;
    bool bHasHover = false;
    FIntPoint Hover = FIntPoint::ZeroValue;
    bool bFriendlyTargets = false;
    /** Combatants the hovered action would strike (area victims), from the forecast. */
    TArray<int32> Victims;
    /** Where the previewed path starts (the actor's cell). */
    bool bHasOrigin = false;
    FIntPoint Origin = FIntPoint::ZeroValue;
    /** The hold-for-full-grid accessibility key is down: faint lines over the whole board. Presentation only. */
    bool bFullGrid = false;
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
    void NewRun(const TArray<FPartyBuildSelection>& Party);
    void NewRun(EFighterSubclass Subclass = EFighterSubclass::Champion);
    void Save();
    void Resume();
    /** True while a run is in progress (route, encounter, reward or recovery) and can be put away. */
    bool CanReturnToMenu() const;
    /** Back to the start screen. Writes nothing: Resume continues from the checkpoint. */
    void ReturnToMenu();
    /** With no run in play: the checkpoint holds an unfinished run (checked at startup and on return). */
    bool HasUnfinishedCheckpoint() const { return bUnfinishedCheckpoint; }
    /** A checkpoint write failed; enemy turns are held until the player saves or resumes. */
    bool CheckpointBlocked() const { return bCheckpointBlocked; }
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
    bool IsPresenting() const { return Show.IsBusy(); }
    void SkipPresentation();
    void CycleSpeed();
    EPlaybackSpeed Speed() const { return PlaybackSpeed; }
    FString LocationName() const;
    void SetHighlights(const FBoardHighlights& Value) { Highlights = Value; }
    void PlayCue(EExpeditionCue Cue);

    // Camera and board geometry.
    void Orbit(float Degrees);
    void Zoom(float Amount);
    /**
     * Moves the camera focus in screen terms: Right and Away are reference cm (scaled by the metric). The focus is
     * clamped to the board plus a margin, and returns to the active unit when the turn changes.
     */
    void Pan(float Right, float Away);
    /** The camera focus the view settles on (view space, before the HUD offset), after the pan clamp. */
    FVector CameraFocus() const { return FocusTarget; }
    /** The camera pitch below horizontal, in degrees, for the current zoom. */
    float CameraPitch() const;
    /** The screen area the HUD leaves free; the board is fitted and centred in it. */
    void SetSafeRect(const ExpeditionCameraRig::FSafeRect& Value);
    const ExpeditionCameraRig::FSafeRect& SafeRect() const { return Safe; }
    /** Shadow-casting lights: the directional key plus the local lights chosen by SelectShadowLights. */
    int32 ShadowCastingLights() const;
    /** The attenuation radius of the first flame light, in world cm; zero when there is none. */
    float FlameLightRadius() const;
    float CameraYaw() const { return TargetYaw; }
    /**
     * Camera distances are in reference units: cm for the 110 cm r001 cell. The camera multiplies them by
     * the board metric's scale, so framing and the zoom clamp (900 to 3200) look the same at any cell size.
     * Where the camera frames the whole board by default (the distance a fresh board starts at).
     */
    float FramedDistance() const;
    float CameraDistance() const { return TargetDistance; }
    /** Capture support: put the camera at a pose at once, with no interpolation from the last one. */
    void SnapCamera(float InYaw, float InDistance);
    /** Capture support: settle the camera on its current targets at once. */
    void SnapCameraToTargets() { bCameraPlaced = false; }
    FTransform CameraTransform() const;
    float CameraFieldOfView() const;
    bool UsesTheme() const { return Theme != nullptr; }
    /** True when this board draws the contextual grid (themed battle boards); primitive boards tint cube tiles instead. */
    bool DrawsContextualGrid() const { return !OverlaySets.IsEmpty(); }
    /** The per-cell state the contextual grid was last built from (also kept on primitive boards, where only the tiles show it). */
    const BoardOverlay::FMask& OverlayMask() const { return OverlayState; }
    /** Bars drawn for a style in the last overlay update; zero when the board has no contextual grid. */
    int32 OverlayBars(BoardOverlay::EStyle Style) const;
    /** True while the full-grid lines are drawn. */
    bool ShowingFullGrid() const { return bFullGridShown; }
    /** Per-cell tile meshes on the board: zero on themed boards, which use the world's ground as the floor. */
    int32 TileCount() const { return Tiles.Num(); }
    /** World diameter in cm of the footprint ring under a combatant, read from the ring's geometry; zero if it has none. */
    float FootprintRingDiameterCm(int32 UnitId) const;
    /** The root component of a combatant's figure (body and parts hang below it), or null if it has none. */
    USceneComponent* UnitRigRoot(int32 UnitId) const;
    /** Capture support: leave enemy turns unplayed so a look at a checkpoint never submits a command. */
    void HoldEnemyTurns(bool bHold) { bHoldEnemyTurns = bHold; }
    /** Capture support: adapt exposure at once so a shot does not depend on the one before it. */
    void PrepareExposureForCapture();
    /** The one board metric: cell size, board top, pointer plane and figure height. Every cell-to-world conversion uses it. */
    const FBoardMetric& Metric() const { return BoardMetric; }
    /** Replaces the metric and rebuilds the board at the new scale (tests and tools; the theme normally supplies it). */
    void SetBoardMetric(const FBoardMetric& Value);
    /** Draws the static r001 meshes instead of the rigged characters (what -ExpeditionStaticFigures does at startup) and rebuilds. */
    void UseStaticFigures();
    /** How many figures on the board are rigged characters. */
    int32 SkeletalFigureCount() const;
    /** The animation shown on a combatant's figure; bSkeletal is false for static and primitive figures. */
    FUnitAnimationState UnitAnimation(int32 UnitId) const;
    /** Switches to the engine-primitive presentation (what -ExpeditionPrimitives does at startup) and rebuilds. */
    void UsePrimitives();
    FVector CellPosition(FIntPoint Cell) const;
    FVector BoardPosition(FVector2D Cell, float Height = 0.0f) const;
    /** Where a combatant currently appears, including playback motion. */
    FVector UnitPosition(int32 Id, float Height = 0.0f) const;
    bool CursorCell(APlayerController* Controller, FIntPoint& Cell) const;
    /** The cell a world-space ray crosses on the pointer plane; false when it misses the board. CursorCell is this plus the pointer. */
    bool CellAtRay(const FVector& Origin, const FVector& Direction, FIntPoint& Cell) const;

private:
    FExpeditionSession Session;
    FExpeditionPlayback Show;
    TArray<FExpeditionEnding> EndingRecords;
    TArray<FString> Feedback;
    FBoardHighlights Highlights;
    bool bShowRecords = false;
    bool bCheckpointBlocked = false;
    bool bUnfinishedCheckpoint = false;
    int32 Revision = 0;
    int32 RecordsOffset = 0;
    float EnemyDelay = 0.0f;
    float Clock = 0.0f;
    EPlaybackSpeed PlaybackSpeed = EPlaybackSpeed::Normal;
    EBoardTheme BoardTheme = EBoardTheme::Meadow;
    bool bOverview = false;
    bool bHoldEnemyTurns = false;
    /** Rigged characters are used when the theme defines them; off for the static-figure comparison. */
    bool bSkeletalFigures = true;

    /** Art set; unset or incomplete sets fall back to the primitive presentation. */
    UPROPERTY(EditDefaultsOnly, Category = "Theme") TSoftObjectPtr<UDiceRollTheme> ThemeAsset;
    UPROPERTY() TObjectPtr<UDiceRollTheme> Theme;
    UPROPERTY() TObjectPtr<UMaterialInterface> ThemeMaterial;
    FBoardMetric BoardMetric;
    /** Loaded once with the theme; the overview needs it, so a failed load keeps primitive boards. */
    UPROPERTY() TObjectPtr<UStaticMesh> OverviewTable;
    UPROPERTY() TArray<FExpeditionSway> Swaying;

    float Yaw = -90.0f;
    float TargetYaw = -90.0f;
    float Distance = 1600.0f;
    float TargetDistance = 1600.0f;
    FVector Focus = FVector::ZeroVector;
    /** Where the focus settles: the board/unit focus plus the clamped pan. View space. */
    FVector FocusTarget = FVector::ZeroVector;
    FVector2D PanOffset = FVector2D::ZeroVector;
    int32 LastFocusId = INDEX_NONE;
    /** Whether the map's sun cast shadows before the battle look took it over (the tabletop restores it). */
    TOptional<bool> StarterSunShadows;
    ExpeditionCameraRig::FSafeRect Safe;
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
    /** One bar set per BoardOverlay::EStyle, built for themed battle boards only. */
    UPROPERTY() TArray<FExpeditionPart> OverlaySets;
    /** Translucent unlit "Color" x "Opacity" material for the contextual grid; absent until created in the editor. */
    UPROPERTY() TObjectPtr<UMaterialInterface> OverlayMaterial;
    BoardOverlay::FMask OverlayState;
    TArray<FIntPoint> OverlayPath;
    FIntPoint OverlayOrigin = FIntPoint::ZeroValue;
    bool bFullGridShown = false;
    bool bOverlayBuilt = false;
    TArray<int32> OverlayCounts;
    UPROPERTY() TArray<FExpeditionUnitRig> Rigs;
    UPROPERTY() TArray<FExpeditionPart> Scenery;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Flames;
    /** Per-flame base candela for themed lights; zero keeps the primitive torch strength. */
    TArray<float> FlameStrength;
    UPROPERTY() TArray<FExpeditionPart> Markers;     // 0 active, 1 selected, rest targets.
    UPROPERTY() TArray<FExpeditionPart> ShotMeshes;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> ShotLights;
    UPROPERTY() TArray<FExpeditionPart> SparkMeshes;
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
    /** Scenery lives under a root scaled by the metric (reference units); the overview table is unscaled. */
    float SceneScale() const { return bOverview ? 1.0f : BoardMetric.Scale(); }
    /** Scenery-local position of a board point: reference cm, because the scenery root is scaled. */
    FVector SceneBoard(FVector2D Cell, float ReferenceHeight = 0.0f) const { return BoardMetric.ReferencePosition(Cell, ReferenceHeight); }
    void ClearPresentation();
    void RebuildBoard();
    void BuildScenery(const FCombatEncounter& Encounter, int32 Recipe, int32 Seed);
    void BuildObstacle(FIntPoint Cell, int32 Recipe, FRandomStream& Random);
    void BuildDecor(const FCombatEncounter& Encounter, FRandomStream& Random);
    void BuildRig(const FCombatantState& Unit);
    UStaticMesh* ThemeMesh(const TSoftObjectPtr<UStaticMesh>& Entry) const;
    UStaticMesh* UnitMesh(const FCombatantState& Unit) const;
    /** The theme's rigged character for a combatant, or null (no theme, static figures, or no definition for this unit). */
    const FDiceRollCharacter* UnitRigged(const FCombatantState& Unit) const;
    bool BuildSkeletalFigure(FExpeditionUnitRig& Rig, USceneComponent* Body, const FCombatantState& Unit);
    void ShowPose(FExpeditionUnitRig& Rig, EUnitPose Pose, bool bSnap);
    void UpdateAnimation(FExpeditionUnitRig& Rig, const FUnitDisplay& Unit);
    bool WantsOverview() const;
    bool BuildThemedScenery(const FCombatEncounter& Encounter, int32 Recipe, int32 Seed);
    void BuildOverview();
    void AddSway(USceneComponent* Component, float Degrees, float Flicker);
    void UpdateSway();
    void BuildEffects(const FCombatEncounter& Encounter);
    void ApplyTheme();
    void UpdateRigs();
    void UpdateTiles();
    void BuildOverlay();
    void UpdateOverlay();
    /** A hollow ring of thin bars, 100 cm across at unit scale; the component scale sets the diameter. */
    FExpeditionPart MakeRing(USceneComponent* Parent, FVector Location, FLinearColor Color);
    UInstancedStaticMeshComponent* MakeBarSet(FExpeditionPart& Part, USceneComponent* Parent, FLinearColor Color, float Opacity);
    void UpdateMarkers();
    void UpdateEffects();
    void UpdateCamera(float DeltaSeconds);
    /** The unpanned focus: the board centre leaned toward the acting unit (view space). */
    FVector BaseFocus() const;
    /** Gives shadows to the two flame lights nearest the board centre and none to the rest. */
    void SelectShadowLights();
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
