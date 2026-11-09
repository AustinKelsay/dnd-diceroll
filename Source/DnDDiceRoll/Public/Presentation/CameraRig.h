#pragma once

#include "CoreMinimal.h"
#include "Presentation/BoardMetric.h"
#include "Presentation/TargetChipLayout.h"

/**
 * The battle camera's pure math (#143). Distances are reference units (cm for the 110 cm r001 cell);
 * the battle view multiplies them by FBoardMetric::Scale(). Presentation only.
 */
namespace ExpeditionCameraRig
{
    constexpr float FieldOfView = 40.0f;
    /** The aspect the FOV is authored for. Unreal's default AspectRatioAxisConstraint (MaintainYFOV) keeps the vertical FOV this implies at any viewport. */
    constexpr float ReferenceAspect = 16.0f / 9.0f;
    /** Zoom range in reference units; the capture poses use the same clamp. Boards of 16 to 18 cells need well over 3000 to fit between the HUD panels. */
    constexpr float MinDistance = 900.0f;
    constexpr float MaxDistance = 6400.0f;
    /** Zoom stops that carry the pitch: at or below Near the camera is low, at the far stop it is high. */
    constexpr float NearDistance = 1300.0f;
    constexpr float FarDistance = 3000.0f;
    /** The far stop is at least this multiple of the framed distance, so a wide board still has a far end to zoom toward. */
    constexpr float FarStopRatio = 1.4f;
    constexpr float NearPitch = 35.0f;
    constexpr float DefaultPitch = 45.0f;
    constexpr float FarPitch = 60.0f;
    /** The framed default is kept inside this range so the pitch ramps stay well defined. */
    constexpr float MinFramed = 1400.0f;
    constexpr float MaxFramed = 4600.0f;
    /** The yaw a fresh board starts at; the pitch anchor is fitted from it. */
    constexpr float HomeYaw = -90.0f;
    /** Length of the camera arm per unit of distance: the vector the r001 camera used (0.62 reach, 1.0 rise). */
    constexpr float ArmLength = 1.1768f;
    /** How far past the outer cell centres the pan focus may travel, in cells. */
    constexpr float PanMarginCells = 2.0f;

