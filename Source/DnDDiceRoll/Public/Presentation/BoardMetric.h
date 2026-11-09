#pragma once

#include "CoreMinimal.h"

/**
 * The single presentation metric of a battle board. Every conversion between a board cell and a
 * world position goes through it, so the cell size lives in one place (the theme supplies it).
 * Presentation only: rules, generation and saves never see it.
 *
 * Positions are relative to the battle view actor. The r001 art and the camera framing were
 * authored for a 110 cm cell (ReferenceCellCm); Scale() is the ratio of this metric to that
 * reference, and "reference" lengths (offsets, heights and camera distances written for r001)
 * are converted with Scaled(). BoardTop and CursorPlane keep their r001 proportions.
 */
struct FBoardMetric
{
    /** The cell size r001 art, the camera framing and poses.json distances were authored for. */
    static constexpr float ReferenceCellCm = 110.0f;
    static constexpr float ReferenceBoardTop = 31.0f;
    static constexpr float ReferenceCursorPlane = 35.0f;
    /** Default: a true 5 ft cell, and a figure about 6 ft tall. */
    static constexpr float DefaultCellCm = 150.0f;
    static constexpr float DefaultFigureCm = 180.0f;
    /** Diameter of a figure's footprint ring on the default 150 cm cell (#142); other cells scale it. */
    static constexpr float FootprintRingCm = 56.0f;

    float CellCm = DefaultCellCm;
    float FigureCm = DefaultFigureCm;

    FBoardMetric() = default;
    FBoardMetric(float InCellCm, float InFigureCm)
        : CellCm(FMath::Max(1.0f, InCellCm)), FigureCm(FMath::Max(1.0f, InFigureCm)) {}

    float Scale() const { return CellCm / ReferenceCellCm; }
    /** A length authored for the 110 cm reference cell, in world cm. */
    float Scaled(float ReferenceCm) const { return ReferenceCm * Scale(); }
    /** The height of the playing surface (tile tops). */
    float BoardTop() const { return Scaled(ReferenceBoardTop); }
    /** The plane the pointer is intersected with. */
    float CursorPlane() const { return Scaled(ReferenceCursorPlane); }
    /** The footprint ring diameter in world cm: 56 cm at the default cell, scaled by the ratio of this cell to 150 cm. */
    float FootprintDiameter() const { return FootprintRingCm * CellCm / DefaultCellCm; }
    /** A height as a fraction of the figure height above the board (1 is the top of the head). */
    float Figure(float Fraction) const { return FigureCm * Fraction; }

    FVector CellPosition(FIntPoint Cell) const { return FVector(Cell.X * CellCm, Cell.Y * CellCm, CursorPlane()); }
    /** Height is in world cm above the board top. */
    FVector BoardPosition(FVector2D Cell, float Height = 0.0f) const { return FVector(Cell.X * CellCm, Cell.Y * CellCm, BoardTop() + Height); }
    /** The cell containing a point in the view's space (cells are centred on their positions). */
    FIntPoint CellAt(const FVector& Point) const { return FIntPoint(FMath::RoundToInt(Point.X / CellCm), FMath::RoundToInt(Point.Y / CellCm)); }

    /** Positions for parts that live under the scenery root, which is scaled by Scale(): reference-space coordinates. */
    FVector ReferencePosition(FVector2D Cell, float ReferenceHeight = 0.0f) const
    { return FVector(Cell.X * ReferenceCellCm, Cell.Y * ReferenceCellCm, ReferenceBoardTop + ReferenceHeight); }
};
