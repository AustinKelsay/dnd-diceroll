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
    /** How far past the board's outer tile edge the visible ground may reach when panning, in cells (was 2 cells past the outer cell centres, #148 gate 2). */
    constexpr float PanMarginCells = 1.5f;
    /** The default zoom makes a standing figure at the focus this many pixels tall at FigureTargetViewportHeight pixels of viewport height (#148 gate 2); it scales with the viewport. */
    constexpr float FigureTargetPixels = 72.0f;
    constexpr float FigureTargetViewportHeight = 1080.0f;
    /** The active unit (tile and head) is kept inside the safe rect shrunk to this fraction while the whole board is not in view. */
    constexpr float UnitFill = 0.9f;

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
    // Clamped to the zoom range: at MaxFramed, 1.4x would land past MaxDistance.
    inline float FarStop(float Framed) { return FMath::Min(FMath::Max(FarDistance, Framed * FarStopRatio), MaxDistance); }

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

    /** The ground point (view space XY on the plane PlaneZ) under a screen position (0..1 from the top left); false when the ray misses the plane. Inverse of Project on that plane. */
    inline bool Unproject(const FVector2D& Screen, const FVector& Eye, const FVector& Forward, float Aspect, float PlaneZ, FVector2D& Ground)
    {
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
        const FVector Up = FVector::CrossProduct(Forward, Right);
        const FVector Ray = Forward + Right * ((Screen.X - 0.5) * 2.0 * HalfTanX(Aspect)) - Up * ((Screen.Y - 0.5) * 2.0 * HalfTanY());
        if (Ray.Z > -1.0e-3) { return false; }
        const double T = (PlaneZ - Eye.Z) / Ray.Z;
        if (T <= 0.0) { return false; }
        Ground = FVector2D(Eye.X + Ray.X * T, Eye.Y + Ray.Y * T);
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
    /** The lean stops just inside the free area, so a bisection ending on the edge never rounds a corner out of it. */
    constexpr float LeanFill = 0.99f;

    inline FVector2D LimitLean(const FVector2D& Centre, const FVector2D& Target, int32 Width, int32 Height, const FBoardMetric& Metric,
        float Yaw, float Distance, float Pitch, const FSafeRect& Safe)
    {
        if (!BoardInside(Width, Height, Metric, Centre, Yaw, Distance, Pitch, Safe, 1.0f)) { return Target; } // the board cannot fit: free lean
        if (BoardInside(Width, Height, Metric, Target, Yaw, Distance, Pitch, Safe, LeanFill)) { return Target; }
        float Low = 0.0f, High = 1.0f;
        for (int32 Iteration = 0; Iteration < 16; ++Iteration)
        {
            const float Mid = 0.5f * (Low + High);
            (BoardInside(Width, Height, Metric, Centre + (Target - Centre) * Mid, Yaw, Distance, Pitch, Safe, LeanFill) ? Low : High) = Mid;
        }
        return Centre + (Target - Centre) * Low;
    }

    /** The fraction of the screen height a standing figure (feet on the board top, FigureCm tall) at the focus covers, from a camera at Distance and Pitch. Yaw does not matter for a vertical line through the focus. */
    inline float FigureScreenHeight(float Distance, float Pitch, const FBoardMetric& Metric, const FSafeRect& Safe)
    {
        const FArm Pose = Arm(0.0f, Distance, Pitch, Metric.Scale(), Safe);
        const FVector Eye = Pose.Offset + Pose.Framing; // the focus is the origin
        const FVector Forward = -Pose.Offset.GetSafeNormal();
        FVector2D Feet, Head;
        if (!Project(FVector(0.0, 0.0, Metric.BoardTop()), Eye, Forward, Safe.Aspect, Feet) ||
            !Project(FVector(0.0, 0.0, Metric.BoardTop() + Metric.FigureCm), Eye, Forward, Safe.Aspect, Head)) { return 0.0f; }
        return static_cast<float>(Feet.Y - Head.Y);
    }

    /**
     * The distance at which a standing figure at the focus is FigureTargetPixels tall at FigureTargetViewportHeight pixels
     * (a fixed fraction of the screen height, so 48 px at 720), with the pitch the zoom ramp gives that distance. The figure
     * shrinks as the camera pulls back, so this is a bisection; the result is inside the zoom range.
     */
    inline float FigureDistance(const FBoardMetric& Metric, float Anchor, const FSafeRect& Safe)
    {
        const float Wanted = FigureTargetPixels / FigureTargetViewportHeight;
        auto Height = [&](float D) { return FigureScreenHeight(D, PitchFor(D, Anchor), Metric, Safe); };
        if (Height(MinDistance) <= Wanted) { return MinDistance; }
        if (Height(MaxDistance) >= Wanted) { return MaxDistance; }
        float Low = MinDistance, High = MaxDistance; // Height(Low) > Wanted > Height(High)
        for (int32 Iteration = 0; Iteration < 40; ++Iteration)
        {
            const float Mid = 0.5f * (Low + High);
            (Height(Mid) > Wanted ? Low : High) = Mid;
        }
        return 0.5f * (Low + High);
    }

    /** The default zoom: the closer of the whole-board fit and the figure-sized distance, inside the zoom range. */
    inline float DefaultDistance(float WholeBoard, float Figure)
    { return FMath::Clamp(FMath::Min(WholeBoard, Figure), MinDistance, MaxDistance); }

    /**
     * The ground (view-space cm, on the board-top plane) under the four corners of the safe rect shrunk to InFill, as offsets
     * from the focus. Translating the camera translates these rigidly, so they bound what the camera shows for any focus.
     */
    inline bool VisibleGround(float Yaw, float Distance, float Pitch, const FBoardMetric& Metric, const FSafeRect& Safe, FVector2D (&Out)[4], float InFill = 1.0f)
    {
        const FArm Pose = Arm(Yaw, Distance, Pitch, Metric.Scale(), Safe);
        const FVector Eye = Pose.Offset + Pose.Framing;
        const FVector Forward = -Pose.Offset.GetSafeNormal();
        const FVector2D Centre = FVector2D(0.5, 0.5) + Safe.CentreOffset();
        const double HalfW = Safe.Width() * 0.5 * InFill, HalfH = Safe.Height() * 0.5 * InFill;
        int32 Index = 0;
        for (const double SignX : {-1.0, 1.0})
        {
            for (const double SignY : {-1.0, 1.0})
            {
                if (!Unproject(FVector2D(Centre.X + SignX * HalfW, Centre.Y + SignY * HalfH), Eye, Forward, Safe.Aspect, Metric.BoardTop(), Out[Index++])) { return false; }
            }
        }
        return true;
    }

    /** A pan-clamped focus, and whether each board axis was locked to the board centre because the view is wider than the bound. */
    struct FPanBound
    {
        FVector2D Focus = FVector2D::ZeroVector;
        bool bLockedX = false;
        bool bLockedY = false;
    };

    /**
     * Clamps a focus (view-space cm) so the visible ground (VisibleGround of the pose) stays within PanMarginCells of the
     * board's outer tile edge on every side. Per board axis: when the board plus its margins is smaller than the view the
     * focus is locked to the board centre on that axis (the pan has nothing to reach); otherwise it is clamped to the interval
     * that keeps the view inside. The bound is on the view's bounding box, so it never shows more than asked at any yaw.
     */
    inline FPanBound ClampPan(const FVector2D& Wanted, int32 Width, int32 Height, const FBoardMetric& Metric,
        float Yaw, float Distance, float Pitch, const FSafeRect& Safe)
    {
        const double C = Metric.CellCm;
        const double Margin = (0.5 + PanMarginCells) * C;
        const double LowerX = -Margin, UpperX = (Width - 1) * C + Margin;
        const double LowerY = -Margin, UpperY = (Height - 1) * C + Margin;
        FPanBound Result;
        Result.Focus = Wanted;
        FVector2D Ground[4];
        if (!VisibleGround(Yaw, Distance, Pitch, Metric, Safe, Ground))
        {
            // No ground under a corner (not reachable with the battle pitches): keep the focus itself inside the bound.
            Result.Focus = FVector2D(FMath::Clamp(Wanted.X, LowerX, UpperX), FMath::Clamp(Wanted.Y, LowerY, UpperY));
            return Result;
        }
        double MinX = Ground[0].X, MaxX = Ground[0].X, MinY = Ground[0].Y, MaxY = Ground[0].Y;
        for (const FVector2D& Point : Ground)
        {
            MinX = FMath::Min(MinX, Point.X); MaxX = FMath::Max(MaxX, Point.X);
            MinY = FMath::Min(MinY, Point.Y); MaxY = FMath::Max(MaxY, Point.Y);
        }
        const FVector2D Centre = BoardCentre(Width, Height, Metric);
        const double LowX = LowerX - MinX, HighX = UpperX - MaxX;
        const double LowY = LowerY - MinY, HighY = UpperY - MaxY;
        Result.bLockedX = LowX > HighX;
        Result.bLockedY = LowY > HighY;
        Result.Focus.X = Result.bLockedX ? Centre.X : FMath::Clamp(Wanted.X, LowX, HighX);
        Result.Focus.Y = Result.bLockedY ? Centre.Y : FMath::Clamp(Wanted.Y, LowY, HighY);
        return Result;
    }

    /** Whether a unit's tile (corners at the board top) and head project inside the safe rect shrunk to InFill, from a focus on the ground. */
    inline bool UnitInside(const FVector2D& Unit, const FBoardMetric& Metric, const FVector2D& FocusXY,
        float Yaw, float Distance, float Pitch, const FSafeRect& Safe, float InFill)
    {
        const FArm Pose = Arm(Yaw, Distance, Pitch, Metric.Scale(), Safe);
        const FVector Eye = FVector(FocusXY.X, FocusXY.Y, 0.0) + Pose.Offset + Pose.Framing;
        const FVector Forward = -Pose.Offset.GetSafeNormal();
        const double Half = 0.5 * Metric.CellCm, Z = Metric.BoardTop();
        const FVector Points[5] = {
            FVector(Unit.X - Half, Unit.Y - Half, Z), FVector(Unit.X + Half, Unit.Y - Half, Z),
            FVector(Unit.X - Half, Unit.Y + Half, Z), FVector(Unit.X + Half, Unit.Y + Half, Z),
            FVector(Unit.X, Unit.Y, Z + Metric.FigureCm)};
        for (const FVector& Point : Points)
        {
            FVector2D Screen;
            if (!Project(Point, Eye, Forward, Safe.Aspect, Screen) || !Safe.Holds(Screen, InFill)) { return false; }
        }
        return true;
    }

    /**
     * Moves Focus toward Unit only as far as brings the unit inside the safe rect (shrunk to InFill). The set of foci that show
     * the unit is convex, so the bisection along the segment is exact; a unit that cannot be shown even centred returns Unit.
     */
    inline FVector2D KeepUnitInside(const FVector2D& Focus, const FVector2D& Unit, const FBoardMetric& Metric,
        float Yaw, float Distance, float Pitch, const FSafeRect& Safe, float InFill = UnitFill)
    {
        auto Inside = [&](const FVector2D& At) { return UnitInside(Unit, Metric, At, Yaw, Distance, Pitch, Safe, InFill); };
        if (Inside(Focus)) { return Focus; }
        if (!Inside(Unit)) { return Unit; }
        float Low = 0.0f, High = 1.0f; // Low is outside, High is inside
        for (int32 Iteration = 0; Iteration < 16; ++Iteration)
        {
            const float Mid = 0.5f * (Low + High);
            (Inside(Focus + (Unit - Focus) * Mid) ? High : Low) = Mid;
        }
        return Focus + (Unit - Focus) * High;
    }
}
