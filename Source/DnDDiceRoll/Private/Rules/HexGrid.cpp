#include "Rules/HexGrid.h"

const FIntPoint FHexGrid::Directions[FHexGrid::DirectionCount] = {
    FIntPoint(1, 0), FIntPoint(1, -1), FIntPoint(0, -1), FIntPoint(-1, 0), FIntPoint(-1, 1), FIntPoint(0, 1)};

FIntPoint FHexGrid::Direction(int32 Index)
{
    return Directions[((Index % DirectionCount) + DirectionCount) % DirectionCount];
}

int32 FHexGrid::Length(FIntPoint V)
{
    return FMath::Max3(FMath::Abs(V.X), FMath::Abs(V.Y), FMath::Abs(V.X + V.Y));
}

int32 FHexGrid::Distance(FIntPoint A, FIntPoint B) { return Length(A - B); }

bool FHexGrid::Adjacent(FIntPoint A, FIntPoint B) { return Distance(A, B) == 1; }

TArray<FIntPoint> FHexGrid::Neighbors(FIntPoint Cell)
{
    TArray<FIntPoint> Out;
    for (const FIntPoint& D : Directions) { Out.Add(Cell + D); }
    return Out;
}

TArray<FIntPoint> FHexGrid::Ring(FIntPoint Center, int32 Radius)
{
    TArray<FIntPoint> Out;
    if (Radius < 0) { return Out; }
    if (Radius == 0) { Out.Add(Center); return Out; }
    // Start on the E corner and walk each side; side k runs toward direction k + 2.
    FIntPoint Cell = Center + Directions[0] * Radius;
    for (int32 Side = 0; Side < DirectionCount; ++Side)
    {
        for (int32 Step = 0; Step < Radius; ++Step)
        {
            Out.Add(Cell);
            Cell += Direction(Side + 2);
        }
    }
    return Out;
}

TArray<FIntPoint> FHexGrid::Range(FIntPoint Center, int32 Radius)
{
    TArray<FIntPoint> Out;
    for (int32 Q = -Radius; Q <= Radius; ++Q)
    {
        for (int32 R = FMath::Max(-Radius, -Q - Radius); R <= FMath::Min(Radius, -Q + Radius); ++R) { Out.Add(Center + FIntPoint(Q, R)); }
    }
    return Out;
}

FIntPoint FHexGrid::Rotate(FIntPoint V, int32 Steps)
{
    Steps = ((Steps % DirectionCount) + DirectionCount) % DirectionCount;
    for (int32 Index = 0; Index < Steps; ++Index)
    {
        // Cube (q, r, s) -> (-s, -q, -r): E (1,0) -> NE (1,-1).
        V = FIntPoint(V.X + V.Y, -V.X);
    }
    return V;
}

int32 FHexGrid::NearestDirection(FIntPoint V)
{
    if (V == FIntPoint::ZeroValue) { return INDEX_NONE; }
    const FVector2D P = ToWorld(V, 1.0);
    int32 Best = 0;
    double BestDot = -TNumericLimits<double>::Max();
    for (int32 Index = 0; Index < DirectionCount; ++Index)
    {
        const FVector2D D = ToWorld(Directions[Index], 1.0).GetSafeNormal();
        const double Dot = FVector2D::DotProduct(P, D);
        if (Dot > BestDot + 1e-9) { BestDot = Dot; Best = Index; }
    }
    return Best;
}

FIntPoint FHexGrid::Round(double Q, double R)
{
    const double S = -Q - R;
    double RQ = FMath::RoundHalfFromZero(Q), RR = FMath::RoundHalfFromZero(R), RS = FMath::RoundHalfFromZero(S);
    const double DQ = FMath::Abs(RQ - Q), DR = FMath::Abs(RR - R), DS = FMath::Abs(RS - S);
    if (DQ > DR && DQ > DS) { RQ = -RR - RS; }
    else if (DR > DS) { RR = -RQ - RS; }
    return FIntPoint((int32)RQ, (int32)RR);
}

TArray<FIntPoint> FHexGrid::Line(FIntPoint A, FIntPoint B, int32 Bias)
{
    TArray<FIntPoint> Out;
    const int32 N = Distance(A, B);
    // Nudge off hex edges so ties resolve consistently to one side.
    const double E = Bias >= 0 ? 1e-6 : -1e-6;
    const double AQ = A.X + E, AR = A.Y + E, BQ = B.X + E, BR = B.Y + E;
    for (int32 Index = 0; Index <= N; ++Index)
    {
        const double T = N == 0 ? 0.0 : (double)Index / N;
        Out.Add(Round(AQ + (BQ - AQ) * T, AR + (BR - AR) * T));
    }
    return Out;
}

FVector2D FHexGrid::ToWorld(FIntPoint Cell, double Size)
{
    return FVector2D(Size * UE_DOUBLE_SQRT_3 * (Cell.X + Cell.Y * 0.5), Size * 1.5 * Cell.Y);
}

FIntPoint FHexGrid::FromWorld(FVector2D P, double Size)
{
    if (Size <= 0.0) { return FIntPoint::ZeroValue; }
    return Round((UE_DOUBLE_SQRT_3 / 3.0 * P.X - P.Y / 3.0) / Size, (2.0 / 3.0 * P.Y) / Size);
}

FIntPoint FHexGrid::FromOffset(int32 Column, int32 Row)
{
    return FIntPoint(Column - (Row - (Row & 1)) / 2, Row);
}

FIntPoint FHexGrid::ToOffset(FIntPoint Cell)
{
    return FIntPoint(Cell.X + (Cell.Y - (Cell.Y & 1)) / 2, Cell.Y);
}

bool FHexGrid::OnBoard(FIntPoint Cell, int32 Width, int32 Height)
{
    const FIntPoint O = ToOffset(Cell);
    return O.X >= 0 && O.Y >= 0 && O.X < Width && O.Y < Height;
}

TArray<FIntPoint> FHexGrid::BoardCells(int32 Width, int32 Height)
{
    TArray<FIntPoint> Out;
    for (int32 Row = 0; Row < Height; ++Row) { for (int32 Column = 0; Column < Width; ++Column) { Out.Add(FromOffset(Column, Row)); } }
    return Out;
}

TMap<FIntPoint, int32> FHexGrid::Reachable(FIntPoint Start, int32 Budget, TFunctionRef<int32(FIntPoint, FIntPoint)> StepCost)
{
    TMap<FIntPoint, int32> Best;
    if (Budget < 0) { return Best; }
    TArray<TArray<FIntPoint>> Buckets;
    Buckets.SetNum(1);
    Buckets[0].Add(Start);
    Best.Add(Start, 0);
    for (int32 Cost = 0; Cost < Buckets.Num(); ++Cost)
    {
        for (int32 Index = 0; Index < Buckets[Cost].Num(); ++Index)
        {
            const FIntPoint Cell = Buckets[Cost][Index];
            if (Best[Cell] != Cost) { continue; }
            for (const FIntPoint& D : Directions)
            {
                const FIntPoint Next = Cell + D;
                const int32 Step = StepCost(Cell, Next);
                if (Step <= 0 || Cost + Step > Budget) { continue; }
                if (const int32* Known = Best.Find(Next); Known && *Known <= Cost + Step) { continue; }
                Best.Add(Next, Cost + Step);
                if (Buckets.Num() <= Cost + Step) { Buckets.SetNum(Cost + Step + 1); }
                Buckets[Cost + Step].Add(Next);
            }
        }
    }
    return Best;
}
