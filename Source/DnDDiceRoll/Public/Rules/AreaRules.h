#pragma once

#include "CoreMinimal.h"

/** SRD 5.2.1 area-of-effect shapes. */
enum class EAreaShape : uint8 { Cone, Cube, Cylinder, Line, Sphere };

/** One area on the 5-foot hex grid, in board cells (odd-r offset column/row, see FHexGrid). */
struct DNDDICEROLL_API FAreaTemplate
{
    EAreaShape Shape = EAreaShape::Sphere;
    /** Cone/line/cube: the caster's hex (the area starts on its edge). Sphere/cylinder: the center hex. */
    FIntPoint Origin = FIntPoint::ZeroValue;
    /** Cone/line/cube: aim, as the board-cell offset from Origin to the aimed cell (Aim - Origin; any non-zero offset). Cones and cubes snap to the nearest of the six hex directions. */
    FIntPoint Direction = FIntPoint(1, 0);
    /** Cone/line length, cube side, or sphere/cylinder radius, in feet. */
    int32 SizeFeet = 15;
};

/**
 * Hex templates. Pure geometry, no RNG; previews and resolution call the same function.
 * Sphere/cylinder: every hex within radius/5 steps of the center (1 + 3N(N+1) hexes).
 * Cone: hexes 1..length/5 steps out inside the 60-degree wedge centered on the aimed hex direction (edges inclusive).
 * Line: one hex wide, length/5 hexes along the hex line toward the aim.
 * Cube: a hex blob of radius floor(side/10) whose near edge touches the caster (odd sides exact, even sides round up).
 */
struct DNDDICEROLL_API FAreaRules
{
    static constexpr int32 FeetPerCell = 5;
    /** Cells covered, sorted by X then Y. The origin cell of a cone, cube or line is never included. */
    static TArray<FIntPoint> Cells(const FAreaTemplate& Area);
    static bool Covers(const FAreaTemplate& Area, FIntPoint Cell);
};