    /** Half the tangent of the vertical / horizontal view angle at a viewport aspect (width / height). */
    inline float HalfTanY() { return FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5f)) / ReferenceAspect; }
    inline float HalfTanX(float Aspect) { return HalfTanY() * Aspect; }

    /**
     * The part of the screen the HUD leaves free, as fractions of the screen size trimmed from each side. The
     * defaults and ForViewport come from ExpeditionLayout::BoardBounds, the rectangle the HUD layout reserves.
     */
    struct FSafeRect
    {
        float Left = static_cast<float>(ExpeditionLayout::BoardLeft / ExpeditionLayout::DesignWidth);     // party cards
        float Right = static_cast<float>(ExpeditionLayout::BoardRight / ExpeditionLayout::DesignWidth);
        float Top = static_cast<float>(ExpeditionLayout::BoardTop / ExpeditionLayout::DesignHeight);      // initiative strip
        float Bottom = static_cast<float>(ExpeditionLayout::BoardBottom / ExpeditionLayout::DesignHeight); // action bar
        /** Viewport width over height. */
        float Aspect = ReferenceAspect;
        /** How much of the free area the fitted board fills; the rest is slack for the active-unit lean. */
        float Fill = 0.95f;
        float Width() const { return FMath::Max(0.2f, 1.0f - Left - Right); }
        float Height() const { return FMath::Max(0.2f, 1.0f - Top - Bottom); }
        /** Offset of the free area's centre from the screen centre, in fractions of the screen (x right, y down). */
        FVector2D CentreOffset() const { return FVector2D((Left - Right) * 0.5f, (Top - Bottom) * 0.5f); }
        /** Whether a screen position (0..1 from the top left) is inside the free area shrunk to InFill. */
        bool Holds(const FVector2D& Screen, float InFill) const
        {
            const FVector2D Centre = FVector2D(0.5, 0.5) + CentreOffset();
            return FMath::Abs(Screen.X - Centre.X) <= Width() * 0.5f * InFill && FMath::Abs(Screen.Y - Centre.Y) <= Height() * 0.5f * InFill;
        }
        bool operator==(const FSafeRect& Other) const
        { return Left == Other.Left && Right == Other.Right && Top == Other.Top && Bottom == Other.Bottom && Aspect == Other.Aspect && Fill == Other.Fill; }

        /** The free area for a viewport in pixels, using the HUD's own design-unit scale. */
        static FSafeRect ForViewport(int32 PixelsX, int32 PixelsY)
        {
            FSafeRect Result;
            if (PixelsX <= 0 || PixelsY <= 0) { return Result; }
            const double Scale = ExpeditionLayout::ViewScale(PixelsX, PixelsY);
            const double W = PixelsX / Scale, H = PixelsY / Scale;
            const ExpeditionLayout::Rect Bounds = ExpeditionLayout::BoardBounds(W, H);
            Result.Left = static_cast<float>(Bounds.Left / W);
            Result.Right = static_cast<float>((W - Bounds.Right) / W);
            Result.Top = static_cast<float>(Bounds.Top / H);
            Result.Bottom = static_cast<float>((H - Bounds.Bottom) / H);
            Result.Aspect = static_cast<float>(PixelsX) / static_cast<float>(PixelsY);
            return Result;
        }
    };

    /** What the view remembers of the home-yaw fit, so the per-frame pitch lookups do not re-solve it. */
    struct FFramedCache
    {
        int32 Width = -1;
        int32 Height = -1;
        float CellCm = 0.0f;
        FSafeRect Rect;
        float Anchor = 0.0f;
    };

    /** The distance at which the pitch ramp ends at FarPitch for a framed distance. */
    inline float FarStop(float Framed) { return FMath::Max(FarDistance, Framed * FarStopRatio); }

    /** Camera pitch in degrees for a zoom distance; Framed is the board's default distance. Non-decreasing in Distance. */
    inline float PitchFor(float Distance, float Framed)
    {
        const float F = FMath::Clamp(Framed, MinFramed, MaxFramed);
        if (Distance <= F)
        { return FMath::Lerp(NearPitch, DefaultPitch, FMath::Clamp((Distance - NearDistance) / (F - NearDistance), 0.0f, 1.0f)); }
        return FMath::Lerp(DefaultPitch, FarPitch, FMath::Clamp((Distance - F) / (FarStop(F) - F), 0.0f, 1.0f));
    }

    /** The camera's offset from its focus, and the slide that puts the focus in the middle of the free screen area. World cm. */
    struct FArm
    {
        FVector Offset = FVector::ZeroVector;
        FVector Framing = FVector::ZeroVector;
    };

    inline FArm Arm(float Yaw, float Distance, float Pitch, float Scale, const FSafeRect& Safe)
    {
        const float Span = Distance * Scale;
        const float Radians = FMath::DegreesToRadians(Yaw);
        const float PitchRadians = FMath::DegreesToRadians(Pitch);
        const float Reach = ArmLength * FMath::Cos(PitchRadians);
        const float Rise = ArmLength * FMath::Sin(PitchRadians);
        FArm Result;
        Result.Offset = FVector(FMath::Cos(Radians) * Span * Reach, FMath::Sin(Radians) * Span * Reach, Span * Rise);
        const FVector Forward = -Result.Offset.GetSafeNormal();
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
        const FVector Up = FVector::CrossProduct(Forward, Right);
        const double Depth = Result.Offset.Size();
        const double VisibleWidth = 2.0 * HalfTanX(Safe.Aspect) * Depth;
        const double VisibleHeight = 2.0 * HalfTanY() * Depth;
        const FVector2D Centre = Safe.CentreOffset();
        Result.Framing = -Right * (Centre.X * VisibleWidth) + Up * (Centre.Y * VisibleHeight);
        return Result;
    }

    /** Screen position (0..1 from the top left) of a point seen from a camera at Eye looking along Forward; false when it is behind. */
    inline bool Project(const FVector& Point, const FVector& Eye, const FVector& Forward, float Aspect, FVector2D& Screen)
    {
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
        const FVector Up = FVector::CrossProduct(Forward, Right);
        const FVector Delta = Point - Eye;
        const double Depth = FVector::DotProduct(Delta, Forward);
        if (Depth < 1.0) { return false; }
        Screen.X = 0.5 + 0.5 * FVector::DotProduct(Delta, Right) / (Depth * HalfTanX(Aspect));
        Screen.Y = 0.5 - 0.5 * FVector::DotProduct(Delta, Up) / (Depth * HalfTanY());
        return true;
    }

    /** The four board corners (outer tile edges, view space, at the board top) as seen from a focus on the ground. */
    inline bool BoardInside(int32 Width, int32 Height, const FBoardMetric& Metric, const FVector2D& FocusXY,
        float Yaw, float Distance, float Pitch, const FSafeRect& Safe, float InFill)
    {
        const FArm Pose = Arm(Yaw, Distance, Pitch, Metric.Scale(), Safe);
        const FVector Eye = FVector(FocusXY.X, FocusXY.Y, 0.0) + Pose.Offset + Pose.Framing;
        const FVector Forward = -Pose.Offset.GetSafeNormal();
        const double C = Metric.CellCm, Z = Metric.BoardTop();
        const FVector Corners[4] = {
            FVector(-0.5 * C, -0.5 * C, Z), FVector((Width - 0.5) * C, -0.5 * C, Z),
            FVector(-0.5 * C, (Height - 0.5) * C, Z), FVector((Width - 0.5) * C, (Height - 0.5) * C, Z)};
        for (const FVector& Corner : Corners)
        {
            FVector2D Screen;
            if (!Project(Corner, Eye, Forward, Safe.Aspect, Screen) || !Safe.Holds(Screen, InFill)) { return false; }
        }
        return true;
    }

    /** The board's centre, the default focus (view space). */
    inline FVector2D BoardCentre(int32 Width, int32 Height, const FBoardMetric& Metric)
    { return FVector2D((Width - 1) * 0.5 * Metric.CellCm, (Height - 1) * 0.5 * Metric.CellCm); }

    struct FFit
    {
        /** Reference units; MaxDistance when the board does not fit. */
        float Distance = MaxDistance;
        /** The pitch anchor (the framed distance at the home yaw). */
        float Anchor = MinFramed;
        float Pitch = DefaultPitch;
        bool bFits = false;
    };

    /**
     * The nearest distance at which the board's four corners project inside the safe rect (shrunk to Safe.Fill) from
     * Yaw, with the focus on the board centre. The pitch follows the zoom: PitchFor(Distance, Anchor), or the default
     * pitch when Anchor is zero. Scans outward then bisects, so a board that cannot fit reports bFits = false.
     */
    inline FFit FitDistance(int32 Width, int32 Height, const FBoardMetric& Metric, float Yaw, float Anchor, const FSafeRect& Safe)
    {
        const FVector2D Centre = BoardCentre(Width, Height, Metric);
        auto PitchAt = [Anchor](float D) { return Anchor > 0.0f ? PitchFor(D, Anchor) : DefaultPitch; };
        auto Inside = [&](float D) { return BoardInside(Width, Height, Metric, Centre, Yaw, D, PitchAt(D), Safe, Safe.Fill); };
        constexpr float Step = 25.0f;
        FFit Result;
        Result.Anchor = Anchor;
        float Low = MinFramed;
        float High = MinFramed;
        bool bFound = Inside(High);
        while (!bFound && High < MaxDistance)
        {
            Low = High;
            High = FMath::Min(High + Step, MaxDistance);
            bFound = Inside(High);
        }
        if (!bFound) { Result.Distance = MaxDistance; Result.Pitch = PitchAt(MaxDistance); return Result; }
        for (int32 Iteration = 0; Iteration < 24 && High - Low > 0.01f; ++Iteration)
        {
            const float Mid = 0.5f * (Low + High);
            (Inside(Mid) ? High : Low) = Mid;
        }
        Result.Distance = High;
        Result.Pitch = PitchAt(High);
        Result.bFits = true;
        return Result;
    }

    /**
     * The default zoom for a board seen from Yaw. The pitch anchor is the home-yaw fit (45 degrees at that distance), so
     * orbiting at a fixed zoom never tilts the camera; other yaws fit at the pitch their distance implies.
     */
    inline FFit FitBoard(int32 Width, int32 Height, const FBoardMetric& Metric, float Yaw, const FSafeRect& Safe)
    {
        const FFit Home = FitDistance(Width, Height, Metric, HomeYaw, 0.0f, Safe);
        const float Anchor = FMath::Clamp(Home.Distance, MinFramed, MaxFramed);
        FFit Result = FitDistance(Width, Height, Metric, Yaw, Anchor, Safe);
        Result.bFits = Result.bFits && Home.bFits && Home.Distance <= MaxFramed;
        return Result;
    }

    /**
     * Moves a focus from the board centre toward Target only as far as keeps the board corners inside the unshrunk safe
     * rect. When the board does not fit at this zoom (zoomed in) the lean is free.
     */
    inline FVector2D LimitLean(const FVector2D& Centre, const FVector2D& Target, int32 Width, int32 Height, const FBoardMetric& Metric,
        float Yaw, float Distance, float Pitch, const FSafeRect& Safe)
    {
        if (!BoardInside(Width, Height, Metric, Centre, Yaw, Distance, Pitch, Safe, 1.0f)) { return Target; }
        if (BoardInside(Width, Height, Metric, Target, Yaw, Distance, Pitch, Safe, 1.0f)) { return Target; }
        float Low = 0.0f, High = 1.0f;
        for (int32 Iteration = 0; Iteration < 16; ++Iteration)
        {
            const float Mid = 0.5f * (Low + High);
            (BoardInside(Width, Height, Metric, Centre + (Target - Centre) * Mid, Yaw, Distance, Pitch, Safe, 1.0f) ? Low : High) = Mid;
        }
        return Centre + (Target - Centre) * Low;
    }

    /** Clamps a focus point (view-space cm) to the board's cell-centre box grown by the pan margin. */
    inline FVector2D ClampFocus(FVector2D Focus, int32 Width, int32 Height, float CellCm)
    {
        const float Margin = PanMarginCells * CellCm;
        return FVector2D(FMath::Clamp(Focus.X, -Margin, (Width - 1) * CellCm + Margin),
                         FMath::Clamp(Focus.Y, -Margin, (Height - 1) * CellCm + Margin));
    }
}
