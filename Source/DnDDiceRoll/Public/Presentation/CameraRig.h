#pragma once

#include "CoreMinimal.h"

/**
 * The battle camera's pure math (#143). Distances are reference units (cm for the 110 cm r001 cell);
 * the battle view multiplies them by FBoardMetric::Scale(). Presentation only.
 */
namespace ExpeditionCameraRig
{
    constexpr float FieldOfView = 40.0f;
    /** Zoom range in reference units; the capture poses use the same clamp. */
    constexpr float MinDistance = 900.0f;
    constexpr float MaxDistance = 3200.0f;
    /** Zoom stops that carry the pitch: at or below Near the camera is low, at Far it is high. */
    constexpr float NearDistance = 1300.0f;
    constexpr float FarDistance = 3000.0f;
    constexpr float NearPitch = 35.0f;
    constexpr float DefaultPitch = 45.0f;
    constexpr float FarPitch = 60.0f;
    /** The framed default is kept inside this range so the pitch ramps stay well defined. */
    constexpr float MinFramed = 1400.0f;
    constexpr float MaxFramed = 2900.0f;
    /** Length of the camera arm per unit of distance: the vector the r001 camera used (0.62 reach, 1.0 rise). */
    constexpr float ArmLength = 1.1768f;
    /** How far past the outer cell centres the pan focus may travel, in cells. */
    constexpr float PanMarginCells = 2.0f;

    /** The part of the screen the HUD leaves free, as fractions of the screen size trimmed from each side. */
    struct FSafeRect
    {
        float Left = 0.16f;   // party cards
        float Right = 0.02f;
        float Top = 0.06f;
        float Bottom = 0.12f; // action bar
        float Aspect = 16.0f / 9.0f;
        /** How much of the free area the board may fill. */
        float Fill = 0.95f;
        float Width() const { return FMath::Max(0.2f, 1.0f - Left - Right); }
        float Height() const { return FMath::Max(0.2f, 1.0f - Top - Bottom); }
        /** Offset of the free area's centre from the screen centre, in fractions of the screen (x right, y down). */
        FVector2D CentreOffset() const { return FVector2D((Left - Right) * 0.5f, (Top - Bottom) * 0.5f); }
    };

    /** Camera pitch in degrees for a zoom distance; Framed is the board's default distance. Non-decreasing in Distance. */
    inline float PitchFor(float Distance, float Framed)
    {
        const float F = FMath::Clamp(Framed, MinFramed, MaxFramed);
        if (Distance <= F)
        { return FMath::Lerp(NearPitch, DefaultPitch, FMath::Clamp((Distance - NearDistance) / (F - NearDistance), 0.0f, 1.0f)); }
        return FMath::Lerp(DefaultPitch, FarPitch, FMath::Clamp((Distance - F) / (FarDistance - F), 0.0f, 1.0f));
    }

    /**
     * The reference distance at which a Width x Height board fills Fill of the safe rect when seen from yaw -90
     * at the default pitch. Cell and figure sizes are world cm; the result is in reference units.
     */
    inline float FitDistance(int32 Width, int32 Height, float CellCm, float FigureCm, float Scale, const FSafeRect& Safe)
    {
        const float Tan = FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5f));
        const float VisibleWidth = 2.0f * Tan * Safe.Width() * Safe.Fill;           // per cm of view distance
        const float VisibleHeight = VisibleWidth / Safe.Aspect * (Safe.Height() / Safe.Width());
        const float BoardWidth = Width * CellCm;
        const float BoardDepth = Height * CellCm * FMath::Sin(FMath::DegreesToRadians(DefaultPitch)) + FigureCm * 0.8f;
        const float View = FMath::Max(BoardWidth / VisibleWidth, BoardDepth / VisibleHeight);
        return View / ArmLength / FMath::Max(0.01f, Scale);
    }

    /** Clamps a focus point (view-space cm) to the board's cell-centre box grown by the pan margin. */
    inline FVector2D ClampFocus(FVector2D Focus, int32 Width, int32 Height, float CellCm)
    {
        const float Margin = PanMarginCells * CellCm;
        return FVector2D(FMath::Clamp(Focus.X, -Margin, (Width - 1) * CellCm + Margin),
                         FMath::Clamp(Focus.Y, -Margin, (Height - 1) * CellCm + Margin));
    }
}
