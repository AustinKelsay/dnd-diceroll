#include "Presentation/BoardOverlay.h"
#include "Presentation/ExpeditionBattleView.h"

namespace BoardOverlay
{
TArray<FIntPoint> FMask::Cells(uint8 Bit) const
{
    TArray<FIntPoint> Result;
    for (int32 Y = 0; Y < Height; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        { if (Bits[Y * Width + X] & Bit) { Result.Add(FIntPoint(X, Y)); } }
    }
    return Result;
}

FMask BuildMask(int32 Width, int32 Height, const FBoardHighlights& Highlights, const TArray<FIntPoint>& VictimCells)
{
    FMask Mask;
    Mask.Width = FMath::Max(0, Width);
    Mask.Height = FMath::Max(0, Height);
    Mask.Bits.SetNumZeroed(Mask.Width * Mask.Height);
    auto Mark = [&Mask](const TArray<FIntPoint>& Cells, uint8 Bit)
    { for (const FIntPoint& Cell : Cells) { if (Mask.IsValidCell(Cell)) { Mask.Bits[Cell.Y * Mask.Width + Cell.X] |= Bit; } } };
    Mark(Highlights.Moves, Move);
    Mark(Highlights.Reach.Visible, Reach);
    Mark(Highlights.Reach.Obstructed, Obstructed);
    Mark(Highlights.Path, Path);
    Mark(VictimCells, Victim);
    if (Highlights.bHasHover) { Mark({Highlights.Hover}, Hover); }
    return Mask;
}

namespace
{
void Add(TArray<FBar>& Out, EStyle Style, FVector2D A, FVector2D B, float Width) { Out.Add({A, B, Width, Style}); }

/**
 * The outline of every cell carrying Bit: a bar per maximal straight run of edges that face a cell without the bit
 * (or the board edge). Runs extend half a width past their ends so corners close exactly, convex or concave.
 */
void Contour(const FMask& Mask, uint8 Bit, EStyle Style, TArray<FBar>& Out)
{
    const float Half = ContourWidth * 0.5f;
    // Edges facing +-Y run along X; edges facing +-X run along Y.
    for (const int32 Side : {-1, 1})
    {
        for (int32 Y = 0; Y < Mask.Height; ++Y)
        {
            for (int32 X = 0; X < Mask.Width;)
            {
                const auto Edge = [&](int32 Along) { return Mask.Has(FIntPoint(Along, Y), Bit) && !Mask.Has(FIntPoint(Along, Y + Side), Bit); };
                if (!Edge(X)) { ++X; continue; }
                int32 End = X;
                while (End + 1 < Mask.Width && Edge(End + 1)) { ++End; }
                const float Line = Y + Side * 0.5f;
                Add(Out, Style, FVector2D(X - 0.5f - Half, Line), FVector2D(End + 0.5f + Half, Line), ContourWidth);
                X = End + 1;
            }
        }
        for (int32 X = 0; X < Mask.Width; ++X)
        {
            for (int32 Y = 0; Y < Mask.Height;)
            {
                const auto Edge = [&](int32 Along) { return Mask.Has(FIntPoint(X, Along), Bit) && !Mask.Has(FIntPoint(X + Side, Along), Bit); };
                if (!Edge(Y)) { ++Y; continue; }
                int32 End = Y;
                while (End + 1 < Mask.Height && Edge(End + 1)) { ++End; }
                const float Line = X + Side * 0.5f;
                Add(Out, Style, FVector2D(Line, Y - 0.5f - Half), FVector2D(Line, End + 0.5f + Half), ContourWidth);
                Y = End + 1;
            }
        }
    }
}

/** Four corner brackets of a cell, inset from its edges. */
void Brackets(FIntPoint Cell, EStyle Style, TArray<FBar>& Out)
{
    const float Half = BracketWidth * 0.5f;
    for (const int32 SX : {-1, 1})
    {
        for (const int32 SY : {-1, 1})
        {
            const FVector2D Corner(Cell.X + SX * BracketInset, Cell.Y + SY * BracketInset);
            Add(Out, Style, Corner + FVector2D(SX * Half, 0.0f), Corner - FVector2D(SX * BracketArm, 0.0f), BracketWidth);
            Add(Out, Style, Corner + FVector2D(0.0f, SY * Half), Corner - FVector2D(0.0f, SY * BracketArm), BracketWidth);
        }
    }
}

/** A closed square frame around a cell. */
void Frame(FIntPoint Cell, EStyle Style, TArray<FBar>& Out)
{
    const float Half = BracketWidth * 0.5f;
    const float Edge = BracketInset + 0.02f;
    for (const int32 Sign : {-1, 1})
    {
        Add(Out, Style, FVector2D(Cell.X - Edge - Half, Cell.Y + Sign * Edge), FVector2D(Cell.X + Edge + Half, Cell.Y + Sign * Edge), BracketWidth);
        Add(Out, Style, FVector2D(Cell.X + Sign * Edge, Cell.Y - Edge - Half), FVector2D(Cell.X + Sign * Edge, Cell.Y + Edge + Half), BracketWidth);
    }
}
}

void BuildBars(const FMask& Mask, const FBoardHighlights& Highlights, bool bFullGrid, TArray<FBar>& Out)
{
    Out.Reset();
    if (bFullGrid)
    {
        const float Half = GridWidth * 0.5f;
        for (int32 X = 0; X <= Mask.Width; ++X) { Add(Out, EStyle::Grid, FVector2D(X - 0.5f, -0.5f - Half), FVector2D(X - 0.5f, Mask.Height - 0.5f + Half), GridWidth); }
        for (int32 Y = 0; Y <= Mask.Height; ++Y) { Add(Out, EStyle::Grid, FVector2D(-0.5f - Half, Y - 0.5f), FVector2D(Mask.Width - 0.5f + Half, Y - 0.5f), GridWidth); }
    }
    Contour(Mask, Move, EStyle::MoveContour, Out);
    Contour(Mask, Reach, EStyle::ReachContour, Out);

    // Corner ticks: a small plus at every lattice vertex touching a reachable cell (four L-shaped corners of the adjoining cells).
    for (int32 VY = 0; VY <= Mask.Height; ++VY)
    {
        for (int32 VX = 0; VX <= Mask.Width; ++VX)
        {
            bool bNear = false;
            for (const FIntPoint Cell : {FIntPoint(VX - 1, VY - 1), FIntPoint(VX, VY - 1), FIntPoint(VX - 1, VY), FIntPoint(VX, VY)})
            { bNear |= (Mask.At(Cell) & (Move | Reach)) != 0; }
            if (!bNear) { continue; }
            const FVector2D At(VX - 0.5f, VY - 0.5f);
            Add(Out, EStyle::Tick, At - FVector2D(TickHalfLength, 0.0f), At + FVector2D(TickHalfLength, 0.0f), TickWidth);
            Add(Out, EStyle::Tick, At - FVector2D(0.0f, TickHalfLength), At + FVector2D(0.0f, TickHalfLength), TickWidth);
        }
    }

    // Blocked sight: a subtle cross on each cell in range with no clear line.
    for (const FIntPoint& Cell : Mask.Cells(Obstructed))
    {
        const FVector2D C(Cell.X, Cell.Y);
        Add(Out, EStyle::Blocked, C + FVector2D(-BlockedHalfDiagonal, -BlockedHalfDiagonal), C + FVector2D(BlockedHalfDiagonal, BlockedHalfDiagonal), BlockedWidth);
        Add(Out, EStyle::Blocked, C + FVector2D(-BlockedHalfDiagonal, BlockedHalfDiagonal), C + FVector2D(BlockedHalfDiagonal, -BlockedHalfDiagonal), BlockedWidth);
    }

    // Path ribbon: legs between cell centres (cardinal for the cardinal rules), each extended half a width so corners are square.
    if (!Highlights.Path.IsEmpty())
    {
        TArray<FIntPoint> Points;
        if (Highlights.bHasOrigin) { Points.Add(Highlights.Origin); }
        Points.Append(Highlights.Path);
        const float Half = RibbonWidth * 0.5f;
        for (int32 Index = 1; Index < Points.Num(); ++Index)
        {
            const FVector2D From(Points[Index - 1].X, Points[Index - 1].Y), To(Points[Index].X, Points[Index].Y);
            const FVector2D Along = (To - From).GetSafeNormal();
            Add(Out, EStyle::Ribbon, From - Along * Half, To + Along * Half, RibbonWidth);
        }
        // One cost tick per step, across the ribbon at the cell entered.
        const int32 First = Highlights.bHasOrigin ? 1 : 0;
        for (int32 Index = First; Index < Points.Num(); ++Index)
        {
            const FVector2D To(Points[Index].X, Points[Index].Y);
            const FVector2D From = Index > 0 ? FVector2D(Points[Index - 1].X, Points[Index - 1].Y) : To - FVector2D(1.0f, 0.0f);
            const FVector2D Along = (To - From).GetSafeNormal();
            const FVector2D Across(-Along.Y, Along.X);
            Add(Out, EStyle::Ribbon, To - Across * CostTickHalfLength, To + Across * CostTickHalfLength, CostTickWidth);
        }
        Brackets(Highlights.Path.Last(), EStyle::PathBracket, Out);
    }

    for (const FIntPoint& Cell : Mask.Cells(Victim)) { Frame(Cell, EStyle::Victim, Out); }
    // The hover bracket marks the pointer cell; the destination already has its own.
    if (Highlights.bHasHover && Mask.IsValidCell(Highlights.Hover) && (Highlights.Path.IsEmpty() || Highlights.Path.Last() != Highlights.Hover))
    { Brackets(Highlights.Hover, EStyle::HoverBracket, Out); }
}
}
