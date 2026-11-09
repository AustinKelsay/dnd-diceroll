#pragma once

#include "CoreMinimal.h"

/**
 * Dither fade of the Drowned Vigil near-architecture occluder (#145). The occluder is one mesh, so the fade is decided per
 * pixel by its material, from two tests whose lower opacity wins:
 *  - Board: the ray from the camera through the pixel meets the board-top plane; when that point lies inside the board
 *    rectangle (grown by BoardMarginCells) and the pixel is closer to the camera than it, the pixel is covering the board
 *    and is dithered toward MinOpacity, easing back to opaque over BoardRampCells outside the rectangle. This keeps every
 *    playable cell and figure on the ground readable at any yaw and zoom.
 *  - Sight line: a point inside a cylinder around the line from the camera to the camera focus or the active unit is
 *    dithered too. It stays because a figure at the board's far edge, seen over its head or shoulders, is covered by pixels
 *    whose ray reaches the plane beyond the board rectangle, which the board test cannot see.
 * Everything else stays opaque. This is the same formula the material evaluates (M_DiceRollOccluder, built by import_dark_fantasy_theme.py),
 * kept here so it can be tested without a GPU. Presentation only.
 */
namespace OccluderFade
{
/** Never fully transparent: the occluder dithers, it does not vanish. */
constexpr float MinOpacity = 0.15f;
/** The fade cylinder's radius in cells; the inner half is at full fade and the outer half ramps back to opaque. */
constexpr float RadiusCells = 2.0f;
/** The board rectangle is grown by this many cells before the board test, so a figure on an edge cell is protected too. */
constexpr float BoardMarginCells = 0.5f;
/** Past the grown rectangle the board fade eases back to opaque over this many cells. */
constexpr float BoardRampCells = 0.5f;

/**
 * Opacity of a point of the occluder for one sight line from Eye to Target. Points before the eye or beyond the target
 * are not between them and stay opaque.
 */
inline float Opacity(const FVector& Point, const FVector& Eye, const FVector& Target, float Radius, float Minimum = MinOpacity)
{
    const FVector Ray = Target - Eye;
    const float Length2 = FMath::Max(static_cast<float>(Ray.SizeSquared()), 1.0f);
    const float T = static_cast<float>(FVector::DotProduct(Point - Eye, Ray)) / Length2;
    if (T < 0.0f || T > 1.0f) { return 1.0f; }
    const float Distance = static_cast<float>(FVector::Dist(Point, Eye + Ray * T));
    const float Inner = FMath::Max(Radius * 0.5f, 1.0f);
    const float Ramp = FMath::Clamp(Distance / Inner - 1.0f, 0.0f, 1.0f);
    return FMath::Lerp(Minimum, 1.0f, Ramp);
}

/** The lower of the opacities for the focus and the active unit. */
inline float Lowest(const FVector& Point, const FVector& Eye, const FVector& Focus, const FVector& Unit, float Radius, float Minimum = MinOpacity)
{
    return FMath::Min(Opacity(Point, Eye, Focus, Radius, Minimum), Opacity(Point, Eye, Unit, Radius, Minimum));
}

/**
 * Opacity of a point of the occluder for the board test. Eye is the camera, the rectangle is the board in world XY (already
 * grown by the margin) at height PlaneZ, and RampCm is the distance past the rectangle over which the fade eases out.
 * A point at or above the eye's height, below the plane, or whose view ray lands outside the rectangle stays opaque.
 */
inline float Board(const FVector& Point, const FVector& Eye, const FVector2D& Min, const FVector2D& Max, float PlaneZ, float RampCm, float Minimum = MinOpacity)
{
    const FVector Ray = Point - Eye;
    if (Ray.Z >= 0.0) { return 1.0f; }
    const float T = static_cast<float>((PlaneZ - Eye.Z) / FMath::Min(Ray.Z, -0.001));
    if (T <= 1.0f) { return 1.0f; }  // the point is at or below the plane: nothing of it stands between the camera and the board
    const FVector Hit = Eye + Ray * T;
    const float Outside = static_cast<float>(FMath::Max(FMath::Max(Min.X - Hit.X, Hit.X - Max.X), FMath::Max(Min.Y - Hit.Y, Hit.Y - Max.Y)));
    const float Ramp = FMath::Clamp(Outside / FMath::Max(RampCm, 1.0f), 0.0f, 1.0f);
    return FMath::Lerp(Minimum, 1.0f, Ramp);
}
}
