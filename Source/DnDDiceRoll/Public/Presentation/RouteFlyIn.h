#pragma once

#include "CoreMinimal.h"

/**
 * The route fly-in (#249, spec #239): once a route is committed on the war-table journey map, the camera flies from the journey's view down into the chosen
 * world's raised diorama, the screen dips to dark, and the full world comes up at the fight's opening framing (a matched cut: the move ends looking into the
 * diorama from the same side and at about the pitch the battle camera opens with).
 *
 * Presentation only (ADR-0001): it starts after the route command has committed (the run, both random streams and the checkpoint are already written), reads
 * nothing back, and converges at once under Instant, Finish (skip) and Resume. Every pose and the fade are pure functions of the fly-in time below, so a capture
 * can hold any moment and a test can check the path without a world.
 */
namespace RouteFlyIn
{
/** The timeline in seconds at playback speed 1: the move ends in the dark at CutSeconds, where the board is built, then the world fades up. */
constexpr float CutSeconds = 1.6f;
/** The dip to dark over the move's last part, and the world fading up after the cut. */
constexpr float FadeOutSeconds = 0.4f;
constexpr float FadeInSeconds = 0.45f;
constexpr float DurationSeconds = CutSeconds + FadeInSeconds;
/** The longest frame the fly-in advances by: the cut frame builds the board (the load hitch), and the fade-up after it starts from dark instead of jumping. */
constexpr float MaxStepSeconds = 1.0f / 30.0f;
/**
 * How the move ends: looking down at this pitch (inside the battle camera's 35 to 60 degree ramp), at a point this far up the diorama (its little world stands on a
 * wooden base), close enough that the diorama overfills the view by EndFill, but never lower than EndClearanceCm over the diorama's top, so the straight path from the
 * journey camera stays outside the diorama and never flies through its base or its towers.
 */
constexpr double EndPitchDegrees = 50.0;
constexpr double AimHeight = 0.35;
constexpr double EndFill = 0.6;
constexpr double EndClearanceCm = 4.0;
/** The eye eases in and out, but is still moving at the cut (it would have stopped this much later), so the move plunges into the dark rather than parking. */
constexpr double EyeOverrunSeconds = 0.35;
/** The aim turns from the table toward the chosen diorama over the move's first part. */
constexpr double LookShare = 0.6;

inline double Smooth(double X)
{
    X = FMath::Clamp(X, 0.0, 1.0);
    return X * X * (3.0 - 2.0 * X);
}

/** How far the eye has travelled from the journey camera to the end of the move (0 to 1; 1 from the cut on). */
inline double EyeAlpha(float Seconds)
{
    if (Seconds >= CutSeconds) { return 1.0; }
    const double Span = CutSeconds + EyeOverrunSeconds;
    return Smooth(Seconds / Span) / Smooth(CutSeconds / Span);
}
/** How far the aim has turned from the journey's aim point to the diorama (0 to 1). */
inline double LookAlpha(float Seconds) { return Smooth(Seconds / (LookShare * CutSeconds)); }

/** The black over the screen: none for most of the move, dark at the cut, gone again at the end. */
inline float Fade(float Seconds)
{
    if (Seconds < CutSeconds - FadeOutSeconds || Seconds >= DurationSeconds) { return 0.0f; }
    if (Seconds < CutSeconds) { return static_cast<float>(Smooth((Seconds - (CutSeconds - FadeOutSeconds)) / FadeOutSeconds)); }
    return static_cast<float>(1.0 - Smooth((Seconds - CutSeconds) / FadeInSeconds));
}

/** The move in world space: where the eye starts and ends, and what it looks at. */
struct FPath
{
    FVector StartEye = FVector::ZeroVector, StartAim = FVector::ZeroVector;
    FVector EndEye = FVector::ZeroVector, EndAim = FVector::ZeroVector;
};

/**
 * The path from the journey camera (its world pose) into a diorama (its world bounds), for a horizontal field of view in degrees. It starts exactly at the camera,
 * aimed where the camera's centre ray meets the diorama's base, and ends on the side the camera looks from (the battle camera's home side), EndPitchDegrees down.
 */
inline FPath PathFor(const FTransform& Camera, const FBox& Diorama, double FieldOfViewDegrees)
{
    FPath Path;
    Path.StartEye = Camera.GetLocation();
    const FVector Forward = Camera.GetRotation().GetForwardVector();
    const double Drop = Path.StartEye.Z - Diorama.Min.Z;
    Path.StartAim = Forward.Z < -0.05 && Drop > 1.0 ? Path.StartEye + Forward * (Drop / -Forward.Z) : Path.StartEye + Forward * 300.0;
    const FVector Centre = Diorama.GetCenter(), Extent = Diorama.GetExtent();
    Path.EndAim = FVector(Centre.X, Centre.Y, Diorama.Min.Z + 2.0 * Extent.Z * AimHeight);
    FVector Flat(Forward.X, Forward.Y, 0.0);
    Flat = Flat.IsNearlyZero() ? FVector(0.0, 1.0, 0.0) : Flat.GetSafeNormal();
    const double Pitch = FMath::DegreesToRadians(EndPitchDegrees);
    const FVector Direction = Flat * FMath::Cos(Pitch) - FVector::UpVector * FMath::Sin(Pitch);
    const double Half = FMath::Max(Extent.X, Extent.Y);
    const double Fill = EndFill * Half / FMath::Tan(FMath::DegreesToRadians(FieldOfViewDegrees * 0.5));
    const double Above = (Diorama.Max.Z + EndClearanceCm - Path.EndAim.Z) / FMath::Sin(Pitch);
    const double Reach = FMath::Max(Fill, Above);
    Path.EndEye = Path.EndAim - Direction * Reach;
    return Path;
}

/** The camera at a fly-in time (clamped to the move: the pose at the cut holds after it). */
inline FTransform At(const FPath& Path, float Seconds)
{
    const FVector Eye = FMath::Lerp(Path.StartEye, Path.EndEye, EyeAlpha(Seconds));
    const FVector Aim = FMath::Lerp(Path.StartAim, Path.EndAim, LookAlpha(Seconds));
    return FTransform((Aim - Eye).Rotation(), Eye);
}
}
