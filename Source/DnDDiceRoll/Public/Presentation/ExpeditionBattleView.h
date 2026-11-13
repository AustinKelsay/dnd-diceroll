#pragma once

#include "CoreMinimal.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/BoardMetric.h"
#include "Presentation/ExpeditionPalette.h"
#include "Presentation/CameraRig.h"
#include "Presentation/BoardOverlay.h"
#include "Presentation/ExpeditionPlayback.h"
#include "Presentation/FoeLook.h"
#include "Presentation/VigilKit.h"
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
enum class EDiceRollKit : uint8;
struct FDiceRollCharacter;
struct FDiceRollWorldKit;

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
    /** Footprint rings and markers drawn from the shared ring bank (#144) have no component of their own: the bank slot. */
    int32 RingSlot = INDEX_NONE;
    void SetColor(const FLinearColor& Color);
    /**
     * A hit's flash on the figure's rim (#189): the material's "RimColor" turns toward a warm white (ExpeditionFlash::RimColor). Materials without
     * "Rim" and "RimColor" (the engine primitives) are left alone. The first call remembers the look the material had.
     */
    void SetFlash(float Flash);
    /** The rim look the material had before any flash, read on the first SetFlash. */
    float BaseRim = 0.0f;
    FLinearColor BaseRimColor = FLinearColor::White;
    float ShownFlash = 0.0f;
    enum class ERim : uint8 { Unread, Present, Absent } RimState = ERim::Unread;
};

/**
 * One instanced group (#144): every instance shares a mesh and a material, so the group is one component and one draw.
 * Tint and glow ride in per-instance custom data (CustomFloats floats per instance), not in per-part material instances.
 */
USTRUCT()
struct FExpeditionGroup
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UStaticMesh> Mesh;
    UPROPERTY() TObjectPtr<UMaterialInterface> Material;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Component;
    bool bCastShadow = true;
    int32 CustomFloats = 0;
    /** A plain group (engine shape, BasicShapeMaterial, no custom data) is keyed by the one colour its material instance carries. */
    bool bPlain = false;
    FLinearColor Colour = FLinearColor::White;
    /** Instances changed since the last flush; the render thread is told once per frame. */
    bool bDirty = false;
    /** Instances moved continuously this frame and last frame. Their previous transforms give the renderer motion vectors (TSR
     * reprojects them like moving loose components); an instance that stops moving has its previous transform reset at flush. */
    TArray<int32> Moved;
    TArray<int32> MovedLast;
};

/** Where an instance lives: its group and its index in the group's component. */
struct FExpeditionInstance
{
    int32 Group = INDEX_NONE;
    int32 Index = INDEX_NONE;
    bool IsValid() const { return Group != INDEX_NONE && Index != INDEX_NONE; }
};

/**
 * A placed prop: an instance in a group, or a loose component when instancing is off (primitive look, tests comparing the two).
 * Local is the transform under the scenery root, so a position never needs the component.
 */
struct FExpeditionProp
{
    FExpeditionInstance Instance;
    UStaticMeshComponent* Loose = nullptr;
    FTransform Local;
    FRotator Rotation = FRotator::ZeroRotator;
};

/** One pooled effect piece (a spark, a shot, a camp stone): a loose part, or an instance in a shared group. */
USTRUCT()
struct FExpeditionSlot
{
    GENERATED_BODY()
    UPROPERTY() FExpeditionPart Loose;
    FExpeditionInstance Instance;
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector Scale = FVector::OneVector;
    FLinearColor Colour = FLinearColor::Transparent;
    bool bVisible = false;
    bool bPlaced = false;
};

/** The shared footprint-ring and marker bank (#144): 24 bars per ring slot in one component, colour per instance. */
struct FExpeditionRingState
{
    FVector Centre = FVector::ZeroVector;
    float Scale = 0.0f;
    FLinearColor Colour = FLinearColor::Transparent;
    bool bVisible = false;
    bool bPlaced = false;
};

