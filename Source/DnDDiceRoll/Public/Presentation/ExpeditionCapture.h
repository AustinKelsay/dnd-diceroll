#pragma once

#include "CoreMinimal.h"
#include "Presentation/ExpeditionPlayback.h"

class AExpeditionBattleView;
class AExpeditionPlayerController;

/** What a capture shot shows. Battle, Targeting, Tooltip, Hint, Impact, Roll and Pose need an encounter; Overview needs the tabletop; Start leaves the run. */
enum class ECaptureMode : uint8 { Battle, Targeting, Overview, Start, Tooltip, Hint, Roll, Impact, Pose };

/** Which contextual hint a Hint shot raises (#158): move, resources, rescue or endturn. */
enum class ECaptureHint : uint8 { Move, Resources, Rescue, EndTurn };

/** Which dock action a Tooltip shot chooses, as its key does: A, F, M, R, P. */
enum class ECaptureAction : uint8 { Attack, Special1, Special2, Rescue, Potion };

constexpr int32 DefaultImpactFrames = 4;
constexpr int32 MaxImpactFrames = 60;

/** One fixed pose from the poses file. A Distance of zero means the board's framed default; FarDistance (negative) means the far zoom stop for the yaw. */
struct DNDDICEROLL_API FCaptureShot
{
    FString Name;
    ECaptureMode Mode = ECaptureMode::Battle;
    bool bHasCamera = false;
    float Yaw = 0.0f;
    float Distance = 0.0f;
    /** Optional pan (#149): the view pans this far right and away from the camera, in reference cm like Distance, then the pan bound applies. A large value reaches the pan stop. Camera shots only. */
    bool bHasPan = false;
    FVector2D Pan = FVector2D::ZeroVector;
    /** Tooltip shots only. */
    ECaptureAction Action = ECaptureAction::Attack;
    /** Hint shots only. */
    ECaptureHint Hint = ECaptureHint::Move;
    /** Impact shots only (#189): a plain hit, or a spell bolt (spell: Cleric radiant, fire: Wizard, missile: Magic Missile) landing with the flash. */
    EImpactPreview Effect = EImpactPreview::Hit;
    /** Impact shots only: when the preview is taken: the hit with its flash (default), a bolt half way along its path, or (Magic Missile) the first or second bolt landing. */
    EImpactMoment Moment = EImpactMoment::Landing;
    /** Impact shots only: rendered frames between the hit and the screenshot (the flash is held at its peak throughout). */
    int32 Frames = DefaultImpactFrames;
    /** Pose shots only (#150): a party hero of this class (the role's enum name, "Rogue") or a foe of this look (the FoeRigs key, "Brute"; ExpeditionFoe::RigKey) held in this clip at this time in seconds, with no command and no rules change. */
    FString Role;
    EUnitPose Pose = EUnitPose::Idle;
    float Time = 0.0f;
    /** Start shots only: a catalog build id to select for the first hero, so the build picker shows it. Empty leaves the party as it is. */
    FString Build;
    /** Overview shots only (#200): the 1-based party slot whose details panel the route screen shows, as when that hero's card is selected. Zero leaves the selection as it is. */
    int32 Hero = 0;
};

struct DNDDICEROLL_API FCapturePoses
{
    /** Rendered frames to wait after each pose change before the screenshot. */
    int32 SettleFrames = 60;
    TArray<FCaptureShot> Shots;
};

namespace ExpeditionCapture
{
/** The camera distance range a battle allows, in reference units (cm for the 110 cm r001 cell; the camera
 * scales them by the board metric). Poses outside it are rejected. */
constexpr float MinDistance = 900.0f;
constexpr float MaxDistance = 6400.0f;
/** Distance sentinel for "far": the zoom stop where the pitch ramp ends (ExpeditionCameraRig::FarStop of the framed distance). */
constexpr float FarDistance = -1.0f;
constexpr int32 MaxSettleFrames = 1200;

/** True when the command line asks for a capture run. A capture run binds no player input, so a
 * focused capture window can never submit a command or write the checkpoint. */
DNDDICEROLL_API bool Requested(const TCHAR* CommandLine);

DNDDICEROLL_API const TCHAR* ModeName(ECaptureMode Mode);
DNDDICEROLL_API const TCHAR* ActionName(ECaptureAction Action);
DNDDICEROLL_API const TCHAR* HintName(ECaptureHint Hint);
DNDDICEROLL_API const TCHAR* EffectName(EImpactPreview Effect);
DNDDICEROLL_API const TCHAR* PhaseName(EImpactMoment Moment);
/** Pure: validates a poses document (version 1) and fills Out, or fails with a message naming the problem. */
DNDDICEROLL_API bool ParsePoses(const FString& Json, FCapturePoses& Out, FString& Error);
}

