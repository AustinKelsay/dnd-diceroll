#pragma once

#include "CoreMinimal.h"

/**
 * Pointy-top hex grid, 1 hex = 5 ft. Cells are axial coordinates stored in FIntPoint (X = q, Y = r);
 * cube math (s = -q - r) is derived on demand and never stored. Pure geometry, no RNG, no state.
 * Boards are odd-r offset rectangles: a cell is on a Width x Height board when its offset column and row are in range.
 */
struct DNDDICEROLL_API FHexGrid
{
    static constexpr int32 FeetPerCell = 5;
    static constexpr int32 DirectionCount = 6;
    /** Axial unit steps, counterclockwise on screen (r grows downward): E, NE, NW, W, SW, SE. */
    static const FIntPoint Directions[DirectionCount];

    static FIntPoint Direction(int32 Index);
    /** Hex steps between two cells. */
    static int32 Distance(FIntPoint A, FIntPoint B);
    /** Hex steps from the origin to an axial offset. */
    static int32 Length(FIntPoint Offset);
    static bool Adjacent(FIntPoint A, FIntPoint B);
    /** The six neighbors, in Directions order. */
    static TArray<FIntPoint> Neighbors(FIntPoint Cell);
    /** The 6 * Radius cells exactly Radius steps away (the center alone for 0), starting E and going counterclockwise. */
    static TArray<FIntPoint> Ring(FIntPoint Center, int32 Radius);
    /** Every cell within Radius steps: 1 + 3R(R+1) cells, sorted by X then Y. */
    static TArray<FIntPoint> Range(FIntPoint Center, int32 Radius);
    /** Rotate an axial offset by Steps * 60 degrees counterclockwise on screen (E -> NE). */
    static FIntPoint Rotate(FIntPoint Offset, int32 Steps);
    /** Nearest of the six directions to an offset (ties resolve to the lower index); INDEX_NONE for zero. */
    static int32 NearestDirection(FIntPoint Offset);
    /** Rounds fractional axial coordinates to the containing hex. */
    static FIntPoint Round(double Q, double R);
    /** Cells from A to B inclusive, Distance + 1 of them. Bias picks the side when the line runs along a hex edge (+1 or -1). */
    static TArray<FIntPoint> Line(FIntPoint A, FIntPoint B, int32 Bias = 1);
    /** Hex center in world units for a circumradius Size (pointy-top). */
    static FVector2D ToWorld(FIntPoint Cell, double Size);
    static FIntPoint FromWorld(FVector2D Point, double Size);
    /** Odd-r offset (column, row) <-> axial. */
    static FIntPoint FromOffset(int32 Column, int32 Row);
    static FIntPoint ToOffset(FIntPoint Cell);
    static bool OnBoard(FIntPoint Cell, int32 Width, int32 Height);
    /** Every cell of a Width x Height board, row by row. */
    static TArray<FIntPoint> BoardCells(int32 Width, int32 Height);
    /**
     * Cheapest step totals from Start within Budget. StepCost returns the cost of entering To from From,
     * or <= 0 when the step is not allowed. Start costs 0.
     */
    static TMap<FIntPoint, int32> Reachable(FIntPoint Start, int32 Budget, TFunctionRef<int32(FIntPoint From, FIntPoint To)> StepCost);
};