USTRUCT()
struct FExpeditionUnitRig
{
    GENERATED_BODY()
    int32 Id = INDEX_NONE;
    float Phase = 0.0f;
    UPROPERTY() TObjectPtr<USceneComponent> Root;
    UPROPERTY() TArray<FExpeditionPart> Parts;
    ECombatSide Side = ECombatSide::Party;
    /** Whether the parts currently render into custom depth (the silhouette pass); dead figures do not. */
    bool bSilhouette = false;
    /** The hollow footprint ring on the board plane under the figure; it stays flat when the figure falls. */
    UPROPERTY() FExpeditionPart Ring;
    /** Rigged figures only (#147): the skinned mesh, driven by single-node animation, and its ten clips by EUnitPose. */
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> Skin;
    UPROPERTY() TArray<TObjectPtr<UAnimSequence>> Clips;
    /** The clip on the figure and its time. The view advances the time itself (at playback speed), so poses are deterministic. */
    EUnitPose Pose = EUnitPose::Idle;
    float PoseTime = 0.0f;
    float Stamp = 0.0f;
    int32 PoseSerial = -1;
    /** A natural return to Idle waits for a one-shot clip that is still playing. */
    bool bIdleAfter = false;
    /** Capture support (#150): the clip and time are held by PreviewPose, so UpdateAnimation leaves the figure alone. */
    bool bPosePreview = false;
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
    /** Instanced props (#144) sway through their instance transform instead; Location is where it stands under the scenery root. */
    FExpeditionInstance Instance;
    FVector Location = FVector::ZeroVector;
    FRotator Base = FRotator::ZeroRotator;
    FVector Scale = FVector::OneVector;
    float Phase = 0.0f;
    float Degrees = 0.0f;
    float Flicker = 0.0f;
};

/** A scatter, mist or light-shaft piece the Vigil kit placed (#145): its plan and where it stands in the view actor's space (cm). */
struct FPlacedDressing
{
    VigilKit::FDressing Plan;
    FVector Position = FVector::ZeroVector;
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
    /**
     * Submits a party command and plays it. Returns whether the rules accepted it. A rejection is explained in the log by
     * the first blocker of the action's facts (ExpeditionStyle::BlockerText) when they have one, else by the rules' own text.
     */
    bool Execute(const FCombatCommand& Command);
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
    /** The overview scene as last built (#149): the table and each diorama by mesh name, and the candle light positions in table cm; empty off the tabletop. */
    const TArray<FName>& OverviewMeshes() const { return OverviewDrawnMeshes; }
    const TArray<FVector>& OverviewLights() const { return OverviewDrawnLights; }
    /** True when the overview was drawn from the r002 set (war table and Root, Vigil and Vault dioramas). */
    bool OverviewIsR002() const { return bOverview && bOverviewR002; }
    const TArray<FExpeditionEnding>& Records() const { return EndingRecords; }
    static constexpr int32 RecordsPerPage = 4;
    int32 RecordOffset() const { return RecordsOffset; }
    void PageRecords(int32 Direction);
    int32 PresentationRevision() const { return Revision; }
    /** Explicit local storage selection before play; automation uses isolated fixtures. */
    void ConfigureStorage(const FString& Path);

