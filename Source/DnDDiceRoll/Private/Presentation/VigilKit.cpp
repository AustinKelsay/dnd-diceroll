#include "Presentation/VigilKit.h"

namespace VigilKit
{
namespace
{
uint32 Combine(uint32 State, uint32 Value)
{
    State ^= Value + 0x9E3779B9u + (State << 6) + (State >> 2);
    return State;
}

uint32 Finish(uint32 State)
{
    State ^= State >> 16; State *= 0x7FEB352Du;
    State ^= State >> 15; State *= 0x846CA68Bu;
    State ^= State >> 16;
    return State;
}

struct FCandidate
{
    uint32 Score = 0;
    FIntPoint Cell = FIntPoint::ZeroValue;
};

/** The Count lowest-scoring candidates not already taken, ties broken by cell, so the pick never depends on scan order. */
void Take(TArray<FCandidate>& Candidates, int32 Count, TSet<FIntPoint>& Taken, TArray<FIntPoint>& Picked)
{
    Candidates.Sort([](const FCandidate& A, const FCandidate& B)
    {
        if (A.Score != B.Score) { return A.Score < B.Score; }
        return A.Cell.Y != B.Cell.Y ? A.Cell.Y < B.Cell.Y : A.Cell.X < B.Cell.X;
    });
    for (const FCandidate& Candidate : Candidates)
    {
        if (Picked.Num() >= Count) { break; }
        if (Taken.Contains(Candidate.Cell)) { continue; }
        Taken.Add(Candidate.Cell);
        Picked.Add(Candidate.Cell);
    }
}
}

uint32 Hash(int32 Seed, int32 Salt, FIntPoint Cell)
{
    uint32 Value = 0x811C9DC5u;
    Value = Combine(Value, static_cast<uint32>(Seed));
    Value = Combine(Value, static_cast<uint32>(Salt));
    Value = Combine(Value, static_cast<uint32>(Cell.X));
    Value = Combine(Value, static_cast<uint32>(Cell.Y));
    return Finish(Value);
}

float Unit(int32 Seed, int32 Salt, FIntPoint Cell)
{
    return static_cast<float>(Hash(Seed, Salt, Cell) >> 8) / 16777216.0f;
}

int32 VariantIndex(int32 Seed, int32 Recipe, FIntPoint Cell, int32 Count)
{
    return Count <= 1 ? 0 : static_cast<int32>(Hash(Seed, 1000 + Recipe, Cell) % static_cast<uint32>(Count));
}

int32 RingDistance(FIntPoint Cell, int32 Width, int32 Height)
{
    const int32 DX = FMath::Max3(0, -Cell.X, Cell.X - (Width - 1));
    const int32 DY = FMath::Max3(0, -Cell.Y, Cell.Y - (Height - 1));
    return FMath::Max(DX, DY);
}

TArray<FArch> ArchPairs(const TArray<FIntPoint>& Blocked)
{
    TArray<FIntPoint> Ordered = Blocked;
    Ordered.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.Y != B.Y ? A.Y < B.Y : A.X < B.X; });
    const TSet<FIntPoint> Cells(Ordered);
    TSet<FIntPoint> Used;
    TArray<FArch> Arches;
    for (const FIntPoint& Cell : Ordered)
    {
        if (Used.Contains(Cell)) { continue; }
        const FIntPoint East(Cell.X + 1, Cell.Y), South(Cell.X, Cell.Y + 1);
        FArch Arch;
        Arch.A = Cell;
        if (Cells.Contains(East) && !Used.Contains(East)) { Arch.B = East; Arch.bAlongX = true; }
        else if (Cells.Contains(South) && !Used.Contains(South)) { Arch.B = South; Arch.bAlongX = false; }
        else { continue; }
        Used.Add(Arch.A); Used.Add(Arch.B);
        Arches.Add(Arch);
    }
    return Arches;
}

TArray<FDressing> PlanDressing(int32 Width, int32 Height, int32 Seed, const FDressingKinds& Kinds, const TArray<FVector2D>& Avoid, float AvoidRadius)
{
    TArray<FCandidate> Scatter, Mist, Shaft;
    for (int32 Y = -MistMaxRing; Y < Height + MistMaxRing; ++Y)
    {
        for (int32 X = -MistMaxRing; X < Width + MistMaxRing; ++X)
        {
            const FIntPoint Cell(X, Y);
            const int32 Ring = RingDistance(Cell, Width, Height);
            if (Ring >= ScatterMinRing && Ring <= ScatterMaxRing)
            {
                const bool bBlocked = Avoid.ContainsByPredicate([&](const FVector2D& Point)
                { return FVector2D::DistSquared(Point, FVector2D(Cell.X, Cell.Y)) < FMath::Square(static_cast<double>(AvoidRadius)); });
                if (!bBlocked) { Scatter.Add({Hash(Seed, 11, Cell), Cell}); }
            }
            if (Ring >= MistMinRing && Ring <= MistMaxRing) { Mist.Add({Hash(Seed, 12, Cell), Cell}); }
            if (Ring >= MistMinRing && Ring <= MistMaxRing && X <= ShaftMaxX) { Shaft.Add({Hash(Seed, 13, Cell), Cell}); }
        }
    }
    TSet<FIntPoint> Taken;
    TArray<FIntPoint> ScatterCells, MistCells, ShaftCells;
    if (Kinds.Scatter > 0) { Take(Scatter, ScatterCount, Taken, ScatterCells); }
    if (Kinds.Mist > 0) { Take(Mist, MistCount, Taken, MistCells); }
    if (Kinds.Shaft > 0) { Take(Shaft, ShaftCount, Taken, ShaftCells); }
    TArray<FDressing> Result;
    auto Add = [&](EDressingKind Kind, const FIntPoint& Cell, int32 Count, float Wander)
    {
        FDressing Item;
        Item.Kind = Kind;
        Item.Cell = Cell;
        Item.Variant = static_cast<int32>(Hash(Seed, 21 + static_cast<int32>(Kind), Cell) % static_cast<uint32>(Count));
        Item.Offset = FVector2D((Unit(Seed, 31, Cell) - 0.5f) * 2.0f * Wander, (Unit(Seed, 32, Cell) - 0.5f) * 2.0f * Wander);
        Item.Yaw = Unit(Seed, 33, Cell) * 360.0f;
        Result.Add(Item);
    };
    for (const FIntPoint& Cell : ScatterCells) { Add(EDressingKind::Scatter, Cell, Kinds.Scatter, 0.2f); }
    for (const FIntPoint& Cell : MistCells) { Add(EDressingKind::Mist, Cell, Kinds.Mist, 0.25f); }
    for (const FIntPoint& Cell : ShaftCells) { Add(EDressingKind::Shaft, Cell, Kinds.Shaft, 0.0f); }
    return Result;
}
}
