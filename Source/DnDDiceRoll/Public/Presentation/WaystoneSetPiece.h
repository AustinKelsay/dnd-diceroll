#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WaystoneSetPiece.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;

/**
 * The finale's set piece (#250, rescoping #104): when the fifth fight is won the Waystone flares, the camera frames the apse, the rune seal
 * that guards the artifact shatters onto the dais along pre-authored arcs (no physics), and the artifact is lifted, before ARTIFACT RECOVERED.
 *
 * Presentation only (ADR-0001): it is keyed to the run's committed Victory, reads nothing back into rules, RNG or the save, and converges to
 * one settled state under Instant, Finish (skip) and Resume. Every pose is a pure function of the set-piece time below, so a capture can
 * hold any moment and a resumed victory shows the same settled frame as a played one.
 */

/** The moments the set piece announces (#243/#245 attach sound and music here; nothing listens today, so the hook is a no-op). */
enum class EWaystoneCue : uint8 { Flare, Shatter, Lift, Settled };

/** Where the set piece is: not on this board, the seal intact (the finale is being fought), playing its timeline, or settled (the artifact recovered). */
UENUM()
enum class EWaystoneSetPieceState : uint8 { Unbound, Intact, Playing, Settled };

namespace WaystoneSetPiece
{
/** The timeline in seconds at playback speed 1: the flare builds, the seal shatters, the artifact is lifted, the shot holds. */
constexpr float ShatterSeconds = 1.3f;
constexpr float LiftStartSeconds = 2.3f;
constexpr float LiftEndSeconds = 3.9f;
constexpr float DurationSeconds = 4.6f;
/** The camera eases from the board's pose into the first shot over this long, then (with the lift) into the held shot. */
constexpr float CameraInSeconds = 1.1f;
/** The extra turns the artifact makes while it charges (degrees, a whole number of turns so it hands back to its slow spin unchanged). */
constexpr float ExtraSpinDegrees = 720.0f;
constexpr float SpinSeconds = 3.2f;
/** The seal: shards in a ring around the artifact, each in two halves, and the chips thrown when it breaks. */
constexpr int32 Shards = 6;
constexpr int32 Halves = 2 * Shards;
constexpr int32 Chips = 12;
constexpr int32 Pieces = Halves + Chips;
/** Ring radius around the artifact and the outward lean of each shard (true cm, degrees). */
constexpr float RingRadiusCm = 75.0f;
constexpr float ShardLeanDegrees = 12.0f;

/** The Waystone light's multiplier (1 at rest and when settled; it builds to the shatter, flashes, and falls back). */
DNDDICEROLL_API float FlareGain(float Seconds);
/** The seal runes' glow multiplier (builds with the flare; the broken runes dim once spent). */
DNDDICEROLL_API float SealGlow(float Seconds);
/** 0 before the lift, 1 once the artifact is lifted (eased). */
DNDDICEROLL_API float LiftAlpha(float Seconds);
/** Degrees the artifact has turned beyond its slow spin: 0 at the start, ExtraSpinDegrees from SpinSeconds on. */
DNDDICEROLL_API float ExtraSpin(float Seconds);
/** How much the set-piece shot replaces the board camera (0 to 1), and how far the first shot has given way to the held one. */
DNDDICEROLL_API float CameraWeight(float Seconds);
DNDDICEROLL_API float ShotBlend(float Seconds);
/** The cues whose moment lies in (From, To]; Flare is at 0 and is announced by Play itself. */
DNDDICEROLL_API void CuesBetween(float From, float To, TArray<EWaystoneCue>& Out);
DNDDICEROLL_API const TCHAR* CueName(EWaystoneCue Cue);

/** One piece's pre-authored flight: from where it hangs in the seal to where it lies on the dais. Positions are in the set piece's space. */
struct DNDDICEROLL_API FFlight
{
    FTransform Start;
    FTransform Rest;
    /** Seconds after the shatter the piece leaves, and how long it flies. */
    float Delay = 0.0f;
    float Duration = 0.7f;
    /** Apex above the straight line (cm), the final hop's height and slide (cm), and the tumble: whole turns about an axis. */
    float Apex = 50.0f;
    float Hop = 5.0f;
    float Slide = 8.0f;
    FVector TumbleAxis = FVector::UpVector;
    int32 Turns = 1;
};
/** The piece's transform at a set-piece time: Start until the shatter, the arc, the hop, then exactly Rest. */
DNDDICEROLL_API FTransform FlightAt(const FFlight& Flight, float Seconds);
/** Seconds after which the piece lies still at Rest. */
DNDDICEROLL_API float LandedAt(const FFlight& Flight);

/** Where the set piece stands and what the floor around it is (all in its own space: true cm, origin at the artifact's pedestal on the floor). */
struct DNDDICEROLL_API FSite
{
    /** The apse's dais (the Vault kit) or the legacy pedestal in the corner (the primitive look and a theme without the Vault kit). */
    bool bApse = true;
    /** The artifact's resting height above the origin (the seal ring is centred there). */
    float ArtifactHeight = 190.0f;
    /** The legacy pedestal's top and radius (zero on the apse, which has none). */
    float PedestalTop = 0.0f;
    float PedestalRadius = 0.0f;
    /** The playing surface: its rectangle (min, max) and height, so a piece that lands over the board lies on it. */
    FVector2D BoardMin = FVector2D::ZeroVector;
    FVector2D BoardMax = FVector2D::ZeroVector;
    float BoardTop = 0.0f;
};
/** The floor height a piece lands on at a point: the apse's three dais steps (authored in build_vault_r002.py), the pedestal top, the board, or 0. */
DNDDICEROLL_API float FloorAt(const FSite& Site, FVector2D Point);
/** The seal and its pre-authored flights: halves first (shard k's upper half at 2k, lower at 2k + 1), then the chips. Deterministic. */
DNDDICEROLL_API TArray<FFlight> Plan(const FSite& Site);
/** The two shots (eye and focus, in the set piece's space) and the held shot's yaw offset that puts the Waystone beside the ending panel. */
struct DNDDICEROLL_API FShots
{
    FVector EyeA = FVector::ZeroVector, FocusA = FVector::ZeroVector;
    FVector EyeB = FVector::ZeroVector, FocusB = FVector::ZeroVector;
    float HeldYawDegrees = 0.0f;
};
DNDDICEROLL_API FShots ShotsFor(const FSite& Site);
/** The set-piece camera at a time, in the set piece's space (the shake at the shatter included). */
DNDDICEROLL_API void ShotAt(const FShots& Shots, float Seconds, FVector& Eye, FQuat& Rotation);
/** The three meshes of the themed seal, by package path (theme_plan.SETPIECE_R001): upper half, lower half, chip. */
DNDDICEROLL_API TArray<FSoftObjectPath> ThemedMeshPaths();
}

