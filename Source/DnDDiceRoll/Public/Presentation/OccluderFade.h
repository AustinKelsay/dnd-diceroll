#pragma once

#include "CoreMinimal.h"

/**
 * Dither fade of the Drowned Vigil near-architecture occluder (#145). The occluder is one mesh, so the fade is decided per
 * pixel by its material: a point of the mesh that lies inside a cylinder around the sight line from the camera to the
 * thing the player watches (the camera focus or the active unit) is dithered toward MinOpacity; everything else stays
 * opaque. This is the same formula the material evaluates (M_DiceRollOccluder, built by import_dark_fantasy_theme.py),
 * kept here so it can be tested without a GPU. Presentation only.
 */
namespace OccluderFade
{
/** Never fully transparent: the occluder dithers, it does not vanish. */
constexpr float MinOpacity = 0.15f;
/** The fade cylinder's radius in cells; the inner half is at full fade and the outer half ramps back to opaque. */
constexpr float RadiusCells = 2.0f;

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
}
