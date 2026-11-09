#include "Rules/AreaRules.h"

bool FAreaRules::Covers(const FAreaTemplate& Area, FIntPoint Cell)
{
    const int64 Size = FMath::Max(Area.SizeFeet, 0) / FeetPerCell;
    const FIntPoint V = Cell - Area.Origin;
    const FIntPoint D = Area.Direction;
    const int64 Norm = (int64)D.X * D.X + (int64)D.Y * D.Y;
    const int64 Dot = (int64)V.X * D.X + (int64)V.Y * D.Y;
    const int64 Cross = (int64)V.X * D.Y - (int64)V.Y * D.X;
    switch (Area.Shape)
    {
    case EAreaShape::Sphere:
    case EAreaShape::Cylinder:
    {
        // Cell centers within the radius of the intersection (Origin is the cell's top-left corner).
        const int64 X = 2 * (int64)V.X + 1, Y = 2 * (int64)V.Y + 1;
        return X * X + Y * Y <= 4 * Size * Size;
    }
    case EAreaShape::Cone:
        // Width at any distance equals that distance: perpendicular offset <= half the forward distance.
        return Norm > 0 && Dot > 0 && Dot * Dot <= Size * Size * Norm && 4 * Cross * Cross <= Dot * Dot;
    case EAreaShape::Line:
        // 5 ft wide: cell centers within half a cell of the aim line.
        return Norm > 0 && Dot > 0 && Dot * Dot <= Size * Size * Norm && 4 * Cross * Cross <= Norm;
    case EAreaShape::Cube:
    {
        if (Norm == 0 || Size == 0) { return false; }
        const FIntPoint Axis = FMath::Abs(D.X) >= FMath::Abs(D.Y) ? FIntPoint(FMath::Sign(D.X), 0) : FIntPoint(0, FMath::Sign(D.Y));
        const int64 Forward = (int64)V.X * Axis.X + (int64)V.Y * Axis.Y;
        const int64 Side = (int64)V.Y * Axis.X - (int64)V.X * Axis.Y;
        // Side cells run from -(n-1)/2 to n/2 so odd cubes are centered on the caster's row.
        return Forward >= 1 && Forward <= Size && Side >= -((Size - 1) / 2) && Side <= Size / 2;
    }
    default: return false;
    }
}

TArray<FIntPoint> FAreaRules::Cells(const FAreaTemplate& Area)
{
    TArray<FIntPoint> Out;
    const int32 Reach = FMath::Max(Area.SizeFeet, 0) / FeetPerCell + 1;
    for (int32 X = Area.Origin.X - Reach; X <= Area.Origin.X + Reach; ++X)
    {
        for (int32 Y = Area.Origin.Y - Reach; Y <= Area.Origin.Y + Reach; ++Y)
        {
            if (Covers(Area, FIntPoint(X, Y))) { Out.Add(FIntPoint(X, Y)); }
        }
    }
    return Out;
}