    // Presentation.
    const FExpeditionPlayback& Playback() const { return Show; }
    /** Capture and test support (#190): a landed roll card that stays up until cleared. */
    void PreviewRoll(const FDiceTray& Tray, float Age = FDiceTray::TumbleSeconds + 0.5f) { Show.PreviewTray(Tray, Age); }
    /** True from the first frame of a roll until its card is gone: the left margin is the card's, so a combat hint waits (#190). */
    bool RollCardUp() const { return Show.Tray().bVisible && !Show.Tray().Dice.IsEmpty(); }
    void ClearRollPreview() { Show.ClearTrayPreview(); }
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
     * Moves the camera focus in screen terms: Right and Away are reference cm (scaled by the metric). The view may
     * show at most PanMarginCells (1.5) past the board's outer tile edge, so an axis on which the view is wider than the
     * board stays at the board centre, and nothing pans while the whole board is in view. The pan returns to the active
     * unit when the turn changes.
     */
    void Pan(float Right, float Away);
    /** The camera focus the view settles on (view space, before the HUD offset), after the pan clamp. */
    FVector CameraFocus() const { return FocusTarget; }
    /** Whether the whole board is inside the safe rect from the pose the camera renders this frame (then nothing pans). */
    bool WholeBoardInView() const;
    /** Shakes the camera (hits and crits call this through playback); it decays over about Amount / 1.8 seconds. */
    void Shake(float Amount) { ShakeAmount = FMath::Max(ShakeAmount, Amount); }
    /** The camera pitch below horizontal, in degrees, for the current zoom. */
    float CameraPitch() const;
    /** The screen area the HUD leaves free; the board is fitted and centred in it. */
    void SetSafeRect(const ExpeditionCameraRig::FSafeRect& Value);
    const ExpeditionCameraRig::FSafeRect& SafeRect() const { return Safe; }
    /** Shadow-casting lights: the directional key plus the local lights chosen by SelectShadowLights. */
    int32 ShadowCastingLights() const;
    /** World positions of the shadow-casting local lights, in build order: a stable identity for the chosen set. */
    TArray<FVector> ShadowedLightPositions() const;
    /** The attenuation radius of the first flame light, in world cm; zero when there is none. */
    float FlameLightRadius() const;
    float CameraYaw() const { return TargetYaw; }
    /**
     * Camera distances are in reference units: cm for the 110 cm r001 cell. The camera multiplies them by
     * the board metric's scale, so framing and the zoom clamp (900 to 6400) look the same at any cell size.
     * FramedDistance() is the pitch anchor: the nearest zoom at which the board's four corners sit inside the
     * HUD-safe rect from the home yaw (-90), at 45 degrees. It no longer is the default zoom (see below), but the pitch
     * ramp (35 / 45 / 60 degrees) and the far stop stay tied to it.
     */
    float FramedDistance() const;
    /**
     * The default zoom for a board seen from a yaw (#148 gate 2): the closer of the whole-board fit (WholeBoardDistance) and
     * the distance at which a standing figure at the focus is 88 px tall at 1080 viewport pixels (FigureDistance), clamped to
     * the zoom range. A board that fits at the closer figure distance keeps its whole-board fit.
     */
    float FramedDistance(float ForYaw) const;
    /** The nearest zoom from a yaw at which the board's corners fit the safe rect (clamped to the zoom range); the old default, and where the far pose starts. */
    float WholeBoardDistance(float ForYaw) const;
    /** The zoom at which a standing figure at the focus is 88 px tall at 1080 viewport pixels (about 59 px at 720), on the zoom-tied pitch curve. */
    float FigureDistance() const;
    /** The far pose: the end of the pitch ramp (FarStop of the anchor), never nearer than the whole-board fit from that yaw. */
    float FarFramedDistance(float ForYaw) const;
    /** Whether the whole-board fit from a yaw lies inside the zoom range (false only for a board too large to fit). */
    bool FramedFits(float ForYaw) const;
    /** Whether the zoom rests at the default for the current yaw. Orbit keeps such a camera at the default; a zoom the player chose is left alone. */
    bool AtFramedZoom() const;
    float CameraDistance() const { return TargetDistance; }
    /** Capture support: put the camera at a pose at once, with no interpolation from the last one. */
    void SnapCamera(float InYaw, float InDistance);
    /**
     * Capture support (#189): shows a hit on a combatant with no command and no rules change (see FExpeditionPlayback::PreviewImpact)
     * and holds its flash at the peak every tick until ClearImpactPreview, so a capture frame is the worst case, not a decaying one.
     */
    void PreviewImpact(int32 TargetId, EImpactPreview Effect, FVector2D From, EImpactMoment Moment = EImpactMoment::Landing, EAdventurerRole Shooter = EAdventurerRole::None);
    void ClearImpactPreview();
    /**
     * Capture support (#150): holds a rigged figure in a clip at a time (seconds, clamped to the clip) with no command and no rules
     * change, until ClearPosePreview hands it back to playback. False when the unit has no skinned figure.
     */
    bool PreviewPose(int32 UnitId, EUnitPose Pose, float Seconds);
    void ClearPosePreview();
    int32 ImpactPreviewId() const { return ImpactTargetId; }
    /** Capture support: settle the camera on its current targets at once. */
    void SnapCameraToTargets() { bCameraPlaced = false; }
    FTransform CameraTransform() const;
    float CameraFieldOfView() const;
    bool UsesTheme() const { return Theme != nullptr; }
    /** True when this board draws the contextual grid (themed battle boards); primitive boards tint cube tiles instead. */
    bool DrawsContextualGrid() const { return !OverlaySets.IsEmpty() || OverlayBase != INDEX_NONE; }
    /** The per-cell state the contextual grid was last built from (also kept on primitive boards, where only the tiles show it). */
    const BoardOverlay::FMask& OverlayMask() const { return OverlayState; }
    /** Bars drawn for a style in the last overlay update; zero when the board has no contextual grid. */
    int32 OverlayBars(BoardOverlay::EStyle Style) const;
    /** True while the full-grid lines are drawn. */
    bool ShowingFullGrid() const { return bFullGridShown; }
    /** True when this board is built from the Drowned Vigil world kit r002 (a Vigil board of a theme whose kit is switched on). */
    bool UsesVigilKit() const { return bVigilKit; }
    /** True when this board is built from the Root of the World kit r002 (#149): a forest board of a theme whose Root kit is switched on. */
    bool UsesRootKit() const { return bRootKit; }
    /** True when this board is built from the Vault world kit r002 (#149): the finale of a theme whose Vault kit is switched on and complete. */
    bool UsesVaultKit() const { return bVaultKit; }
    /** The world kit a board theme asks for: the Root on Meadow, the Vigil on Ruins, the Vault on the finale (UDiceRollTheme::KitFor falls back to the Vigil kit when the Vault is incomplete). */
    static EDiceRollKit KitKindFor(EBoardTheme Theme);
    /** The Waystone's light: the number of lights (0 or 1), where it stands in world space, and whether the legacy pedestal is drawn. On a Vault-kit board the light stands at the apse Waystone and there is no pedestal. */
    int32 WaystoneLightCount() const { return WaystoneLight ? 1 : 0; }
    FVector WaystoneLightLocation() const;
    bool WaystoneDrawsPedestal() const { return WaystoneSlots.Num() == 2 && WaystoneSlots[0].bVisible; }
    /** Builds the current board with another theme (capture and automation support: a copy of the shipped theme with the kit switched off or thinned). Null is the primitive presentation. */
    void UseTheme(UDiceRollTheme* Value);
    /**
     * The world mesh's scale in world space: 1 for an r002 world kit, which is authored at the true 150 cm cell and placed
     * unscaled; the board metric's scale (cell / 110 cm) for the r001 art. Zero when the board has no themed world.
     */
    float WorldMeshScale() const;
    /** The instance tint to draw a kit mesh with: white when instanced (its material instance applies it), the instance Color when loose. */
    FLinearColor KitTint(const UStaticMesh* Mesh) const;
    /** What the kit dressed this board with: scatter, mist and shaft pieces actually placed, with their cells and view-space positions. */
    const TArray<FPlacedDressing>& PlacedDressing() const { return Dressing; }
    /** The braziers the themed board placed (#148), in board cells; empty on the primitive look and the tabletop. */
    const TArray<FVector2D>& PlacedBraziers() const { return BrazierCells; }
    /** Obstacle pieces placed on blocked cells (variants and arches) and arches among them, in the last build. */
    int32 PlacedObstacles() const { return ObstaclePieces; }
    int32 PlacedArches() const { return ArchPieces; }
    /** The obstacle kind the last build drew (0 stones, 1 pillars, 2 walls), from FExpeditionRun::Encounter for the current route (#188). */
    int32 DrawnObstacleKind() const { return DrawnKind; }
    /** The board theme the last build chose: the Vault for the finale stage, otherwise by road. */
    EBoardTheme DrawnBoardTheme() const { return BoardTheme; }
    /** Per blocked cell, the variant index its obstacle used (kit boards only; -1 for a cell under an arch). */
    const TMap<FIntPoint, int32>& ObstacleVariants() const { return VariantsByCell; }
    /** Where each kit obstacle piece stands in the view actor's space: per blocked cell, or the shared-edge midpoint for both cells of an arch. */
    const TMap<FIntPoint, FVector>& ObstacleSpots() const { return SpotsByCell; }
    /** True when the board has the near-architecture occluder. */
    bool HasOccluder() const;
    /**
     * Opacity (OccluderFade::MinOpacity to 1) the sight-line part of the occluder fade has at a world-space point right
     * now: the lower of the fade for the camera-to-focus and the camera-to-active-unit sight lines. 1 when the board has no occluder.
     */
    float OccluderOpacityAt(const FVector& WorldPoint) const;
    /** The same for the board part: the point covers the board when the camera's ray through it lands on the grown board rectangle. */
    float OccluderBoardOpacityAt(const FVector& WorldPoint) const;
    /** What the shader is given: the board rectangle grown by OccluderFade::BoardMarginCells (world XY, min and max) and the board-top height. */
    void OccluderBoard(FVector2D& Min, FVector2D& Max, float& PlaneZ) const;
    /** World points the occluder protects: the camera, the camera focus and the active unit's chest. */
    void OccluderSight(FVector& Eye, FVector& FocusPoint, FVector& UnitPoint) const;
    /** Per-cell tile meshes on the board: zero on themed boards, which use the world's ground as the floor. */
    int32 TileCount() const { return Tiles.Num(); }
    /** World diameter in cm of the footprint ring under a combatant, read from the ring's geometry; zero if it has none. */
    float FootprintRingDiameterCm(int32 UnitId) const;
    /** Custom-depth stencil values the figure silhouette material reads: party 1, enemy 2 (0 is no figure). */
    static constexpr int32 PartyStencil = 1;
    static constexpr int32 EnemyStencil = 2;
    /** The stencil value on a combatant's figure parts when they all render custom depth; 0 when none do (dead, or no rig); -1 when mixed. */
    int32 FigureStencil(int32 UnitId) const;
    /** Whether the silhouette post-process material is in the grade's blendables (it is optional; absent when the asset does not exist). */
    bool HasSilhouetteBlendable() const;
    /**
     * Test-only (#189): the tint last pushed to each of a combatant's figure parts (for a rigged figure, one per skin material slot) and to its
     * footprint ring, as the materials hold it. False when the combatant has no figure. A ring-bank ring reports the bank's colour for its slot.
     */
    bool UnitShownColors(int32 UnitId, TArray<FLinearColor>& OutParts, FLinearColor& OutRing) const;
    /**
     * Test-only (#150 batch 4): the palette a rigged party figure's skin material holds right now (its subclass slots and, when its class appears more
     * than once in the party, the accent slot), read back from the material parameters. False when the combatant is not a rigged figure or its material
     * does not carry the palette parameters.
     */
    bool UnitPalette(int32 UnitId, ExpeditionPalette::FPalette& OutPalette) const;
    /** Test-only (#189): the intensity (candelas) each spell bolt's point light was last set to, in bolt order (zero for an idle light or an arrow). */
    TArray<float> BoltLightIntensities() const;
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
    /**
     * Capture and test support (#150 foe batch): draws the board's enemies, in encounter order, as these looks instead of the ones their names give, and rebuilds.
     * A checkpoint cannot carry a renamed enemy (a restored run is checked against the generated roster), and the skirmisher, brute and warden are not generated
     * until #188, so the gallery shows them through this: presentation only, nothing is written to the run, the rules or the RNG. -ExpeditionFoeLooks=Skirmisher,Brute,Archer
     * does the same at startup (the FoeRigs keys, ExpeditionFoe::RigKey); an empty array restores the names' own looks.
     */
    void SetFoeLooks(const TArray<EFoeLook>& Looks);
    /** The look an enemy is drawn as: the override of its place in the encounter when there is one, otherwise ExpeditionFoe::LookOf. */
    EFoeLook FoeLookFor(const FCombatantState& Unit) const;
    /** How many figures on the board are rigged characters. */
    int32 SkeletalFigureCount() const;
    /** The animation shown on a combatant's figure; bSkeletal is false for static and primitive figures. */
    FUnitAnimationState UnitAnimation(int32 UnitId) const;
    /** Switches to the engine-primitive presentation (what -ExpeditionPrimitives does at startup) and rebuilds. */
    void UsePrimitives();
    /**
     * Test-only presentation data (#144). Draws every static part as its own component again (what the primitive look always does)
     * instead of instancing, and rebuilds; false restores the instanced groups. Only the number of components differs.
     */
    void SetLooseParts(bool bLoose);
    /** True when themed parts are drawn from shared instanced groups (the default for a themed board with the instancing materials). */
    bool UsesInstancedParts() const { return bInstancedParts; }
    /** The shared effect material (sparks, shots, camp, waystone) was found, so those pieces are instanced. */
    bool UsesInstancedEffects() const { return bInstancedEffects; }
    /** The footprint rings and markers are drawn from one shared ring bank. */
    bool UsesRingBank() const { return RingBank != nullptr; }
    /**
     * Test-only: every primitive component (static, instanced and skeletal meshes) this board built, the contextual grid included.
     * InstancedGroupCount is how many of them are shared instanced groups.
     */
    int32 PresentationComponentCount() const;
    int32 InstancedGroupCount() const { return Groups.Num(); }
    /** Test-only: the logical parts of the board (props, figure parts, rings and pooled effect pieces), the same whether instanced or loose. */
    int32 LogicalPartCount() const { return LogicalParts; }
    /** Test-only: logical scenery props only (everything placed through the prop path), the same whether instanced or loose. */
    int32 LogicalPropCount() const { return LogicalProps; }
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
    int32 DrawnKind = 0;
    bool bOverview = false;
    bool bHoldEnemyTurns = false;
    /** Capture support (#189): the combatant whose flash is held at its peak, or INDEX_NONE. */
    int32 ImpactTargetId = INDEX_NONE;
    /** Rigged characters are used when the theme defines them; off for the static-figure comparison. */
    bool bSkeletalFigures = true;
    /** SetFoeLooks: the look of each enemy in encounter order; empty uses the names. */
    TArray<EFoeLook> FoeLookOrder;
    /** The accent slot of each party member whose class appears more than once in the party (ExpeditionPalette::AccentSlots), built with the rigs. */
    TMap<int32, int32> PartyAccents;
    /** Subclass palettes and party accents on rigged party figures (off with -ExpeditionNoPalettes: the class colours, a measurement baseline). */
    bool bPalettes = true;
    /** Party-slot accents on the palettes (off with -ExpeditionNoAccents: the same palettes without the accent, a measurement baseline). */
    bool bAccents = true;

