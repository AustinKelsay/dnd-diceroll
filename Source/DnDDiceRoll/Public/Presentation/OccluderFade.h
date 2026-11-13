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
 * The walled worlds have near walls 3 m high that stand 2 to 3 cells outside the board, and the Vigil's rose window hangs on the home-side wall
 * 8 cells out. Seen from a camera outside, a near wall hides the floor from its base inward for a couple of cells, and the fade only reached half
 * a cell past the board, so the Vault's near wall covered a quarter of the view at some yaws and the Vigil's ledge and stained-glass rose
 * filled the bottom of the frame behind the action bar, without dithering (#209). Their board fade is wider.
 *  - The Vault: on each wall the camera is outside of (a side wall, or the back wall) the fade reaches the wall's face, at every zoom; toward the open
 *    front VaultBoardMarginCells by the camera's share; BoardMarginCells elsewhere, with a VaultBoardRampCells ramp. The camera-side wall's outer face is
 *    black and opaque, so wherever it stands in the frame (default or far pose) every row of it dithers (on-screen D1: with a fade that eased off toward
 *    the far pose it filled the bottom of the frame at 180 degrees from home); a wall seen from inside stays opaque.
 *  - The Vigil: BoardMarginCells on the sides away from the camera, and VigilNearMarginCells on the side(s) the camera is on, with a ramp of
 *    VigilBoardRampCells. A pixel of the rose looks over the near wall and its ray lands about 5 cells past the board's near edge at the default
 *    zoom (farther as the camera rises), so the wide side reaches 7.5 (a diagonal yaw lengthens the ray), easing to VigilFarNearMarginCells at the far pose; the geometry between the camera and the board on the camera's side dithers, whatever the yaw, and the
 *    rose stays opaque on the far side and from the sides.
 * The Root's forest keeps BoardMarginCells and BoardRampCells.
 */
/**
 * The Vault hall's walls from the board centre, in cells at the 150 cm cell (measured from the occluder mesh): the side walls' inner faces are 16 m out and
 * their outer faces 17.5 m; the back wall's inner face is 11.8 m out and its outer face, at the apse, 15.2 m (the front is open). A camera outside a wall
 * sees its black, opaque outer face, and a ray that meets the face at a low height lands on the floor near the face's own plane, so every row of the wall
 * dithers only if the fade reaches the outer face: the margin on a wall the camera is outside of (past its inner face) is the gap from the board's edge to the
 * outer face plus VaultWallEdgeCells, and never less than VaultBoardMarginCells.
 */
constexpr float VaultSideWallCells = 10.667f, VaultSideWallOuterCells = 11.667f;
constexpr float VaultBackWallCells = 7.867f, VaultBackWallOuterCells = 10.133f;
constexpr float VaultWallEdgeCells = 0.15f;
constexpr float VaultBoardMarginCells = 2.5f;
constexpr float VaultBoardRampCells = 1.5f;
constexpr float VigilNearMarginCells = 7.5f;
/** The Vigil's near-side margin at the far pose: the camera is higher, so the rose's rays land farther past the board (about 7 to 8 cells at the farthest zoom). It eases from VigilNearMarginCells with the zoom. */
constexpr float VigilFarNearMarginCells = 11.0f;
/**
 * The Vigil's occluders fade to this, not to MinOpacity: at the close zoom the stained-glass rose and the ledge fill the bottom third of the
 * frame, and at 15% their dither is still a coloured stipple behind the action bar (on-screen pass of #215). At 4% a faded piece is a few specks:
 * still dithered, not hidden. The Vault and the Root keep MinOpacity.
 */
constexpr float VigilMinOpacity = 0.04f;
constexpr float VigilBoardRampCells = 1.0f;

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