/** What the battle view gives the set piece to build with. */
struct DNDDICEROLL_API FWaystoneSetPieceSetup
{
    WaystoneSetPiece::FSite Site;
    /** The themed meshes (upper half, lower half, chip), or none for the primitive look, which draws the engine cube. */
    TArray<UStaticMesh*> Meshes;
    UStaticMesh* Cube = nullptr;
    UMaterialInterface* PlainMaterial = nullptr;
};

/**
 * The set piece on one finale board: the seal's pieces as three instanced groups under this component (its space is true cm with the origin at
 * the artifact's pedestal), a timeline, and the shot the view's camera blends into. The battle view owns it, builds it with each finale board and
 * drives it from its own Tick, so its order against playback is fixed.
 */
UCLASS()
class DNDDICEROLL_API UWaystoneSetPiece : public USceneComponent
{
    GENERATED_BODY()
public:
    /** Builds the seal intact. False (and Unbound) when a themed mesh is missing: the board then ends as it did before the set piece. */
    bool Build(const FWaystoneSetPieceSetup& Setup);
    /** Destroys the pieces; the set piece is Unbound until the next Build. */
    void Clear();
    /** Starts the timeline from the intact seal (announces Flare). Nothing when not Intact. */
    void Play();
    /** Converges on the settled state at once and silently (Instant, skip, Finish, Resume). Nothing when Unbound. */
    void Settle();
    /** Back to the intact seal (a Defeat or a rebuilt finale that is still being fought). */
    void Restore();
    /** Advances a playing timeline by Seconds (already scaled by the playback speed), announcing each cue it passes, and settles at the end. */
    void Advance(float Seconds);
    /** Capture support: holds the timeline at a time (a time at or past the end is the settled state) with no cue; Release lets it go settled. */
    void Hold(float Seconds);
    bool IsHeld() const { return bHeld; }

    EWaystoneSetPieceState State() const { return Current; }
    bool IsBound() const { return Current != EWaystoneSetPieceState::Unbound; }
    bool IsPlaying() const { return Current == EWaystoneSetPieceState::Playing; }
    /** Seconds into the timeline (0 when intact, DurationSeconds when settled). */
    float Time() const { return Seconds; }
    bool IsApse() const { return Site.bApse; }
    const WaystoneSetPiece::FSite& BuiltSite() const { return Site; }

    /** What the view applies to its own Waystone pieces this frame: the light's and the artifact's multipliers, the lift and the extra spin. */
    float FlareGain() const;
    float LiftAlpha() const;
    float ExtraSpin() const;
    /** The camera: how much the shot replaces the board pose, and the shot itself in world space. */
    float CameraWeight() const;
    void Shot(FVector& Eye, FQuat& Rotation) const;
    /** Blends a board camera pose toward the shot by CameraWeight (a no-op at weight 0). */
    void FrameCamera(FVector& Eye, FRotator& Rotation) const;

    /** Each piece's transform in this component's space as last drawn (halves, then chips); a hidden piece has zero scale. */
    const TArray<FTransform>& PieceTransforms() const { return Drawn; }
    /** How many times the pieces' instance transforms were written since the build (a settled or intact seal writes none per frame). */
    int32 PieceWrites() const { return Writes; }
    /** The primitive components the set piece draws with (one instanced group per mesh). */
    int32 ComponentCount() const;

    /** The sound and music hook (#243, #245): called once per cue while the timeline plays; Settle, Hold and Resume are silent. Unset is a no-op. */
    TFunction<void(EWaystoneCue)> OnCue;

private:
    EWaystoneSetPieceState Current = EWaystoneSetPieceState::Unbound;
    float Seconds = 0.0f;
    bool bHeld = false;
    WaystoneSetPiece::FSite Site;
    WaystoneSetPiece::FShots Shots;
    TArray<WaystoneSetPiece::FFlight> Flights;
    TArray<FTransform> Drawn;
    int32 Writes = 0;
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Groups;
    /** The themed seal's own material instances, whose Glow follows the flare (none on the primitive look). */
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Looks;
    TArray<float> BaseGlow;
    float ShownGlow = -1.0f;
    void Pose(bool bMotion);
    void Announce(EWaystoneCue Cue) const;
};
