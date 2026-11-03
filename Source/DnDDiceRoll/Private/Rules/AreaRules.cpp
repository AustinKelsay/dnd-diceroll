#include "Rules/AreaRules.h"
#include "Rules/HexGrid.h"

namespace
{
int32 SizeCells(const FAreaTemplate& Area) { return FMath::Max(Area.SizeFeet, 0) / FAreaRules::FeetPerCell; }

/** The line hexes after the origin, in order. */
TArray<FIntPoint> LineCells(const FAreaTemplate& Area)
{
    TArray<FIntPoint> Out;
    const int32 Size = SizeCells(Area);
    const int32 AimLength = FHexGrid::Length(Area.Direction);
    if (Size <= 0 || AimLength == 0) { return Out; }
    // Extend the aim far enough to cover the full length, then keep the first Size steps.
    const int32 Scale = FMath::DivideAndRoundUp(Size, AimLength);
    const TArray<FIntPoint> Path = FHexGrid::Line(Area.Origin, Area.Origin + Area.Direction * Scale);
    for (int32 Index = 1; Index <= Size && Index < Path.Num(); ++Index) { Out.Add(Path[Index]); }
    return Out;
}

/** Axial-space test; Area here holds an axial Origin and axial Direction. */
bool AxialCovers(const FAreaTemplate& Area, FIntPoint Cell)
{
    const int32 Size = SizeCells(Area);
    const FIntPoint V = Cell - Area.Origin;
    switch (Area.Shape)
    {
    case EAreaShape::Sphere:
    case EAreaShape::Cylinder:
        return FHexGrid::Length(V) <= Size;
    case EAreaShape::Cone:
    {
        const int32 Dir = FHexGrid::NearestDirection(Area.Direction);
        if (Dir == INDEX_NONE) { return false; }
        // Rotate so the aim points E, then keep hexes within 30 degrees of the E axis:
        // |y| <= x tan30 with x = sqrt3 (q + r/2), y = 1.5 r  ->  3|r| <= 2q + r.
        const FIntPoint L = FHexGrid::Rotate(V, -Dir);
        const int32 Steps = FHexGrid::Length(L);
        return Steps >= 1 && Steps <= Size && 3 * FMath::Abs(L.Y) <= 2 * L.X + L.Y;
    }
    case EAreaShape::Line:
        return LineCells(Area).Contains(Cell);
    case EAreaShape::Cube:
    {
        const int32 Dir = FHexGrid::NearestDirection(Area.Direction);
        if (Dir == INDEX_NONE || Size == 0) { return false; }
        const int32 Radius = Size / 2;
        return FHexGrid::Distance(Cell, Area.Origin + FHexGrid::Direction(Dir) * (Radius + 1)) <= Radius;
    }
    default: return false;
    }
}

/** Board template -> axial template. */
FAreaTemplate ToAxial(const FAreaTemplate& Area)
{
    FAreaTemplate Out = Area;
    Out.Origin = FHexGrid::Axial(Area.Origin);
    Out.Direction = FHexGrid::Axial(Area.Origin + Area.Direction) - Out.Origin;
    return Out;
}
}

bool FAreaRules::Covers(const FAreaTemplate& Area, FIntPoint Cell)
{
    return AxialCovers(ToAxial(Area), FHexGrid::Axial(Cell));
}

TArray<FIntPoint> FAreaRules::Cells(const FAreaTemplate& Area)
{
    TArray<FIntPoint> Out;
    const FAreaTemplate Hex = ToAxial(Area);
    for (const FIntPoint& Cell : FHexGrid::Range(Hex.Origin, SizeCells(Area) + 1))
    {
        if (AxialCovers(Hex, Cell)) { Out.Add(FHexGrid::Board(Cell)); }
    }
    Out.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
    return Out;
}
