#include "Presentation/BrazierRing.h"

#include "Presentation/VigilKit.h"

namespace BrazierRing
{
int32 EdgeCount(int32 Length)
{
    return FMath::Max(MinPerEdge, Length / CellsPerBrazier);
}

int32 Count(int32 Width, int32 Height)
{
    return 2 * (EdgeCount(Width) + EdgeCount(Height));
}

TArray<FBrazier> Plan(int32 Width, int32 Height, int32 Seed)
{
    TArray<FBrazier> Result;
    Result.Reserve(Count(Width, Height));
    for (const EEdge Edge : {EEdge::South, EEdge::North, EEdge::West, EEdge::East})
    {
        const bool bAlongX = Edge == EEdge::South || Edge == EEdge::North;
        const int32 Length = bAlongX ? Width : Height;
        const int32 Slots = EdgeCount(Length);
        // The edge of the playable cells is half a cell beyond the outermost centres.
        const float Across = Edge == EEdge::South || Edge == EEdge::West ? -0.5f - OutsideCells
            : (bAlongX ? Height : Width) - 0.5f + OutsideCells;
        for (int32 Slot = 0; Slot < Slots; ++Slot)
        {
            const float Jitter = (VigilKit::Unit(Seed, 41, FIntPoint(static_cast<int32>(Edge), Slot)) * 2.0f - 1.0f) * JitterCells;
            const float Along = (Slot + 0.5f) * static_cast<float>(Length) / Slots - 0.5f + Jitter;
            FBrazier Brazier;
            Brazier.Edge = Edge;
            Brazier.Slot = Slot;
            Brazier.Cell = bAlongX ? FVector2D(Along, Across) : FVector2D(Across, Along);
            Result.Add(Brazier);
        }
    }
    return Result;
}

TArray<FVector2D> Cells(const TArray<FBrazier>& Braziers)
{
    TArray<FVector2D> Result;
    Result.Reserve(Braziers.Num());
    for (const FBrazier& Brazier : Braziers) { Result.Add(Brazier.Cell); }
    return Result;
}

float Intensity(int32 BrazierCount)
{
    return TotalIntensity / static_cast<float>(FMath::Max(BrazierCount, 1));
}

bool OutsideBoard(FVector2D Cell, int32 Width, int32 Height, float Clearance)
{
    const float Out = static_cast<float>(FMath::Max(FMath::Max(-0.5 - Cell.X, Cell.X - (Width - 0.5)), FMath::Max(-0.5 - Cell.Y, Cell.Y - (Height - 0.5))));
    return Out > Clearance;
}
}