/**
 * Presentation-only capture run, active only when the command line has -ExpeditionCapture=<poses.json>.
 * It resumes the checkpoint under -ExpeditionSaveRoot, then for each pose sets the mode and camera,
 * waits a fixed number of frames, saves a screenshot under -ExpeditionCaptureOut and exits. It never
 * submits a combat command and never writes the checkpoint.
 */
class DNDDICEROLL_API FExpeditionCaptureDirector
{
public:
    /** Null unless the command line asks for a capture run. */
    static TSharedPtr<FExpeditionCaptureDirector> TryCreate(AExpeditionPlayerController* Controller, AExpeditionBattleView* View);
    /** Same as TryCreate with the values already read from the command line (also the seam automation uses). */
    static TSharedPtr<FExpeditionCaptureDirector> Create(AExpeditionPlayerController* Controller, AExpeditionBattleView* View, const FString& PosesPath, const FString& OutputDir, const FString& Fixture);
    /** Once per rendered frame, from the player controller. */
    void Tick();
    /**
     * Editor builds only (#174): finishes every outstanding shader and asset compilation, then flushes the render thread. A start-up
     * mitigation for #174 (a shader map replaced while a frame still draws with it): later frames or a return to the start screen can
     * queue more compilation, so a residual window remains. Does nothing in other builds.
     */
    static void DrainCompilation();
    /** Replaces what the director calls to drain compilation (default: DrainCompilation). It runs on the first Tick and on both sides of Resume. */
    void SetCompilationDrain(TFunction<void()> InDrain) { Drain = MoveTemp(InDrain); }

private:
    enum class EStage : uint8 { Loading, Next, Settling, Saving, Done };
    struct FResult
    {
        FCaptureShot Shot;
        FString File, Status, Reason;
        float Yaw = 0.0f, Distance = 0.0f;
        FVector Location = FVector::ZeroVector;
        FRotator Rotation = FRotator::ZeroRotator;
        float Fov = 0.0f;
        /** Impact shots only: the struck combatant, its cell and its projected footprint box in screenshot pixels. */
        int32 TargetId = INDEX_NONE;
        FString TargetName;
        FIntPoint TargetCell = FIntPoint::ZeroValue;
        bool bHasFootprint = false;
        FIntRect Footprint;
        FVector2D ImpactFrom = FVector2D::ZeroVector;
        bool bHasBolt = false;
        FIntRect Bolt;
        /** Impact shots with a "role" (#198): the shooter, a party hero of that class, held in its Ranged clip, and its footprint box (the shot's measured subject). */
        int32 ShooterId = INDEX_NONE;
        FString ShooterName;
        /** The shooter's own cell (the manifest records it as shooter.cellX/cellY, so a shooter result is not reported at the target's cell) and its class. */
        FIntPoint ShooterCell = FIntPoint::ZeroValue;
        EAdventurerRole ShooterRole = EAdventurerRole::None;
        bool bHasShooterBox = false;
        FIntRect ShooterBox;
        bool bImpactFired = false;
        /** Battle and targeting shots (#150): every figure on the board with its projected footprint box in screenshot pixels, so a tool can
         * measure one class's figure against the ground behind it (tools/figure_contrast.py). Units whose box is off screen are left out. */
        struct FUnitBox
        {
            int32 Id = INDEX_NONE;
            FString Name, Role;
            bool bParty = false, bRigged = false;
            FIntRect Box;
        };
        TArray<FUnitBox> Units;
    };

    TWeakObjectPtr<AExpeditionPlayerController> Controller;
    TWeakObjectPtr<AExpeditionBattleView> View;
    FCapturePoses Poses;
    FString OutputDir, PosesPath, Fixture;
    EStage Stage = EStage::Loading;
    int32 NextShot = 0;
    int32 Frames = 0;
    int32 StageFrames = 0;
    bool bResumed = false;
    bool bDrainedAtStart = false;
    TFunction<void()> Drain;
    bool bFailed = false;
    FString Failure;
    FIntPoint Resolution = FIntPoint::ZeroValue;
    /** Read once after the resume; the start screen later leaves the run. */
    int32 Seed = 0;
    FIntPoint Layout = FIntPoint::ZeroValue;
    TArray<FResult> Results;
    FResult Pending;

    bool Idle() const;
    /** The struck unit's footprint box (the footprint ring's square, from the board top to the figure's head) projected with the camera the screenshot sees. */
    bool ProjectFootprint(int32 UnitId, FIntRect& Out) const;
    /** The screenshot box around a point on the board at a height (reference cm), for the bolt of an in-flight impact shot. */
    bool ProjectBox(FVector2D Cell, float HeightCm, float HalfCm, FIntRect& Out) const;
    bool Apply(const FCaptureShot& Shot, FString& Skip);
    void Fail(const FString& Message);
    void Finish();
    void WriteManifest() const;
};