    /** Art set; unset or incomplete sets fall back to the primitive presentation. */
    UPROPERTY(EditDefaultsOnly, Category = "Theme") TSoftObjectPtr<UDiceRollTheme> ThemeAsset;
    UPROPERTY() TObjectPtr<UDiceRollTheme> Theme;
    UPROPERTY() TObjectPtr<UMaterialInterface> ThemeMaterial;
    /** Post-process material that draws hidden figures' silhouettes; optional (see AttachSilhouette). */
    UPROPERTY() TObjectPtr<UMaterialInterface> SilhouetteMaterial;
    FBoardMetric BoardMetric;
    /** Loaded once with the theme; the overview needs it, so a failed load keeps primitive boards. */
    UPROPERTY() TObjectPtr<UStaticMesh> OverviewTable;
    /** The rest of the overview set (#149), loaded with the table: the dioramas, and whether the set is the r002 one. */
    UPROPERTY() TObjectPtr<UStaticMesh> OverviewRoot;
    UPROPERTY() TObjectPtr<UStaticMesh> OverviewVigil;
    UPROPERTY() TObjectPtr<UStaticMesh> OverviewVault;
    bool bOverviewR002 = false;
    /** What the overview scene drew last: the table and each diorama by mesh name, and the candle lights. Read by tests; presentation only. */
    TArray<FName> OverviewDrawnMeshes;
    TArray<FVector> OverviewDrawnLights;
    /** Loads the overview set from the theme (the r002 set or the r001 one, never a mix) into the members above. */
    void LoadOverviewSet();
    UPROPERTY() TArray<FExpeditionSway> Swaying;
    /** Parts are instanced (themed look, default on); off draws them loose for the before/after comparison. */
    bool bLooseParts = false;
    bool bInstancedParts = false;
    bool bInstancedEffects = false;
    int32 LogicalParts = 0;
    int32 LogicalProps = 0;

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
    /** The home-yaw fit, kept until the board, metric or safe rect changes. */
    mutable ExpeditionCameraRig::FFramedCache FramedCache;
    float ShakeAmount = 0.0f;
    bool bCameraPlaced = false;
    /** The camera rests at (or is easing between) framed distances: the rendered distance never drops under the fit for the rendered yaw. A player zoom clears it. */
    bool bFramed = false;

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
    /** Lit opaque material that takes its colour from per-instance custom data (sparks, shots, camp, waystone, plain props); optional. */
    UPROPERTY() TObjectPtr<UMaterialInterface> EffectMaterial;
    /** Every instanced group of this board; one component each. */
    UPROPERTY() TArray<FExpeditionGroup> Groups;
    /**
     * The shared bar bank: one component holding every footprint ring and marker (24 bars each, first) and then the contextual grid's
     * bars (all nine styles, after OverlayBase). Null when rings are loose.
     */
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> RingBank;
    int32 RingGroup = INDEX_NONE;
    /** First instance of the grid's bars in the bank, or INDEX_NONE when the grid draws as one bar set per style. */
    int32 OverlayBase = INDEX_NONE;
    TArray<FExpeditionRingState> RingStates;
    /** Pooled effect pieces: instances in shared groups with the effect material, loose parts without it (and in the primitive look). */
    UPROPERTY() TArray<FExpeditionSlot> ShotSlots;
    UPROPERTY() TArray<FExpeditionSlot> SparkSlots;
    UPROPERTY() TArray<FExpeditionSlot> CampSlots;
    UPROPERTY() TArray<FExpeditionSlot> WaystoneSlots;
    BoardOverlay::FMask OverlayState;
    /** The inputs the mask was last built from, and reusable scratch for this frame's victims; an unchanged frame allocates nothing. */
    FBoardHighlights OverlayInput;
    TArray<FIntPoint> OverlayVictims;
    TArray<FIntPoint> OverlayVictimsShown;
    bool bFullGridShown = false;
    bool bOverlayBuilt = false;
    TArray<int32> OverlayCounts;
    UPROPERTY() TArray<FExpeditionUnitRig> Rigs;
    UPROPERTY() TArray<FExpeditionPart> Scenery;
    /** Vigil kit (#145): the dithered occluder, and a record of what was placed. */
    UPROPERTY() FExpeditionPart Occluder;
    /** The board rectangle last pushed to the occluder material (min X, min Y, max X, max Y) and plane Z; pushed only when it changes. */
    FVector4 OccluderBoardSent = FVector4(0.0, 0.0, 0.0, 0.0);
    float OccluderPlaneSent = 0.0f;
    bool bOccluderBoardSent = false;
    bool bVigilKit = false;
    bool bRootKit = false;
    bool bVaultKit = false;
    /** The far-zoom grade last applied (0 at the default zoom, 1 at the far stop); negative until the first update. Vault boards only. */
    float AppliedFarGrade = -1.0f;
    void UpdateFarGrade();
    /** True when every piece of the Vault kit loads; checked before anything is placed. */
    bool VaultPiecesLoad(const FDiceRollWorldKit& Kit) const;
    /** Where the apse Waystone stands in scenery-local space (a Vault-kit board), and the light's gain there. */
    FVector ApseLocal = FVector::ZeroVector;
    float WaystoneGain = 1.0f;
    /** The world mesh of the last themed build. */
    /** The world mesh as placed (loose or instanced), for WorldMeshScale. Its component is owned by Scenery or a group. */
    FExpeditionProp WorldProp;
    TArray<FPlacedDressing> Dressing;
    /** Where the themed board's braziers stand, in board cells (BrazierRing::Plan), in placement order. */
    TArray<FVector2D> BrazierCells;
    TMap<FIntPoint, int32> VariantsByCell;
    TMap<FIntPoint, FVector> SpotsByCell;
    int32 ObstaclePieces = 0;
    int32 ArchPieces = 0;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Flames;
    /** Per-flame base candela for themed lights; zero keeps the primitive torch strength. */
    TArray<float> FlameStrength;
    UPROPERTY() TArray<FExpeditionPart> Markers;     // 0 active, 1 selected, rest targets.
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> ShotLights;
    UPROPERTY() TObjectPtr<UPointLightComponent> CampLight;
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
    // Instanced groups (#144).
    /** The group for a mesh and material, created on first use (under Parent, the scenery root by default). */
    int32 GroupFor(UStaticMesh* Mesh, UMaterialInterface* Material, bool bCastShadow, int32 CustomFloats, USceneComponent* Parent = nullptr);
    /** The group of an engine shape in one colour (BasicShapeMaterial cannot read custom data, so one material instance per colour). */
    int32 PlainGroupFor(UStaticMesh* Mesh, const FLinearColor& Colour, bool bCastShadow);
    FExpeditionInstance PlaceInstance(int32 Group, const FTransform& Local, TConstArrayView<float> Data);
    /** Rewrites an instance's transform and custom data; marks the group for the end-of-frame flush. */
    /** bMotion: the instance moved continuously since last frame (it draws a velocity); false for show, hide or a reuse elsewhere. */
    void UpdateInstance(const FExpeditionInstance& Instance, const FTransform& Local, TConstArrayView<float> Data, bool bMotion = true);
    /** Moves instances of a group. bMotion: continuous movement, so the previous transform is the current one (a motion vector);
     * otherwise a jump (show, hide, reuse), so previous = new and no velocity is drawn. */
    void MoveInstances(int32 GroupIndex, int32 Start, TConstArrayView<FTransform> Transforms, bool bMotion);
    /** Reused buffers for MoveInstances, so a frame of sway allocates nothing once warm. */
    TArray<FTransform> MoveNew, MovePrev;
    void FlushGroups();
    /**
     * A static themed prop at a scenery-local transform. Instanced into the (mesh, material) group when the board is instanced, a
     * loose MakePart otherwise. Theme meshes tint through custom data (one float), plain shapes through the effect material (rgb).
     */
    FExpeditionProp PlaceProp(UStaticMesh* Mesh, FVector Location, FVector Scale, FLinearColor Color, FRotator Rotation = FRotator::ZeroRotator, bool bCastShadow = true, bool bOwnMaterial = false);
    /** The occluder: one instance with its own material instance (the sight lines go to that material). */
    FExpeditionPart PlaceOccluder(UStaticMesh* Mesh, FVector Location, FVector Scale);
    /** A scenery-local transform in the view actor's space (the same for an instance and a loose part). */
    FVector PropActorPosition(const FExpeditionProp& Prop) const;
    void AddSway(const FExpeditionProp& Prop, float Degrees, float Flicker);
    // Pooled effect pieces: a slot is a loose part or an instance; SetSlot moves and tints it and hides it with a zero scale.
    FExpeditionSlot MakeSlot(UStaticMesh* Mesh, FVector Location, FVector Scale, FLinearColor Color, FRotator Rotation, bool bCastShadow);
    void SetSlot(FExpeditionSlot& Slot, bool bVisible, const FVector& Location, const FRotator& Rotation, const FVector& Scale, const FLinearColor& Color);
    // Ring bank.
    bool CanBankRings() const;
    void EnsureBank();
    /** Writes the grid's bars (transforms and rgba custom data, in style order) into the bank, growing it when needed and hiding what is left over. */
    void SetOverlayBars(const TArray<FTransform>& Bars, const TArray<float>& Data);
    int32 AddRingSlot(const FLinearColor& Color);
    void SetRing(int32 Slot, bool bVisible, const FVector& Centre, float Scale, const FLinearColor& Color);
    void BuildScenery(const FCombatEncounter& Encounter, int32 ObstacleKind, int32 Seed);
    void BuildObstacle(FIntPoint Cell, int32 ObstacleKind, FRandomStream& Random);
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
    bool BuildThemedScenery(const FCombatEncounter& Encounter, int32 ObstacleKind, int32 Seed);
    /** The r002 obstacles of a world kit: three variants per recipe by seed and cell, arches over adjacent pairs when bArches (pillar and wall recipes of a world kit; see VigilKit::ArchesFor). Metric-authored, so placed unscaled. False when the theme has no variant to place. */
    bool BuildKitObstacles(const FCombatEncounter& Encounter, int32 ObstacleKind, int32 Seed, bool bArches, const FDiceRollWorldKit& Kit);
    /** Scatter, mist and light shafts outside the board (see VigilKit::PlanDressing; the Root kit uses the same plan). */
    void BuildKitDressing(const FCombatEncounter& Encounter, int32 Seed, const FDiceRollWorldKit& Kit, const TArray<FVector2D>& Braziers);
    /** A part from a mesh that keeps its own material (the occluder and the translucent cards), instanced for per-part parameters. */
    FExpeditionPart MakeOwnMaterialPart(UStaticMesh* Mesh, FVector Location, FVector Scale, FRotator Rotation);
    /** Pushes the sight lines to the occluder material. */
    void UpdateOccluder();
    void BuildOverview();
    void AddSway(USceneComponent* Component, float Degrees, float Flicker);
    void UpdateSway();
    void BuildEffects(const FCombatEncounter& Encounter);
    void ApplyTheme();
    void UpdateRigs();
    /** Adds M_FigureSilhouette to the grade's blendables when the asset exists; does nothing otherwise. */
    void AttachSilhouette();
    /** Turns a figure's custom depth and side stencil on or off (the footprint ring is excluded). */
    void SetFigureSilhouette(FExpeditionUnitRig& Rig, bool bOn);
    void UpdateTiles();
    void BuildOverlay();
    void UpdateOverlay();
    /** A hollow ring of thin bars, 100 cm across at unit scale; the component scale sets the diameter. */
    FExpeditionPart MakeRing(USceneComponent* Parent, FVector Location, FLinearColor Color);
    UInstancedStaticMeshComponent* MakeBarSet(FExpeditionPart& Part, USceneComponent* Parent, FLinearColor Color, float Opacity);
    void UpdateMarkers();
    void UpdateEffects();
    void UpdateCamera(float DeltaSeconds);
    /**
     * The unpanned focus (view space). While the whole board is in view from the rendered pose: the board centre leaned toward
     * the acting unit, as far as the board stays inside the safe rect. Otherwise: the acting unit itself.
     */
    FVector BaseFocus() const;
    /** The unit the camera follows: the presented focus during playback, else the active combatant; INDEX_NONE outside combat. */
    int32 CameraFocusId() const;
    /**
     * Clamps Base + Offset to the pan bound for the pose rendered this frame (CameraRig.h ClampPan) and returns the focus.
     * Offset is the player's pan: it is cleared while the whole board is in view and on axes locked to the board centre,
     * and otherwise re-pinned to the bound so a held key does not wind up past it.
     */
    FVector2D LimitPan(const FVector2D& Base, FVector2D& Offset) const;
    /** Moves a focus toward the followed unit only as far as brings the unit's tile and head inside the safe rect; a no-op while the whole board is in view. */
    FVector2D HoldUnit(const FVector2D& Wanted) const;
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
