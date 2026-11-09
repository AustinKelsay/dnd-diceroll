#pragma once

#include "CoreMinimal.h"

/** SRD 5.2.1 area-of-effect shapes. */
enum class EAreaShape : uint8 { Cone, Cube, Cylinder, Line, Sphere };

/** One area on the 5-foot grid. Origin is a cell for cone/cube/line and a grid intersection for sphere/cylinder. */
struct DNDDICEROLL_API FAreaTemplate
{
    EAreaShape Shape = EAreaShape::Sphere;
    /** Cone/line: caster's cell. Cube: caster's cell (the cube starts on its face). Sphere/cylinder: the intersection at the cell's top-left corner. */
    FIntPoint Origin = FIntPoint::ZeroValue;
    /** Cone/line/cube: aim, as a cell offset from Origin (any non-zero vector; cubes snap to the dominant axis). */
    FIntPoint Direction = FIntPoint(1, 0);
    /** Cone/line length, cube side, or sphere/cylinder radius, in feet. */
    int32 SizeFeet = 15;
};

/** Grid templates. Pure geometry, no RNG; previews and resolution call the same function. */
struct DNDDICEROLL_API FAreaRules
{
    static constexpr int32 FeetPerCell = 5;
    /** Cells covered, sorted by X then Y. The origin cell of a cone, cube or line is never included. */
    static TArray<FIntPoint> Cells(const FAreaTemplate& Area);
    static bool Covers(const FAreaTemplate& Area, FIntPoint Cell);
};
